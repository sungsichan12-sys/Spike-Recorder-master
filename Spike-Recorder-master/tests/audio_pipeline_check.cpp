// Live engine diagnostic: statistics only, no window or saved audio.
#include "engine/RecordingManager.h"
#include "engine/SignalPowerAnalysis.h"
#include "GameSignal.h"
#include "AudioView.h"
#include <windows.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cmath>
#include <limits>
using namespace BackyardBrains;
namespace BackyardBrains {
std::string getRecordingPath() { return "../../tests/core-artifacts"; }
std::string getLoggingPath() { return "../../tests/core-artifacts/audio-engine.log"; }
std::string getConfigPath() { return "../../tests/core-artifacts/audio-config.xml"; }
}
#ifdef main
#undef main
#endif
struct InspectAudioView : AudioView {
    InspectAudioView(RecordingManager &m,AnalysisManager &a):AudioView(NULL,m,a) {}
    bool saneChannels() const {
        for(size_t i=0;i<_channels.size();++i)
            if(!std::isfinite(_channels[i].gain) || _channels[i].gain<.001f || _channels[i].gain>10 ||
               !std::isfinite(_channels[i].pos) || _channels[i].pos<=0 || _channels[i].pos>=1) return false;
        return !_channels.empty();
    }
};
int main() {
    RecordingManager manager;
    std::cout<<"MODE serial="<<manager.serialMode()<<" hid="<<manager.hidMode()<<" paused="<<manager.paused()
        <<" selected="<<manager.selectedVDevice()<<" rate="<<manager.sampleRate()<<"\n";
    std::cout<<"FILTERS hp="<<manager.highPassFilterEnabled()<<" lp="<<manager.lowPassFilterEnabled()
        <<" hpf="<<manager.highCornerFrequency()<<" lpf="<<manager.lowCornerFrequency()<<"\n";
    for(size_t i=0;i<manager.virtualDevices().size();++i) {
        const auto &d=manager.virtualDevices()[i];
        std::cout<<"CHANNEL "<<i<<" bound="<<d.bound<<" threshold="<<d.threshold<<"\n";
    }
    Sleep(30);
    const int64_t positionBeforeEmpty=manager.pos();
    manager.advance(0);
    if(manager.pos()!=positionBeforeEmpty) return 2;
    std::cout<<"PASS zero-frame audio update\n";
    const DWORD start=GetTickCount(); DWORD last=start;
    bool interrupted=false,reopened=false;
    int64_t previousPosition=0;
    double maxRms=0,maxPeak=0; int windows=0,above=0,maxEnvelopeSpan=0; float baseline=0;
    while(GetTickCount()-start<4400) {
        Sleep(20); DWORD now=GetTickCount();
        if(!interrupted && now-start>700) {
            // Invalidate only this diagnostic process's capture stream.
            BASS_RecordFree(); interrupted=true;
            std::cout<<"SIMULATED capture stream loss (no OS settings changed)\n";
        }
        manager.advance(static_cast<uint32_t>((now-last)*manager.sampleRate()/1000)); last=now;
        if(manager.pos()<previousPosition) reopened=true;
        previousPosition=manager.pos();
        int d=manager.selectedVDevice(),count=manager.sampleRate()/20;
        if(d<0 || manager.pos()<count) continue;
        std::vector<int16_t> samples(count);
        manager.getData(d,manager.pos()-count,count,samples.data());
        auto r=SignalPower::measure(samples.data(),samples.size());
        const auto envelope=manager.getSamplesEnvelope(d,manager.pos()-count,count,std::max(1,count/500));
        int low=32767,high=-32768;
        for(size_t i=0;i<envelope.size();++i) {low=std::min(low,int(envelope[i].first));high=std::max(high,int(envelope[i].second));}
        maxEnvelopeSpan=std::max(maxEnvelopeSpan,high-low);
        maxRms=std::max(maxRms,r.rms); maxPeak=std::max(maxPeak,r.peak);
        ++windows;
        if(windows<30) baseline=baseline*.75f+r.peak*.25f;
        if(r.peak>GameSignal::triggerLevel(100,baseline,1)) ++above;
    }
    std::cout<<"PIPELINE frames="<<manager.pos()<<" windows="<<windows<<" max_rms="<<maxRms
        <<" max_peak="<<maxPeak<<" baseline="<<baseline<<" above_threshold="<<above<<"\n";
    std::cout<<"RECOVERY interrupted="<<interrupted<<" reopened="<<reopened<<" frames_after_reopen="<<manager.pos()<<"\n";
    AnalysisManager analysis(manager);
    manager.saveGainForAudioInput(std::numeric_limits<float>::quiet_NaN());
    manager.saveTimeScaleForAudioInput(std::numeric_limits<float>::infinity());
    InspectAudioView view(manager,analysis);view.setGeometry(Widgets::Rect(0,0,1000,600));
    if(!view.saneChannels() || !std::isfinite(view.scaleWidth())) return 3;
    view.setOffset(-manager.sampleRate());view.updateChannels();
    if(view.offset()!=0) return 5;
    view.setOffset(-manager.sampleRate());view.resetLiveDisplay();
    const int screen=view.screenWidth(),count=view.sampleCount(screen,view.scaleWidth());
    if(view.offset()!=0 || !view.saneChannels() || count<manager.sampleRate()*1.9f || count>manager.sampleRate()*2.1f) return 4;
    std::cout<<"DISPLAY envelope_span="<<maxEnvelopeSpan<<" channels="<<view.channelCount()<<" visible_samples="<<count
             <<" offset="<<view.offset()<<" finite_settings=1 live_fit=1\n";
    return reopened && manager.pos()>manager.sampleRate()/4 && maxPeak>0 && maxEnvelopeSpan>0 ? 0 : 1;
}
