#include "../game/emg_fight/EMGFightGame.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
using namespace EMGFight;
static int checks=0;
void check(bool good,const char *message) {++checks;if(!good) {std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
BYBGameHost host() {BYBGameHost h={};h.width=960;h.height=640;h.delta_seconds=.02f;h.channel_trigger_level[0]=h.channel_trigger_level[1]=100;h.player_names[0]="하늘";h.player_names[1]="바다";return h;}
void advance(State &s,BYBGameHost &h,int frames) {for(int i=0;i<frames;++i)s.update(h);}
State playing() {State s;s.phase=Fighting;return s;}
std::string escape(std::string s) {std::string out;for(size_t i=0;i<s.size();++i) {if(s[i]=='&')out+="&amp;";else if(s[i]=='<')out+="&lt;";else if(s[i]=='>')out+="&gt;";else if(s[i]=='\"')out+="&quot;";else out+=s[i];}return out;}
std::string color(uint32_t rgba) {std::ostringstream s;s<<'#'<<std::hex<<std::setw(6)<<std::setfill('0')<<(rgba>>8);return s.str();}
struct Trace {std::ostringstream svg;int triangles=0,texts=0;};
void BYB_GAME_CALL triangles(void *p,const float *xy,const uint32_t *colors,int n) {
    Trace &t=*static_cast<Trace *>(p);t.triangles+=n;
    for(int i=0;i<n;++i) {t.svg<<"<polygon fill='"<<color(colors[i])<<"' fill-opacity='"<<(colors[i]&255)/255.f<<"' points='";for(int v=0;v<3;++v) {const int k=i*6+v*2;if(!std::isfinite(xy[k])||!std::isfinite(xy[k+1])) {std::cerr<<"Invalid render coordinate\n";std::exit(1);}t.svg<<xy[k]<<','<<xy[k+1]<<' ';}t.svg<<"'/>\n";}
}
void BYB_GAME_CALL rect(void *p,int x,int y,int w,int h,uint32_t col) {Trace &t=*static_cast<Trace *>(p);t.svg<<"<rect x='"<<x<<"' y='"<<y<<"' width='"<<w<<"' height='"<<h<<"' fill='"<<color(col)<<"'/>\n";}
void BYB_GAME_CALL text(void *p,const char *value,float x,float y,float scale,uint32_t col,int alignment) {Trace &t=*static_cast<Trace *>(p);++t.texts;t.svg<<"<text x='"<<x<<"' y='"<<y+15*scale<<"' font-size='"<<16*scale<<"' fill='"<<color(col)<<"' font-family='Malgun Gothic,Arial,sans-serif' text-anchor='"<<(alignment==1?"middle":alignment==2?"end":"start")<<"'>"<<escape(value)<<"</text>\n";}
void preview(State &s,const char *file,int width=960,int height=640) {
    BYBGameHost h=host();h.width=width;h.height=height;Trace trace;h.draw_context=&trace;h.fill_rect=rect;h.draw_triangles=triangles;h.draw_scaled_text=text;
    const double clock=s.now;const int score=s.p[0].score;
    render(s,h);check(s.now==clock&&s.p[0].score==score,"render does not advance or score");check(trace.triangles>400 && trace.texts>10,"real scene and labels rendered");
    std::ofstream out(file);out<<"<svg xmlns='http://www.w3.org/2000/svg' width='"<<width<<"' height='"<<height<<"' viewBox='0 0 "<<width<<' '<<height<<"'>"<<trace.svg.str()<<"</svg>";
}
int main() {
    check(descriptor.api_version==6 && std::string(descriptor.id)=="emg-fight","native module descriptor");
    BYBGameHost h=host();
    {State s;s.startRequested=true;s.update(h);check(s.phase==Countdown,"intro starts countdown");advance(s,h,122);check(s.phase==Fighting&&s.remaining>59.8,"countdown reaches fight");}
    {State s=playing();check(s.action(0,Attack),"normal attack queued");check(s.p[1].hp==100,"windup does not hit early");advance(s,h,11);check(s.p[1].hp==91&&s.p[0].hits==1,"normal hit lands once");check(!s.action(0,Attack),"attack cooldown enforced");advance(s,h,20);check(s.action(0,Attack),"normal cooldown recovers");}
    {State s=playing();s.action(1,Strong);advance(s,h,20);check(s.p[0].hp==76,"strong attack damage");advance(s,h,30);check(!s.action(1,Strong),"strong cooldown enforced");advance(s,h,160);check(s.action(1,Strong),"strong cooldown recovers");}
    {State s=playing();s.action(1,Guard);s.action(0,Attack);advance(s,h,11);check(s.p[1].hp==98&&s.p[1].blocks==1&&s.p[1].score==300,"guard reduces damage and awards defense score");}
    {State s=playing();s.p[0].hp=s.p[1].hp=9;s.action(0,Attack);s.action(1,Attack);advance(s,h,11);check(s.phase==Finished&&s.winner==-1,"simultaneous KO is a draw");}
    {State s=playing();s.p[1].hp=9;s.action(0,Attack);advance(s,h,11);check(s.phase==Finished&&s.winner==0&&s.p[0].score==15100,"perfect win bonus applied once");advance(s,h,100);check(s.p[0].score==15100,"finished match cannot score again");}
    {State s=playing();s.remaining=.01;s.p[1].hp=70;s.update(h);check(s.phase==Finished&&s.winner==0,"time limit resolves by health");}
    {State s=playing();key(&s,'a',1);s.update(h);advance(s,h,30);key(&s,'a',1);advance(s,h,30);check(s.p[1].hp==91,"held key never repeats attacks");key(&s,'a',0);key(&s,'a',1);advance(s,h,12);check(s.p[1].hp==82,"release arms next key attack");key(&s,'j',1);advance(s,h,12);check(s.p[0].hp==91,"player two key is independent");}
    BYBGameHost sensor=host();sensor.signal_source=2;sensor.channel_signal_available[0]=sensor.channel_signal_available[1]=1;
    {State s;auto calibrated=sensor;calibrated.channel_trigger_level[0]=450;calibrated.channel_signal_level[0]=4000;advance(s,calibrated,30);check(s.p[0].threshold==450,"measured threshold is not overwritten by intro contraction");}
    {State s=playing();sensor.channel_signal_level[1]=120;s.update(sensor);sensor.channel_signal_level[1]=0;advance(s,sensor,34);check(s.p[0].hp==91&&s.p[1].hp==100,"channel two controls only player two");}
    {State s=playing();sensor.channel_signal_level[0]=300;s.update(sensor);sensor.channel_signal_level[0]=0;advance(s,sensor,21);check(s.p[1].hp==76,"strong muscle peak triggers strong attack");}
    {State s=playing();sensor.channel_signal_level[0]=120;s.update(sensor);sensor.channel_signal_level[0]=0;advance(s,sensor,4);sensor.channel_signal_level[0]=120;s.update(sensor);check(s.p[0].guardUntil>s.now&&s.p[0].tapUntil==0,"double contraction becomes guard");sensor.channel_signal_level[0]=0;advance(s,sensor,45);check(s.p[1].hp==100,"double contraction does not also attack");}
    {State s=playing();sensor.channel_signal_level[0]=120;advance(s,sensor,100);check(s.p[1].hp==91,"held muscle signal does not auto repeat");sensor.channel_signal_level[0]=0;}
    {State s=playing();s.update(sensor);sensor.channel_signal_available[1]=0;s.update(sensor);check(s.phase==Paused,"losing either active channel pauses match");sensor.channel_signal_available[1]=1;}
    {State s=playing();s.update(sensor);sensor.signal_source=1;s.update(sensor);check(s.phase==Paused,"input source change pauses match");sensor.signal_source=2;}
    {State s=playing();h.delta_seconds=2;s.update(h);check(s.phase==Paused&&s.remaining==60,"stalled frame pauses instead of playing unseen");h.delta_seconds=.02f;}
    {State s=playing();s.pauseRequested=true;s.update(h);const double time=s.remaining;advance(s,h,30);check(s.phase==Paused&&s.remaining==time,"manual pause freezes time");s.startRequested=true;s.update(h);check(s.phase==Countdown,"resume includes ready countdown");}
    State intro;preview(intro,"tests/emg-fight-intro.svg");
    State action=playing();action.now=3;action.visual=3;action.action(0,Strong);advance(action,h,15);action.p[1].guardUntil=5;preview(action,"tests/emg-fight-action.svg");preview(action,"tests/emg-fight-small.svg",640,480);
    action.p[1].hp=0;action.finish();preview(action,"tests/emg-fight-result.svg",1280,720);
    std::cout<<checks<<" native EMG fighting checks passed.\n";
}
