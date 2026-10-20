#define NIGHTPULSE_TEST
#include "../game/night-pulse/NightPulseGame.h"
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
static int passCount=0,failCount=0,trianglesSeen=0;
static std::ofstream *capture=NULL;
static void check(bool ok,const char *why) { if(!ok) throw std::runtime_error(why); }
static void test(const char *name,const std::function<void()> &f) {
    try { f(); ++passCount; std::cout<<"PASS "<<name<<"\n"; }
    catch(const std::exception &e) { ++failCount; std::cout<<"FAIL "<<name<<": "<<e.what()<<"\n"; }
}
static void BYB_GAME_CALL captureRect(void *,int x,int y,int w,int h,uint32_t c) {
    if(capture) *capture<<"R\t"<<x<<'\t'<<y<<'\t'<<w<<'\t'<<h<<'\t'<<c<<'\n';
}
static void BYB_GAME_CALL captureText(void *,const char *s,int x,int y,uint32_t c,int a) {
    if(capture) *capture<<"T\t"<<x<<'\t'<<y<<'\t'<<c<<'\t'<<a<<'\t'<<s<<'\n';
}
static void BYB_GAME_CALL captureTriangles(void *context,const float *xy,const uint32_t *c,int count) {
    const BYBGameHost *h=static_cast<BYBGameHost *>(context);
    check(count>500 && count<=BYB_GAME_MAX_TRIANGLES,"Invalid mesh batch size"); trianglesSeen+=count;
    for(int i=0;i<count;++i) {
        for(int k=0;k<6;++k) check(std::isfinite(xy[i*6+k]),"Nonfinite triangle");
        for(int v=0;v<3;++v) {
            check(xy[i*6+v*2]>=-.01f && xy[i*6+v*2]<=h->width+.01f,"Triangle escaped horizontal bounds");
            check(xy[i*6+v*2+1]>=91.99f && xy[i*6+v*2+1]<=h->height-87.99f,"Triangle escaped scene bounds");
        }
        if(capture) {
            *capture<<"P";
            for(int k=0;k<6;++k) *capture<<'\t'<<xy[i*6+k];
            *capture<<'\t'<<c[i]<<'\n';
        }
    }
}
static int gradientTriangles=0;
struct ResultCapture { int count=0; uint64_t run=0; int64_t score=-1; };
static int BYB_GAME_CALL captureResult(void *context,uint64_t run,int64_t score,int) {
    auto *result=static_cast<ResultCapture *>(context);
    ++result->count; result->run=run; result->score=score; return 1;
}
static void BYB_GAME_CALL captureGradient(void *context,const float *xy,const uint32_t *colors,const float *depth,int count) {
    std::ofstream *out=capture; capture=NULL;
    captureTriangles(context,xy,colors,count); capture=out; gradientTriangles+=count;
    check(depth!=NULL,"Missing depth buffer data");
    for(int i=0;i<count*3;++i) check(std::isfinite(depth[i]) && depth[i]>=0 && depth[i]<=3.334f,"Invalid projected depth");
    if(capture) for(int i=0;i<count;++i) {
        *capture<<"G";
        for(int k=0;k<6;++k) *capture<<'\t'<<xy[i*6+k];
        for(int k=0;k<3;++k) *capture<<'\t'<<colors[i*3+k];
        for(int k=0;k<3;++k) *capture<<'\t'<<depth[i*3+k];
        *capture<<'\n';
    }
}
static BYBGameHost host() {
    BYBGameHost h={}; h.width=1200; h.height=760; h.delta_seconds=.01f;
    h.fill_rect=captureRect; h.draw_text=captureText; h.draw_triangles=captureTriangles;
    h.draw_gradient_triangles=captureGradient;
    h.trigger_level=100; h.signal_source=1; h.signal_available=1; h.signal_level=45;
    return h;
}
static void exportFrame(State &s,const char *name,int width=1200,int height=760) {
    BYBGameHost h=host(); h.width=width; h.height=height; h.draw_context=&h;
    std::ofstream out(name); capture=&out; out<<"S\t"<<width<<'\t'<<height<<'\n';
    renderGame(&s,&h); capture=NULL;
}
int main() {
    test("Entire chart can be cleared perfectly",[] {
        Rhythm r; r.reset(); const auto notes=r.notes;
        for(const auto &n:notes) r.update(n.time,true);
        r.update(EndTime,false);
        check(r.finished && !r.failed && r.misses==0 && r.perfect==56 && r.score==5600,"Perfect run not clearable");
    });
    test("Judgement windows",[] {
        Rhythm a,b,c; a.reset(); b.reset(); c.reset();
        a.update(2.08,true); b.update(2.14,true); c.update(2.20,true);
        check(a.perfect==1 && b.great==1 && c.good==1,"Hit boundaries incorrect");
    });
    test("Outside hit window and 12 misses fail",[] {
        Rhythm r; r.reset(); r.update(2.21,false);
        check(r.misses==1,"Late miss not counted"); r.update(10,false);
        check(r.finished && r.failed && r.misses==12,"Miss limit not enforced");
    });
    test("Single note cannot be scored twice",[] {
        Rhythm r; r.reset(); r.update(2,true); r.update(2.01,true);
        check(r.score==100 && r.combo==1,"Repeated input scored twice");
    });
    test("Offbeat input breaks combo",[] {
        Rhythm r; r.reset(); r.update(2,true); r.update(2.25,true);
        check(r.combo==0 && r.misses==1,"Offbeat spam is free");
        check(r.strokes==2 && r.strikeTime>0,"Mistimed stroke has no physical feedback");
    });
    test("Frame stall does not slow song time",[] {
        State s; BYBGameHost h=host(); updateGame(&s,&h); h.signal_edge=1; updateGame(&s,&h);
        h.signal_edge=0; h.delta_seconds=3; updateGame(&s,&h);
        check(s.rhythm.elapsed==3 && s.rhythm.misses==2,"Clock capped or not aligned");
    });
    test("Microphone starts muted",[] {
        State s; BYBGameHost h=host(); updateGame(&s,&h);
        check(!s.musicEnabled,"Microphone can auto-trigger on accompaniment");
    });
    test("Held key does not auto-hit",[] {
        State s; BYBGameHost h=host(); keyEvent(&s,' ',1); updateGame(&s,&h);
        for(int i=0;i<300;++i) { keyEvent(&s,' ',1); updateGame(&s,&h); }
        check(s.rhythm.score==0,"Key repeat hit notes");
        keyEvent(&s,' ',0); keyEvent(&s,' ',1); check(s.pendingTap,"Released key cannot trigger again");
    });
    test("Sensor edge starts and scores",[] {
        State s; BYBGameHost h=host(); h.signal_edge=1; updateGame(&s,&h);
        h.delta_seconds=2; updateGame(&s,&h);
        check(s.rhythm.perfect==1 && s.rhythm.strikeTime>0,"Signal not connected to gameplay");
    });
    test("Retry resets notes and score",[] {
        State s; BYBGameHost h=host(); h.signal_edge=1; updateGame(&s,&h);
        h.signal_edge=0; h.delta_seconds=20; updateGame(&s,&h); check(s.rhythm.failed,"Did not fail");
        h.delta_seconds=1; h.signal_edge=1; updateGame(&s,&h);
        check(!s.rhythm.finished && s.rhythm.elapsed==0 && s.rhythm.misses==0,"Retry not clean");
    });
    test("Render callbacks at small and large sizes",[] {
        State s; s.rhythm.reset(); s.rhythm.update(2,true);
        for(const auto &size:std::vector<std::pair<int,int>>{{640,480},{1200,760},{1800,1000}}) {
            BYBGameHost h=host(); h.width=size.first; h.height=size.second; h.draw_context=&h;
            renderGame(&s,&h);
        }
        check(trianglesSeen>1500,"3D geometry not drawn");
    });
    test("Rendering is independent of scoring",[] {
        State s; s.rhythm.reset(); s.rhythm.update(2,true);
        BYBGameHost h=host(); h.draw_context=&h;
        renderGame(&s,&h); renderGame(&s,&h);
        check(s.rhythm.score==100 && s.rhythm.elapsed==2 && s.rhythm.strikeTime==.26f,"Render mutates state");
    });
    test("Notes choose the matching instrument",[] {
        Rhythm r; r.reset();
        for(int i=0;i<8;++i) r.update(r.notes[i].time,true);
        check(r.hitNote==7 && noteTarget(r.hitNote).instrument==2,"Ride note does not target ride");
        check(noteTarget(3).instrument==1 && noteTarget(0).instrument==0,"Kit mapping changed");
    });
    test("Gradient renderer and flat callback fallback both work",[] {
        State s; BYBGameHost h=host(); h.draw_context=&h;
        const int before=gradientTriangles; renderGame(&s,&h);
        check(gradientTriangles>before,"Smooth drawing callback unused");
        h.draw_gradient_triangles=NULL; const int flat=trianglesSeen; renderGame(&s,&h);
        check(trianglesSeen>flat,"Flat fallback no longer draws");
    });
    test("Rigid sticks contact, rebound and settle without stretching",[] {
        Rhythm r; r.reset(); r.update(2,true);
        const Vec3 contact=stickPose(r,0).tip;
        r.update(2.09,false);
        check(stickPose(r,0).tip.y>contact.y+.65f,"No elastic rebound");
        for(int i=0;i<=60;++i) {
            Rhythm pose=r; pose.elapsed=2+i*.01;
            for(int hand=0;hand<2;++hand) {
                const StickPose p=stickPose(pose,hand); const Vec3 length=p.tip-p.butt;
                check(std::fabs(std::sqrt(dot(length,length))-1.77f)<.0001f,"Stick stretches during stroke");
            }
        }
        r.update(2.43,false);
        check(std::fabs(stickPose(r,0).tip.y-1.83f)<.001f,"Recovery failed");
    });
    test("Alternating strokes retain the other hand and cymbal vibration",[] {
        Rhythm r; r.reset();
        for(int i=0;i<8;++i) r.update(r.notes[i].time,true);
        const double ride=r.elapsed; const double right=r.stickAt[1];
        r.update(ride+.13,true); // An offbeat left-hand tap must not cancel the ride.
        check(r.stickAt[1]==right && r.stickAt[0]>right,"Other hand was reset");
        check(r.instrumentAt[2]==ride && std::fabs(cymbalAngle(r))>.01f,"Cymbal vibration lost");
        r.reset();
        check(cymbalAngle(r)==0 && membraneDip(r,0)==0,"Retry retains vibration");
    });
    test("Preparatory movement never strikes or awards points",[] {
        Rhythm r; r.reset(); r.update(1.98,false);
        check(stickPose(r,0).tip.y>1.65f && r.strokes==0 && r.score==0,"Anticipation triggers a hit");
        check(membraneDip(r,0)==0 && dot(cameraImpulse(r),cameraImpulse(r))==0,"No-input impact");
    });
    test("Motion is time based, bounded and settles after the result",[] {
        Rhythm a,b; a.reset(); b.reset(); a.update(2,true); b.update(2,true);
        for(int i=1;i<=6;++i) a.update(2+i*.01,false);
        b.update(2.06,false);
        check(std::fabs(stickPose(a,0).tip.y-stickPose(b,0).tip.y)<.0001f,"Frame rate changes motion");
        for(int i=0;i<150;++i) {
            a.elapsed=2+i*.01;
            check(dot(cameraImpulse(a),cameraImpulse(a))<.003f,"Camera kick too large");
            check(membraneDip(a,0)>-.033f,"Skin penetrates shell cap");
        }
        State s; s.rhythm.reset(); s.rhythm.update(2,true); s.rhythm.finished=true;
        BYBGameHost h=host(); h.delta_seconds=1; updateGame(&s,&h);
        Rhythm pose=s.rhythm; pose.elapsed+=s.resultAge;
        check(std::fabs(stickPose(pose,0).tip.y-1.83f)<.001f && membraneDip(pose,0)==0,"Result freezes a strike");
    });
    test("Every instrument animation stays clipped through the full stroke",[] {
        State s; s.rhythm.reset();
        for(int note=0;note<8;++note) {
            s.rhythm.update(s.rhythm.notes[note].time,true);
            if(note!=0 && note!=3 && note!=7) continue;
            const Rhythm original=s.rhythm;
            for(float age: {0.f,.04f,.09f,.18f,.34f,.6f,1.6f}) {
                s.rhythm=original; s.rhythm.elapsed+=age;
                BYBGameHost h=host(); h.width=640; h.height=480; h.draw_context=&h;
                renderGame(&s,&h);
            }
            s.rhythm=original;
        }
    });
    test("Same-hand recovery flows into the next preparation",[] {
        Rhythm r; r.reset();
        for(int i=0;i<=3;++i) r.update(r.notes[i].time,true);
        const double hit=r.elapsed;
        r.elapsed=hit+.4199; const Vec3 before=stickPose(r,0).tip;
        r.elapsed=hit+.4201; const Vec3 after=stickPose(r,0).tip;
        check(dot(after-before,after-before)<.00001f,"Preparation snaps at recovery boundary");
    });
    test("Fast rebound pivots at the wrist instead of translating the stick",[] {
        Rhythm r; r.reset(); r.update(2,true);
        r.elapsed=2.02; const StickPose low=stickPose(r,0);
        r.elapsed=2.08; const StickPose high=stickPose(r,0);
        check(dot(high.grip-low.grip,high.grip-low.grip)<.000001f,"Rebound moves the wrist");
        check(high.tip.y>low.tip.y+.35f,"No visible rotation about the wrist");
    });
    test("Completed rounds report once and retries use a new run ID",[] {
        State s; BYBGameHost h=host(); ResultCapture result;
        h.draw_context=&result; h.report_result=captureResult;
        h.signal_edge=1; updateGame(&s,&h);
        h.signal_edge=0; h.delta_seconds=20; updateGame(&s,&h);
        check(s.rhythm.finished && result.count==1 && result.run==1,"First result was not reported");
        updateGame(&s,&h); updateGame(&s,&h);
        check(result.count==1,"Finished run was reported again");
        h.signal_edge=1; updateGame(&s,&h);
        h.signal_edge=0; updateGame(&s,&h);
        check(result.count==2 && result.run==2,"Retry did not receive a new result ID");
    });
    State s; exportFrame(s,"tests/night-pulse-intro.tsv");
    s.rhythm.reset(); s.rhythm.update(2,true); exportFrame(s,"tests/night-pulse-play.tsv");
    exportFrame(s,"tests/night-pulse-small.tsv",640,480);
    s.rhythm.update(2.065,false); exportFrame(s,"tests/night-pulse-rebound.tsv");
    s.rhythm.reset();
    for(int i=0;i<8;++i) s.rhythm.update(s.rhythm.notes[i].time,true);
    s.rhythm.update(s.rhythm.elapsed+.05,false); exportFrame(s,"tests/night-pulse-ride.tsv");
    std::cout<<"RESULT "<<passCount<<" passed, "<<failCount<<" failed\n";
    return failCount?1:0;
}
