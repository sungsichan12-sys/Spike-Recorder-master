// Headless microphone diagnostic. Keeps only amplitude statistics, no recording.
#include "bass.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
int main() {
    BASS_DEVICEINFO info={}; int device=-1,first=-1;
    for(int i=0;BASS_RecordGetDeviceInfo(i,&info);++i) {
        std::printf("INPUT %d enabled=%d default=%d name=%s\n",i,
            !!(info.flags&BASS_DEVICE_ENABLED),!!(info.flags&BASS_DEVICE_DEFAULT),info.name?info.name:"");
        if(info.flags&BASS_DEVICE_ENABLED) {
            if(first<0) first=i;
            if(info.flags&BASS_DEVICE_DEFAULT) device=i;
        }
    }
    if(device<0) device=first;
    if(device<0 || !BASS_RecordInit(device)) {
        std::printf("FAIL RecordInit device=%d error=%d\n",device,BASS_ErrorGetCode()); return 1;
    }
    float volume=-1;
    const DWORD inputFlags=BASS_RecordGetInput(-1,&volume);
    std::printf("SELECTED %d input_flags=%lu input_volume=%.3f\n",device,inputFlags,volume);
    HRECORD stream=BASS_RecordStart(44100,2,0,NULL,NULL);
    if(!stream) {
        std::printf("STEREO FAILED error=%d; trying mono\n",BASS_ErrorGetCode());
        stream=BASS_RecordStart(44100,1,0,NULL,NULL);
    }
    if(!stream) { std::printf("FAIL RecordStart error=%d\n",BASS_ErrorGetCode()); BASS_RecordFree(); return 2; }
    BASS_CHANNELINFO channel={}; BASS_ChannelGetInfo(stream,&channel);
    std::printf("FORMAT rate=%lu channels=%lu\n",channel.freq,channel.chans);
    std::vector<short> data(4410*channel.chans);
    unsigned long frames=0,nonzero=0; double squares[2]={},sum[2]={}; int peak[2]={};
    const DWORD started=GetTickCount();
    while(GetTickCount()-started<2200) {
        Sleep(20);
        const DWORD bytes=BASS_ChannelGetData(stream,data.data(),data.size()*sizeof(short));
        if(bytes==static_cast<DWORD>(-1)) { std::printf("FAIL GetData error=%d\n",BASS_ErrorGetCode()); break; }
        const DWORD count=bytes/sizeof(short)/channel.chans;
        frames+=count;
        for(DWORD n=0;n<count;++n) for(DWORD c=0;c<channel.chans;++c) {
            const int value=data[n*channel.chans+c];
            sum[c]+=value; squares[c]+=double(value)*value;
            peak[c]=std::max(peak[c],std::abs(value)); if(value) ++nonzero;
        }
    }
    BASS_RecordFree();
    for(DWORD c=0;c<channel.chans;++c) {
        const double mean=frames?sum[c]/frames:0;
        std::printf("CHANNEL %lu frames=%lu peak=%d ac_rms=%.3f\n",c,frames,peak[c],
            frames?std::sqrt(std::max(0.0,squares[c]/frames-mean*mean)):0);
    }
    std::printf("RESULT %s nonzero_samples=%lu (audio samples were not saved)\n",frames&&nonzero?"SIGNAL":"NO_SIGNAL",nonzero);
    return frames&&nonzero?0:3;
}
