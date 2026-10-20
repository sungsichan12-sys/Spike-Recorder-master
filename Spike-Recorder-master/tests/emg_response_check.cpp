#include "engine/RecordingManager.h"
#include "engine/SignalPowerAnalysis.h"
#include <windows.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#ifdef main
#undef main
#endif
using namespace BackyardBrains;
namespace BackyardBrains {
std::string getRecordingPath(){return "../../tests/core-artifacts";}
std::string getLoggingPath(){return "../../tests/core-artifacts/emg-response.log";}
std::string getConfigPath(){return "../../tests/core-artifacts/emg-response-config.xml";}
}
struct Metrics {double energy[2]={0,0},peak[2]={0,0};int windows=0;};
static Metrics collect(RecordingManager &m) {
    Metrics r;DWORD start=GetTickCount(),last=start;int64_t position=m.pos();
    while(GetTickCount()-start<8000) {
        Sleep(50);DWORD now=GetTickCount();m.advance((now-last)*m.sampleRate()/1000);last=now;
        if(!m.hidMode())throw std::runtime_error("HID device disconnected");
        if(m.pos()==position)continue;position=m.pos();
        int count=std::min<int64_t>(m.sampleRate()/20,m.pos());if(count<2)continue;
        for(int c=0;c<2 && c<int(m.virtualDevices().size());++c) {
            std::vector<int16_t> data(count);m.getData(c,m.pos()-count,count,data.data());
            const auto reading=SignalPower::measure(data.data(),data.size());
            r.energy[c]+=reading.rms*reading.rms;r.peak[c]=std::max(r.peak[c],reading.peak);
        }
        ++r.windows;
    }
    return r;
}
int main() {
    try {
        RecordingManager m;m.scanForHIDDevices();
        const auto board=m.isHIDBoardTypeAvailable(HID_BOARD_TYPE_MUSCLE)?HID_BOARD_TYPE_MUSCLE:HID_BOARD_TYPE_NEURON;
        if(!m.initHIDUSB(board)){std::cerr<<"FAIL "<<m.hidError<<std::endl;return 1;}
        for(int c=0;c<2 && c<int(m.virtualDevices().size());++c)m.bindVirtualDevice(c);
        m.setPaused(false);
        std::cout<<"READY rate="<<m.sampleRate()<<" channels="<<m.numberOfHIDChannels()<<" HP="<<m.highCornerFrequency()<<" LP="<<m.lowCornerFrequency()<<std::endl;
        Metrics phases[3];const char *names[]={"REST","CONTRACT","REST_AGAIN"};
        for(int p=0;p<3;++p){std::string go;if(!std::getline(std::cin,go))return 2;
            std::cout<<"PHASE "<<names[p]<<" 8 seconds"<<std::endl;phases[p]=collect(m);
            for(int c=0;c<2;++c)std::cout<<"MEASURE "<<names[p]<<" channel="<<c+1<<" rms="<<std::sqrt(phases[p].energy[c]/std::max(1,phases[p].windows))<<" peak="<<phases[p].peak[c]<<" windows="<<phases[p].windows<<std::endl;
            std::cout<<"READY_NEXT"<<std::endl;
        }
        for(int c=0;c<2;++c){double rest=phases[0].energy[c]/std::max(1,phases[0].windows),active=phases[1].energy[c]/std::max(1,phases[1].windows);
            std::cout<<"RESULT channel="<<c+1<<" contraction_rms_ratio="<<std::sqrt(active/std::max(1e-9,rest))<<std::endl;}
        m.disconnectFromHID();return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
