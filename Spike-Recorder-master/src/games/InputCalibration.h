#ifndef LOTTOL_INPUT_CALIBRATION_H
#define LOTTOL_INPUT_CALIBRATION_H
#include <algorithm>
#include <cmath>
#include <stdint.h>
#include <string>
#include <vector>
namespace BackyardBrains { namespace GameInput {
struct Reading {float level=0;bool edge=false;};
struct Profile {
    bool valid=false,armed=false,pending=false,held=false,delivering=false;
    float rest=0,effort=0,fraction=.25f;
    uint32_t onset=0,released=0,lastEdge=0;
    float threshold(float sensitivity=1) const {
        const float safe=std::isfinite(sensitivity)?std::max(.25f,std::min(4.f,sensitivity)):1;
        return std::max(12.f,(effort-rest)*fraction/safe);
    }
    void resetGate() {armed=pending=held=delivering=false;onset=released=lastEdge=0;}
    Reading sample(float raw,uint32_t now,bool available,float sensitivity=1) {
        Reading out;
        if(!valid || !available || !std::isfinite(raw)) {resetGate();return out;}
        const float level=std::max(0.f,raw-rest),trigger=threshold(sensitivity);
        if(level<trigger*.65f) {
            pending=false;
            if(!released)released=now?now:1;
            if(now-released>=100)held=false;
            if(now-released>=400)armed=true;
        } else released=0;
        if(!armed)return out; // A held contraction must be released after setup.
        if(level>=trigger) {
            if(!pending && !held){pending=true;onset=now;}
            if(pending && now-onset>=40 && (!lastEdge || now-lastEdge>=200)) {
                pending=false;held=delivering=true;lastEdge=now;out.edge=true;
            }
            // Reject isolated 50 ms noise spikes for level-driven plugins too.
            if(held && delivering)out.level=level;
        } else {pending=false;delivering=false;out.level=level;}
        return out;
    }
};
struct Calibration {
    enum Stage {Closed,RestReady,Rest,EffortReady,Effort,Review,Error};
    Stage stage=Closed;
    Profile profiles[2];
    int player=0,players=1;
    uint32_t started=0,quietSince=0;
    bool quiet=false;
    std::string error;
    std::vector<float> samples;
    float restMedian=0;
    bool open() const{return stage!=Closed;}
    bool measuring() const{return stage==Rest || stage==Effort;}
    void begin(int count) {
        players=std::max(1,std::min(2,count));player=0;stage=RestReady;
        profiles[0]=Profile();profiles[1]=Profile();samples.clear();error.clear();quiet=false;quietSince=0;
    }
    void start(uint32_t now) {
        if(stage==RestReady || stage==Error) {
            profiles[player]=Profile();stage=Rest;
        } else if(stage==EffortReady)stage=Effort;
        else return;
        started=now;samples.clear();error.clear();quiet=false;quietSince=0;
    }
    void retry(){stage=RestReady;profiles[player]=Profile();samples.clear();error.clear();quiet=false;quietSince=0;}
    float progress(uint32_t now) const{return measuring()?std::min(1.f,float(now-started)/3000.f):0;}
    static float percentile(std::vector<float> data,float quantile) {
        if(data.empty())return 0;
        std::sort(data.begin(),data.end());return data[std::min(data.size()-1,size_t(quantile*(data.size()-1)))];
    }
    void fail(const char *message){error=message;stage=Error;samples.clear();quiet=false;quietSince=0;}
    void feed(float raw,uint32_t now,bool available) {
        if(!open())return;
        if(!available || !std::isfinite(raw) || raw<0) {
            quiet=false;quietSince=0;
            if(measuring() || stage==Review)fail("신호가 끊겼습니다. 연결 후 다시 측정하세요.");
            return;
        }
        if(stage==Review) {
            if(raw<profiles[player].rest+profiles[player].threshold()*.65f) {
                if(!quietSince)quietSince=now?now:1;
                quiet=now-quietSince>=500;
            } else {quiet=false;quietSince=0;}
            return;
        }
        if(!measuring())return;
        samples.push_back(raw);
        if(now-started<3000)return;
        if(samples.size()<40){fail("측정 데이터가 부족합니다. 다시 측정하세요.");return;}
        if(stage==Rest) {
            restMedian=percentile(samples,.5f);
            const float high=percentile(samples,.98f);
            profiles[player].rest=high+std::max(12.f,std::max(restMedian*.2f,high-restMedian));
            stage=EffortReady;samples.clear();
        } else {
            const float effort=percentile(samples,.65f);
            if(effort-profiles[player].rest<std::max(30.f,restMedian*.75f)) {
                fail("휴식과 힘주기의 차이가 작습니다. 전극·채널을 확인하고 다시 측정하세요.");return;
            }
            profiles[player].effort=effort;profiles[player].valid=true;
            profiles[player].resetGate();stage=Review;samples.clear();quiet=false;quietSince=0;
        }
    }
    void adjust(float delta) {
        if(stage==Review)profiles[player].fraction=std::max(.15f,std::min(.55f,profiles[player].fraction+delta));
    }
    // The game remains uncreated until every player confirms their own profile.
    bool confirm() {
        if(stage!=Review || !profiles[player].valid || !quiet)return false;
        profiles[player].resetGate();
        profiles[player].armed=true; // Review already required a quiet release.
        if(++player<players){stage=RestReady;quiet=false;quietSince=0;return false;}
        stage=Closed;return true;
    }
};
} }
#endif
