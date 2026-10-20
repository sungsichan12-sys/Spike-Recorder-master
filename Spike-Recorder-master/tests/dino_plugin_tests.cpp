#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../game/dino/DinoGame.h"
#include <iostream>
#include <functional>
#include <stdexcept>
using namespace BackyardBrains;
static int passed=0,failed=0;
static void check(bool ok,const char *message) {if(!ok)throw std::runtime_error(message);}
static void test(const char *name,const std::function<void()> &f) {
    try {f();++passed;std::cout<<"PASS "<<name<<'\n';}
    catch(const std::exception &e) {++failed;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}
}
struct Capture {int triangles=0,labels=0,reports=0,practice=0,accepted=0;uint64_t run=0;int64_t score=0;bool depth=false;};
static void BYB_GAME_CALL rect(void *,int,int,int,int,uint32_t) {}
static void BYB_GAME_CALL text(void *p,const char *,int,int,uint32_t,int) {++static_cast<Capture*>(p)->labels;}
static void BYB_GAME_CALL mesh(void *p,const float *xy,const uint32_t *,const float *depth,int n) {
    check(n>0 && n<=BYB_GAME_MAX_TRIANGLES,"Triangle batch exceeds host limit");
    Capture &c=*static_cast<Capture*>(p);c.triangles+=n;
    for(int i=0;i<n*3;++i) {check(std::isfinite(xy[i*2]) && std::isfinite(xy[i*2+1]) && std::isfinite(depth[i]),"Invalid projected vertex");if(depth[i]>0)c.depth=true;}
}
static int BYB_GAME_CALL report(void *p,uint64_t run,int64_t score,int practice) {
    Capture &c=*static_cast<Capture*>(p);++c.reports;c.run=run;c.score=score;c.practice=practice;return c.accepted;
}
static BYBGameHost host(Capture &c) {
    BYBGameHost h={};h.width=1280;h.height=720;h.delta_seconds=.05f;h.trigger_level=100;h.input_sensitivity=1;
    h.draw_context=&c;h.fill_rect=rect;h.draw_text=text;h.draw_gradient_triangles=mesh;h.report_result=report;return h;
}
static void tap(DinoPlugin::State &s,int key) {s.key(key,true);s.key(key,false);}
int main() {
    test("Descriptor and module lifecycle",[] {
        const BYBGamePlugin *p=BYB_GetGamePlugin();check(p->api_version==6 && std::string(p->id)=="dino","Wrong ABI or manifest id");
        void *s=p->create();check(s!=NULL,"Create failed");p->destroy(s);
    });
    test("Keyboard holds and right click retain normal and long jumps",[] {
        DinoPlugin::State s;s.key(' ',true);check(s._run.height>=0 && s._run.velocity>0 && !s._run.longJump,"Normal jump failed");
        s._run.velocity=0;s.key(' ',true);check(s._run.velocity==0,"Held key repeated");
        s.key(' ',false);s._run.reset();s.mouse(100,100,4);check(s._run.longJump,"Native right-button value did not give long jump");
    });
    test("Developer stages, magic, laser and practice flag",[] {
        DinoPlugin::State s;tap(s,1073741883);tap(s,'3');tap(s,'m');check(s._run.practice && s._run.magicTime>0,"Storm magic failed");
        tap(s,'4');tap(s,'l');check(s._run.laserTime>0,"Citadel laser failed");
        tap(s,1073741883);check(s._run.practice,"Practice result became competitive");
        s._run.over=true;tap(s,' ');check(!s._run.practice && s._run.stage==Dino3D::Runner::Desert,"New competitive round failed");
    });
    test("Microphone peak chooses one strong action and rearms after release",[] {
        Capture c;auto h=host(c);h.signal_source=1;h.signal_available=1;DinoPlugin::State s;
        for(int i=0;i<16;++i)s.update(h);
        h.signal_level=220;for(int i=0;i<4;++i)s.update(h);
        check(s._run.longJump && s._peakInput.held,"Strong peak failed");
        const uint32_t jumpAt=s._lastJumpAt;for(int i=0;i<10;++i)s.update(h);check(s._lastJumpAt==jumpAt,"Held peak repeated");
        h.signal_level=0;s.update(h);check(!s._peakInput.held,"Peak did not rearm");
    });
    test("Disconnect discards a partially collected peak",[] {
        Capture c;auto h=host(c);h.signal_source=2;h.signal_available=1;DinoPlugin::State s;
        for(int i=0;i<16;++i)s.update(h);h.signal_level=300;s.update(h);check(s._peakInput.pending,"No pending onset");
        h.signal_available=0;h.signal_source=0;s.update(h);check(!s._peakInput.pending && !s._peakInput.held && s._sensorLevel==0,"Disconnected signal was retained");
    });
    test("Completed result retries, queues once and uses a new round id",[] {
        Capture c;auto h=host(c);DinoPlugin::State s;s._run.over=true;s._run.score=321;s.update(h);
        check(c.reports==1 && c.score==321 && !s._resultReported,"Initial queue failure lost score");
        for(int i=0;i<25;++i)s.update(h);check(c.reports==2,"Queue retry missing");
        c.accepted=1;for(int i=0;i<25;++i)s.update(h);check(s._resultReported,"Result not accepted");
        const int count=c.reports;const uint64_t first=c.run;for(int i=0;i<30;++i)s.update(h);check(c.reports==count,"Duplicate score");
        s.resetGame();s._run.over=true;s._run.practice=true;s.update(h);check(c.run>first && c.practice==1,"New round id or practice flag lost");
    });
    test("Four stages render finite depth meshes at two sizes without advancing",[] {
        Capture c;auto h=host(c);DinoPlugin::State s;
        const Dino3D::Runner::Stage stages[]={Dino3D::Runner::Desert,Dino3D::Runner::Sky,Dino3D::Runner::Storm,Dino3D::Runner::Citadel};
        for(int i=0;i<4;++i)for(int size=0;size<2;++size) {
            s._run.debugStage(stages[i]);h.width=size?640:1280;h.height=size?480:720;
            const float t=s._run.time,score=s._run.score;s.render(h);check(s._run.time==t && s._run.score==score,"Render mutated gameplay");
        }
        check(c.triangles>10000 && c.labels>100 && c.depth,"Models or UI were not drawn");
    });
    test("Installed DinoGame.dll loads and renders through the host ABI",[] {
        HMODULE lib=LoadLibraryW(L"win/build/game/dino/DinoGame.dll");check(lib!=NULL,"Installed DLL missing or dependent runtime missing");
        auto get=reinterpret_cast<BYBGetGamePluginFn>(GetProcAddress(lib,BYB_GAME_PLUGIN_ENTRY));check(get!=NULL,"Plugin export missing");
        const BYBGamePlugin *p=get();check(p && p->api_version==BYB_GAME_PLUGIN_API_VERSION && std::string(p->id)=="dino","Incompatible installed DLL");
        Capture c;auto h=host(c);void *s=p->create();check(s!=NULL,"DLL create failed");p->update(s,&h);p->render(s,&h);p->key_event(s,' ',1);p->key_event(s,' ',0);p->mouse_event(s,100,100,4);p->destroy(s);FreeLibrary(lib);
        check(c.triangles>1000,"DLL rendering missing");
    });
    std::cout<<"RESULT "<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
