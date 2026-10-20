// Exercises the original recording/analysis engine without creating a window.
#include "engine/RecordingManager.h"
#include "engine/FileRecorder.h"
#include "engine/FileReadUtil.h"
#include "engine/SampleBuffer.h"
#include "engine/FFTBackend.h"
#include "engine/SpikeAnalysis.h"
#include "engine/SpikeSorter.h"
#include "engine/HighPassFilter.h"
#include "engine/LowPassFilter.h"
#include "engine/NotchFilter.h"
#include "engine/SignalPowerAnalysis.h"
#include "engine/HIDUsbManager.h"
#include "widgets/Application.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <cstring>
#ifdef main
#undef main
#endif
using namespace BackyardBrains;
static std::string artifacts;
// Isolate config, logs and recordings from the user's documents and settings.
namespace BackyardBrains {
std::string getRecordingPath() { return artifacts; }
std::string getLoggingPath() { return artifacts+"/engine.log"; }
std::string getConfigPath() { return artifacts+"/config.xml"; }
}
static void require(bool condition,const char *message) { if(!condition) throw std::runtime_error(message); }
static void put16(std::ofstream &f,unsigned v) { f.put(v&255); f.put((v>>8)&255); }
static void put32(std::ofstream &f,uint32_t v) { put16(f,v&65535); put16(f,v>>16); }
static std::string wav(const char *name,int bits,int channels,int rate,const std::vector<double> &data,bool floating=false) {
    std::string path=artifacts+"/"+name;
    std::ofstream f(path,std::ios::binary);
    unsigned bytes=data.size()*(bits/8);
    f.write("RIFF",4); put32(f,36+bytes); f.write("WAVEfmt ",8); put32(f,16);
    put16(f,floating?3:1); put16(f,channels); put32(f,rate);
    put32(f,rate*channels*bits/8); put16(f,channels*bits/8); put16(f,bits);
    f.write("data",4); put32(f,bytes);
    for(double v:data) {
        if(floating) { float value=v; f.write(reinterpret_cast<char *>(&value),4); }
        else if(bits==8) f.put(static_cast<int>(v));
        else if(bits==16) put16(f,static_cast<int16_t>(v));
        else if(bits==24) { int32_t value=static_cast<int32_t>(v); f.put(value&255); f.put((value>>8)&255); f.put((value>>16)&255); }
    }
    if(bytes&1) { f.put(0); f.seekp(4); put32(f,37+bytes); }
    return path;
}
static std::vector<std::vector<int16_t>> decode(const std::string &path,int count,int expectedChannels,int expectedRate) {
    HSTREAM stream=0; int channels=0,rate=0,bytes=0;
    require(openAnyFile(path.c_str(),stream,channels,rate,bytes),"WAV open failed");
    bool info=channels==expectedChannels && rate==expectedRate && anyFilesLength(stream,bytes,channels)==count;
    if(!info) std::cout<<"  decoded info: channels="<<channels<<", rate="<<rate<<", bytes="<<bytes<<", byteLength="<<BASS_ChannelGetLength(stream,BASS_POS_BYTE)<<", frames="<<anyFilesLength(stream,bytes,channels)<<" (expected "<<count<<")\n";
    std::vector<std::vector<int16_t>> result;
    bool read=readAnyFile(result,count*channels*bytes,stream,channels,bytes);
    BASS_StreamFree(stream);
    require(info,"WAV frame count / format mismatch");
    require(read && result.size()==static_cast<size_t>(channels) && result[0].size()==static_cast<size_t>(count),"WAV decoded frame count mismatch");
    return result;
}
static int passed=0,failed=0;
struct HidFixture : HIDUsbManager {
    void seed(int count,int channels,int tail,int available) {
        delete[] mainCircularBuffer;
        maxSamples=count; _numberOfChannels=channels;
        mainCircularBuffer=new int32_t[count];
        for(int i=0;i<count;++i) mainCircularBuffer[i]=i;
        mainTail=tail; mainHead=(tail+available)%count;
    }
};
struct ResetObserver : sigslot::has_slots<> {
    int count=0;
    void reset() { ++count; }
};
struct OwnedWidget : Widgets::Widget {
    int &destroyed;
    OwnedWidget(int &n):destroyed(n) {}
    ~OwnedWidget() { ++destroyed; }
};
static void test(const char *name,const std::function<void()> &body) {
    try { body(); std::cout<<"PASS "<<name<<std::endl; ++passed; }
    catch(const std::exception &e) { std::cout<<"FAIL "<<name<<": "<<e.what()<<std::endl; ++failed; }
}
int main(int argc,char **argv) {
    if(argc!=2) return 2;
    artifacts=argv[1];
    test("Widget and application ownership without a visible window",[] {
        delete new Widgets::Widget;
        SDL_setenv("SDL_VIDEODRIVER","dummy",1);
        int destroyed=0;
        for(int i=0;i<2;++i) {
            { Widgets::Application app; app.addWindow(new OwnedWidget(destroyed)); app.addPopup(new OwnedWidget(destroyed)); }
            require(Widgets::Application::getInstance()==NULL,"Application singleton survives destruction");
        }
        require(destroyed==4,"Window or popup ownership leaked/double-deleted");
    });
    test("HID read respects caller frame capacity",[] {
        HidFixture hid; hid.seed(80000,2,0,79998);
        std::vector<int32_t> out(60002,-123456);
        require(hid.readDevice(out.data()+1,30000)==30000,"Read did not respect max frames");
        require(out.front()==-123456 && out.back()==-123456 && out[60000]==59999,"HID read overflowed the output");
        require(hid.readDevice(out.data()+1,30000)==9999 && out[1]==60000,"Remaining frames were lost");
        require(hid.readDevice(out.data(),1)==0,"Empty HID buffer returned stale data");
    });
    test("HID wrap and incomplete stereo frame",[] {
        HidFixture hid; int32_t out[8]={};
        require(hid.readDevice(out,4)==0,"Unopened HID read succeeded");
        hid.seed(12,2,10,7);
        require(hid.readDevice(out,3)==3 && out[0]==10 && out[1]==11 && out[2]==0 && out[5]==3,"Wrapped HID frame order incorrect");
        require(hid.readDevice(out,1)==0,"Incomplete frame was consumed");
        require(hid.readDevice(NULL,1)==0 && hid.readDevice(out,0)==0,"Invalid caller buffer accepted");
    });
    require(BASS_Init(0,44100,0,NULL,NULL)!=FALSE,"BASS offline init failed");
    test("PCM16 stereo decoding",[] {
        auto p=wav("pcm16.wav",16,2,8000,{0,30000,1234,-2345,-32768,32767,320,-640});
        auto d=decode(p,4,2,8000);
        require(d[0]==std::vector<int16_t>({0,1234,-32768,320}) && d[1]==std::vector<int16_t>({30000,-2345,32767,-640}),"PCM16 sample values changed");
    });
    test("PCM8 decoding",[] {
        auto p=wav("pcm8.wav",8,1,8000,{0,64,128,192,255,128});
        auto d=decode(p,6,1,8000);
        std::cout<<"  PCM8 endpoints: "<<d[0][0]<<", "<<d[0][4]<<"\n";
        require(d[0][0]==-32768 && d[0][2]==0 && d[0][4]>=32510,"8-bit amplitude / decoding is incorrect");
    });
    test("PCM24 decoding",[] {
        auto path=wav("pcm24.wav",24,1,8000,{0,4194304,-4194304,8388607,-8388608,0});
        auto d=decode(path,6,1,8000);
        require(std::abs(d[0][1]-16383)<=2 && std::abs(d[0][2]+16383)<=2,"24-bit normalized amplitude is incorrect");
    });
    test("Float WAV clipping and invalid samples",[] {
        auto d=decode(wav("float.wav",32,1,8000,{0,.5,-.5,2,-2,std::numeric_limits<double>::quiet_NaN()},true),6,1,8000);
        std::cout<<"  float samples: "; for(auto v:d[0]) std::cout<<v<<", "; std::cout<<"\n";
        require(std::abs(d[0][1]-16383)<=1 && d[0][3]==32767 && d[0][4]==-32768 && d[0][5]==0,"Float conversion / NaN handling is incorrect");
    });
    test("Missing / unsupported file rejection",[] {
        HSTREAM h=42; int c=1,r=1,b=1;
        require(!openAnyFile("missing.wav",h,c,r,b) && h==0,"Missing WAV was accepted");
        require(!openAnyFile("x.mp3",h,c,r,b) && h==0,"Unsupported extension was accepted");
    });
    test("Raw sample buffer and envelopes",[] {
        SampleBuffer b; int16_t values[]={-20,10,300,-70,2,400,-800,4}; b.addData(values,8);
        require(b.getData(0,8)==std::vector<int16_t>(values,values+8),"Raw buffer mismatch");
        auto e=b.getDataEnvelope(0,8,2);
        require(e.size()==4 && e[0]==std::make_pair<int16_t,int16_t>(-20,10) && e[3]==std::make_pair<int16_t,int16_t>(-800,4),"Envelope min/max mismatch");
    });
    test("Filter impulse timing",[] {
        LowPassFilter f; f.initWithSamplingRate(8000); f.setQ(.707); f.setCornerFrequency(100);
        double d[]={1,0,0,0,0,0}; f.filterContiguousData(d,6);
        require(d[0]>0 && std::isfinite(d[0]),"Filter inserts an unexpected two-sample delay");
    });
    test("Partial envelope and invalid stride",[] {
        SampleBuffer b; int16_t values[]={-20,10,300,-70,2,400,-800}; b.addData(values,7);
        auto envelope=b.getDataEnvelope(0,7,3);
        require(envelope.size()==3 && envelope.back()==std::make_pair<int16_t,int16_t>(-800,-800),"Partial envelope lost samples or exceeded the output buffer");
        require(b.getDataEnvelope(0,7,0).empty(),"Zero stride was not rejected");
    });
    test("Filter chunk continuity",[] {
        LowPassFilter a,b; a.initWithSamplingRate(8000); b.initWithSamplingRate(8000);
        a.setQ(.707); b.setQ(.707); a.setCornerFrequency(100); b.setCornerFrequency(100);
        std::vector<double> x(1000); for(int i=0;i<1000;++i) x[i]=std::sin(i*.02);
        auto split=x; a.filterContiguousData(x.data(),1000);
        for(int offset=0;offset<1000;offset+=10) b.filterContiguousData(split.data()+offset,10);
        for(int i=0;i<1000;++i) require(std::fabs(x[i]-split[i])<1e-10,"Block boundaries changed filtered samples");
    });
    test("Unconfigured filters and integer clipping",[] {
        FilterBase identity; double d[]={1,-2,3}; identity.filterContiguousData(d,3);
        require(d[0]==1 && d[1]==-2 && d[2]==3,"Unconfigured filter suppresses samples");
        struct Gain : FilterBase { Gain() { b0=2; b1=b2=a1=a2=0; setCoefficients(); } } gain;
        int16_t clipped[]={22000,-22000}; gain.filterIntData(clipped,2);
        require(clipped[0]==32767 && clipped[1]==-32768,"Filter output wraps instead of clipping");
    });
    test("High-pass removes DC",[] {
        HighPassFilter f; f.initWithSamplingRate(8000); f.setQ(.707); f.setCornerFrequency(100);
        std::vector<double> d(8000,1); f.filterContiguousData(d.data(),d.size());
        require(std::fabs(d.back())<1e-6,"High-pass DC rejection failed");
    });
    test("50 Hz notch response",[] {
        NotchFilter f; f.initWithSamplingRate(8000); f.setQ(10); f.setCenterFrequency(50);
        std::vector<double> d(32000); for(size_t i=0;i<d.size();++i) d[i]=std::sin(2*3.141592653589793*50*i/8000);
        f.filterContiguousData(d.data(),d.size()); double energy=0;
        for(size_t i=24000;i<d.size();++i) energy+=d[i]*d[i];
        require(std::sqrt(energy/8000)<.001,"50 Hz notch suppression failed");
    });
    test("FFT known frequency",[] {
        std::vector<std::complex<float>> d(1024);
        for(int i=0;i<1024;++i) d[i]=std::sin(2*3.141592653589793*32*i/1024);
        FFTBackend::transform(d);
        require(std::abs(std::abs(d[32])-512)<.01 && std::abs(d[16])<.01,"FFT known-bin magnitude mismatch");
    });
    test("Spike intervals and correlations",[] {
        std::vector<int> h; std::vector<double> bins={.001,.01,.1,1};
        SpikeAnalysis::isi(h,{0,20,220,2220},bins,1000);
        require(h==std::vector<int>({0,1,1,1}),"Inter-spike intervals mismatch");
        SpikeAnalysis::crossCorrelation(h,{100},{100},20,4);
        require(std::accumulate(h.begin(),h.end(),0)==1,"Correlation count mismatch");
    });
    test("Event markers including final line",[] {
        std::string file=artifacts+"/markers.txt"; std::ofstream f(file); f<<"# events\n1, 0.25\n2, 1.5"; f.close();
        std::list<std::pair<std::string,int64_t>> markers;
        FileRecorder::parseMarkerTextFile(markers,file,8000);
        require(markers.size()==2 && markers.front().second==2000 && markers.back().second==12000,"Marker timestamps changed");
    });
    test("Spike detection at known pulse positions",[] {
        std::vector<double> pulses(12000,0); pulses[2000]=25000; pulses[6000]=28000;
        SpikeSorter sorter; sorter.findAllSpikes(wav("spikes.wav",16,1,8000,pulses),80);
        const auto &spikes=sorter.spikes(0);
        require(spikes.size()==2 && spikes[0].first==2000 && spikes[1].first==6000,"Spike positions / count differ from the source waveform");
    });
    BASS_Free();
    {
        RecordingManager manager;
    test("Player stop, restart and invalid stream lifecycle",[] {
        Player p; p.stop(); p.stop();
        require(p.pos(0,0)==0 && p.stateOfBuffer()==0 && p.sampleRate()==0 && p.volume()==0,"Closed player returned invalid state");
        for(int i=0;i<3;++i) {
            p.start(8000); require(p.sampleRate()==8000,"Player restart failed");
            p.setVolume(150); require(p.volume()==100,"Volume clamp failed");
            p.stop(); require(p.sampleRate()==0 && p.stateOfBuffer()==0,"Stopped player kept stale handle");
        }
        p.start(0); p.push(NULL,10); p.setPos(10,0,0);
        require(p.pos(2,1)==0,"Invalid stream position overflowed");
    });

        std::vector<double> values(32768*2);
        for(int i=0;i<32768;++i) { values[2*i]=22000; values[2*i+1]=12000*std::sin(2*3.141592653589793*25*i/8000); }
        std::string input=wav("integration.wav",16,2,8000,values);
        test("File changes and seeks publish buffer resets",[&] {
            ResetObserver observer; manager.bufferReset.connect(&observer,&ResetObserver::reset);
            require(manager.loadFile(input.c_str()),"Reset fixture load failed");
            require(observer.count>0,"File replacement did not invalidate cached signal data");
            const int before=observer.count;
            manager.setPos(1000); require(observer.count==before+1,"Seek did not invalidate calibrated activity");
            manager.setPos(1100,false); require(observer.count==before+1,"Continuous playback invalidated calibration");
        });
        test("File load, preload and seek",[&] {
            require(manager.loadFile(input.c_str()),"RecordingManager WAV loading failed");
            manager.player().setVolume(0); manager.advance(0); manager.setPos(1000);
            int16_t d[4]; manager.getData(0,100,4,d);
            require(manager.fileLength()==32768 && d[0]==22000 && manager.pos()==1000,"File preloading or seeking failed");
        });
        test("WAV recording, metadata and reopening",[&] {
            manager.setPos(0); FileRecorder recorder(manager);
            require(recorder.start(artifacts+"/recorded.wav"),"Recorder could not start");
            manager.setPos(1000); recorder.advance();
            MetadataChunk meta; manager.constructMetadata(&meta); meta.timeScale=.25f; meta.markers.push_back({"1",100});
            recorder.stop(&meta);
            auto d=decode(artifacts+"/recorded.wav",1000,2,8000);
            require(d[0][999]==22000 && d[1][100]==static_cast<int16_t>(values[201]),"Recording changed PCM samples");
            HSTREAM h=0; int c,r,b; require(openAnyFile((artifacts+"/recorded.wav").c_str(),h,c,r,b),"Recorded WAV reopen failed");
            const char *tags=readEventsAndSpikesForAnyFile(h); MetadataChunk parsed;
            bool good=tags && !FileRecorder::parseMetadataStr(&parsed,tags,manager) && parsed.channels.size()==2 && std::fabs(parsed.timeScale-.25f)<1e-6;
            BASS_StreamFree(h); require(good,"Recording metadata round-trip failed");
        });
        test("Trigger waveform average at large amplitudes",[&] {
            manager.setThreshAvgCount(4); manager.clearTriggers(); manager.addTrigger(200); manager.addTrigger(1000);
            int16_t d[16]={}; manager.getTriggerData(0,16,d);
            require(d[8]==22000,"Trigger averaging overflows 16-bit samples");
            int16_t odd[15]={}; manager.getTriggerData(0,15,odd);
            require(odd[14]==22000,"Odd-sized trigger average leaves a sample uninitialized");
        });
        test("FFT display cache stays finite",[&] {
            FFTBackend fft(manager); fft.setDevice(1); fft.request(4096,4096);
            const float *d=fft.fftcache()[fft.offset()%FFTBackend::FFTTRES]; int best=0;
            for(int i=0;i<FFTBackend::FFTFRES;++i) { require(std::isfinite(d[i]) && d[i]>=0,"FFT display contains corrupted magnitudes"); if(d[i]>d[best]) best=i; }
            std::cout<<"  FFT display peak bin: "<<best<<"\n";
            require(std::abs(best-7)<=1,"FFT display dominant frequency is wrong");
        });
        test("FFT display amplitude linearity",[&] {
            FFTBackend first(manager); first.setDevice(1); first.request(4096,4096);
            float original[FFTBackend::FFTFRES];
            std::copy(first.fftcache()[first.offset()%FFTBackend::FFTTRES],first.fftcache()[first.offset()%FFTBackend::FFTTRES]+FFTBackend::FFTFRES,original);
            auto half=values; for(double &value:half) value=static_cast<int16_t>(value)/2;
            require(manager.loadFile(wav("half-amplitude.wav",16,2,8000,half).c_str()),"Half amplitude file load failed");
            manager.player().setVolume(0); manager.advance(0);
            FFTBackend second(manager); second.setDevice(1); second.request(4096,4096);
            const float *d=second.fftcache()[second.offset()%FFTBackend::FFTTRES];
            int best=std::max_element(original,original+FFTBackend::FFTFRES)-original;
            std::cout<<"  FFT amplitude ratio: "<<original[best]/d[best]<<"\n";
            require(std::isfinite(d[best]) && std::fabs(original[best]/d[best]-2)<.005,"FFT display magnitude is not linear in input amplitude");
        });
        test("Low-rate WAV spectrum remains in bounds",[&] {
            std::vector<double> signal(16384); for(int i=0;i<16384;++i) signal[i]=1000*std::sin(2*3.141592653589793*10*i/100);
            require(manager.loadFile(wav("low-rate.wav",16,1,100,signal).c_str()),"Low-rate WAV rejected");
            manager.player().setVolume(0); manager.advance(0);
            FFTBackend fft(manager); fft.request(4096,4096);
            const float *d=fft.fftcache()[fft.offset()%FFTBackend::FFTTRES];
            for(int i=0;i<FFTBackend::FFTFRES;++i) require(std::isfinite(d[i]) && d[i]>=0,"Low-rate FFT corrupted memory");
            for(int i=0;i<8;++i) require(d[i]==0,"FFT reports energy outside the sampled frequency range");
        });
        test("Relative power meter using decoded file samples",[&] {
            std::vector<double> signal(6000);
            for(int i=0;i<6000;++i) { int amplitude=i<2000?10:i<5000?100:50; signal[i]=(i%2?1:-1)*amplitude; }
            require(manager.loadFile(wav("power-levels.wav",16,1,1000,signal).c_str()),"Power fixture load failed");
            manager.player().setVolume(0); manager.advance(0);
            SignalPower::RelativeActivity activity; activity.start(SignalPower::RelativeActivity::Rest);
            for(int end=50;end<=5000;end+=50) {
                if(end==2050) require(activity.start(SignalPower::RelativeActivity::Reference),"Rest state not passed to reference calibration");
                const int count=std::min(end,250); std::vector<int16_t> samples(count);
                manager.getData(0,end-count,count,samples.data());
                activity.ingest(SignalPower::measure(samples.data(),samples.size()),.05);
            }
            int16_t samples[250]; manager.getData(0,5250,250,samples);
            const double expected=100*std::sqrt(2400.0/9900.0);
            require(activity.ready() && std::fabs(activity.percent(SignalPower::measure(samples,250))-expected)<1e-6,"Decoded samples do not produce the expected relative activity");
        });
    }
    std::cout<<"RESULT "<<passed<<" passed, "<<failed<<" failed"<<std::endl;
    return failed?1:0;
}
