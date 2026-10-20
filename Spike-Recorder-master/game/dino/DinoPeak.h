#ifndef BYB_DINO_PEAK_H
#define BYB_DINO_PEAK_H
#include <algorithm>
#include <cmath>
#include <stdint.h>
namespace BackyardBrains { namespace Dino3D {
// Collect a 100 ms onset before selecting ONE action. A rising magic peak must
// not spend itself on the flap/fire thresholds it crosses on the way up.
struct PeakInput {
    bool held,pending;
    float peak;
    uint32_t started;
    PeakInput():held(false),pending(false),peak(0),started(0) {}
    void reset() { held=pending=false; peak=0; }
    int sample(float ratio,uint32_t now,bool magic,bool laser=false) {
        if(!std::isfinite(ratio)) { reset(); return 0; }
        if(!held && ratio>=1) { held=pending=true; peak=ratio; started=now; }
        if(pending) {
            peak=std::max(peak,ratio);
            if(now-started>=100 || ratio<.75f) {
                pending=false;
                const int action=laser && peak>=3.4f?4:(magic && peak>=2.6f?3:(peak>=1.8f?2:1));
                if(ratio<.75f) held=false;
                return action;
            }
        }
        if(ratio<.75f) held=false;
        return 0;
    }
};
} }
#endif
