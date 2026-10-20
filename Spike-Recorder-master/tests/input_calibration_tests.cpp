#include "games/InputCalibration.h"
#include <iostream>
#include <functional>
#include <stdexcept>
#include <limits>
using namespace BackyardBrains::GameInput;
static int passed=0,failed=0;
static void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
static void test(const char *name,const std::function<void()> &f){
    try{f();++passed;std::cout<<"PASS "<<name<<'\n';}
    catch(const std::exception &e){++failed;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}
}
static uint32_t measure(Calibration &c,uint32_t now,float base) {
    c.start(now);
    for(int i=0;i<=60;++i)c.feed(base+float(i%7)*3,now+i*50,true);
    return now+3050;
}
static uint32_t prepare(Calibration &c,int players=1) {
    c.begin(players);uint32_t now=measure(c,100,200);
    require(c.stage==Calibration::EffortReady,"Rest did not finish");
    now=measure(c,now,2200);require(c.stage==Calibration::Review,"Effort did not finish");return now;
}
static void release(Calibration &c,uint32_t now){for(int i=0;i<=12;++i)c.feed(210,now+i*50,true);}
static Profile profile(){Calibration c;const auto now=prepare(c);release(c,now);require(c.confirm(),"Confirmation failed");return c.profiles[0];}
int main(){
    test("No game before explicit rest and effort measurements",[]{Calibration c;c.begin(1);c.feed(5000,100,true);require(c.stage==Calibration::RestReady && !c.confirm(),"Spontaneous launch");});
    test("Two measurements produce a threshold above all resting noise",[]{Calibration c;prepare(c);require(c.profiles[0].valid && c.profiles[0].rest>218 && c.profiles[0].threshold()>12,"Unsafe threshold");});
    test("Held effort cannot confirm or leak into game start",[]{Calibration c;auto now=prepare(c);for(int i=0;i<30;++i)c.feed(2200,now+i*50,true);require(!c.quiet && !c.confirm() && c.open(),"Held contraction started game");release(c,now+1600);require(c.confirm(),"Quiet release did not unlock start");});
    test("Rest noise produces zero game edges and levels",[]{auto p=profile();for(uint32_t now=1000;now<21000;now+=50){auto r=p.sample(200+(now/50)%21,now,true);require(!r.edge && r.level==0,"Resting noise triggered input");}});
    test("One isolated 50 ms impulse is ignored",[]{auto p=profile();p.sample(210,1000,true);auto r=p.sample(5000,1050,true);require(!r.edge && r.level==0,"First spike leaked");r=p.sample(210,1100,true);require(!r.edge && r.level==0,"Transient produced edge");});
    test("A sustained contraction triggers once and retains strength",[]{auto p=profile();require(!p.sample(2200,1000,true).edge,"Immediate edge");auto r=p.sample(2200,1050,true);require(r.edge && r.level>p.threshold()*3,"Strength mapping lost");for(int i=0;i<30;++i)require(!p.sample(2200,1100+i*50,true).edge,"Held contraction repeated");});
    test("Borderline alternating noise cannot accumulate onset time",[]{auto p=profile();for(uint32_t now=1000;now<5000;now+=50){const float raw=p.rest+p.threshold()*((now/50)%2?1.05f:.95f);require(!p.sample(raw,now,true).edge,"Nonconsecutive onset triggered");}});
    test("Release hysteresis rearms the next deliberate contraction",[]{auto p=profile();p.sample(2200,1000,true);require(p.sample(2200,1050,true).edge,"Missing first edge");p.sample(210,1100,true);p.sample(210,1250,true);p.sample(2200,1300,true);require(p.sample(2200,1350,true).edge,"Next contraction did not rearm");});
    test("A partial release cannot retrigger level-driven games",[]{auto p=profile();p.sample(2200,1000,true);p.sample(2200,1050,true);p.sample(p.rest+p.threshold()*.7f,1100,true);for(uint32_t now=1150;now<2500;now+=50){const auto r=p.sample(2200,now,true);require(!r.edge && r.level==0,"Partial-release bounce leaked another peak");}});
    test("Disconnected input requires a quiet release after return",[]{auto p=profile();p.sample(2200,1000,false);for(uint32_t now=1050;now<3000;now+=50)require(!p.sample(2200,now,true).edge,"Reconnect fired held input");for(uint32_t now=3000;now<3550;now+=50)p.sample(210,now,true);p.sample(2200,3600,true);require(p.sample(2200,3650,true).edge,"Reconnect did not recover");});
    test("Weak effort cannot calibrate background noise as a contraction",[]{Calibration c;c.begin(1);auto now=measure(c,100,200);measure(c,now,205);require(c.stage==Calibration::Error && !c.profiles[0].valid,"Noise calibrated as effort");});
    test("A single effort-phase cable spike cannot pass weak calibration",[]{Calibration c;c.begin(1);auto now=measure(c,100,200);c.start(now);for(int i=0;i<=60;++i)c.feed(i==30?8000:210,now+i*50,true);require(c.stage==Calibration::Error,"Transient passed effort measurement");});
    test("Insufficient data and signal loss reject measurement",[]{Calibration c;c.begin(1);c.start(100);c.feed(100,100,true);c.feed(100,3100,true);require(c.stage==Calibration::Error,"Sparse measurement accepted");c.retry();c.start(4000);c.feed(0,4100,false);require(c.stage==Calibration::Error,"Disconnected measurement continued");});
    test("NaN input never enters a profile or emits a game event",[]{Calibration c;c.begin(1);c.start(100);c.feed(std::numeric_limits<float>::quiet_NaN(),150,true);require(c.stage==Calibration::Error,"NaN measurement accepted");auto p=profile();auto r=p.sample(std::numeric_limits<float>::infinity(),1000,true);require(!r.edge && r.level==0,"Nonfinite game signal leaked");});
    test("Threshold adjustment is bounded and keeps a fixed noise floor",[]{Calibration c;prepare(c);const float rest=c.profiles[0].rest;c.adjust(10);require(c.profiles[0].fraction==.55f,"Upper bound lost");c.adjust(-10);require(c.profiles[0].fraction==.15f && c.profiles[0].rest==rest,"Noise floor changed with sensitivity");});
    test("Two players require separate completed measurements",[]{Calibration c;auto now=prepare(c,2);release(c,now);require(!c.confirm() && c.player==1 && c.stage==Calibration::RestReady,"Second player skipped");now=measure(c,now+1000,400);now=measure(c,now,4000);release(c,now);require(c.confirm() && !c.open() && c.profiles[0].effort!=c.profiles[1].effort,"Independent profiles lost");});
    test("Retry clears stale profile and previous release confirmation",[]{Calibration c;auto now=prepare(c);release(c,now);c.retry();require(!c.profiles[0].valid && !c.quiet && c.stage==Calibration::RestReady,"Retry kept old calibration");});
    test("Comfortable measured effort reaches stronger dinosaur skill tiers",[]{auto p=profile();require((p.effort-p.rest)/p.threshold()>3.4f,"Highest skill threshold exceeds measured effort");});
    std::cout<<"RESULT "<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
