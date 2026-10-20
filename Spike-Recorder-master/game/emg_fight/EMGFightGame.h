#ifndef LOTTOL_EMG_FIGHT_GAME_H
#define LOTTOL_EMG_FIGHT_GAME_H
#include "games/GamePlugin.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace EMGFight {
const uint32_t Ink=0x101827ff,White=0xf3f5edff,Muted=0xa9b5c7ff,Blue=0x67cdffff,Red=0xff9875ff,Gold=0xffd666ff;
inline float clamp(float x,float a,float b) {return std::max(a,std::min(b,x));}
enum Phase {Intro,Countdown,Fighting,Paused,Finished};
enum Action {None,Attack,Strong,Guard};
struct Fighter {
    int hp,score,combo,hits,blocks,damage;
    double cooldown,strongReady,guardReady,guardUntil,landAt,actionAt,hitAt,lastHit;
    Action pending,animation;
    bool high,strongUsed,available;
    double tapUntil;
    float level,noise,threshold,strongThreshold;
    Fighter():hp(100),score(0),combo(0),hits(0),blocks(0),damage(0),cooldown(0),strongReady(0),guardReady(0),guardUntil(0),landAt(0),actionAt(-10),hitAt(-10),lastHit(-10),pending(None),animation(None),high(false),strongUsed(false),available(false),tapUntil(0),level(0),noise(0),threshold(100),strongThreshold(220) {}
};
struct Particle {float x,y,vx,vy,age,life;uint32_t color;};
struct State {
    Fighter p[2];
    Phase phase,beforePause;
    double now,visual,remaining,countdown,calibration,impactAt;
    int winner,lastSource;
    bool keys[128],startRequested,pauseRequested,resetRequested,hadSensor;
    Action commands[2];
    std::vector<Particle> particles;
    unsigned random;
    State():phase(Intro),beforePause(Fighting),now(0),visual(0),remaining(60),countdown(0),calibration(0),impactAt(-10),winner(-1),lastSource(0),startRequested(false),pauseRequested(false),resetRequested(false),hadSensor(false),random(18341) {
        std::fill(keys,keys+128,false);commands[0]=commands[1]=None;
    }
    float rnd() {random^=random<<13;random^=random>>17;random^=random<<5;return (random&65535)/65535.f;}
    void reset() {
        for(int i=0;i<2;++i) {const float noise=p[i].noise;p[i]=Fighter();p[i].noise=noise;commands[i]=None;}
        phase=Countdown;countdown=2.4;remaining=60;winner=-1;particles.clear();impactAt=-10;
        startRequested=pauseRequested=resetRequested=false;
    }
    bool action(int i,Action type) {
        Fighter &f=p[i];
        if(phase!=Fighting || f.hp<=0 || f.pending!=None) return false;
        if(type==Guard) {
            if(now<f.guardReady) return false;
            f.guardUntil=now+1.1;f.guardReady=now+1.5;f.animation=Guard;f.actionAt=now;
            return true;
        }
        if(now<f.cooldown || (type==Strong && now<f.strongReady)) return false;
        f.pending=type;f.animation=type;f.actionAt=now;f.guardUntil=0;
        f.landAt=now+(type==Strong?.38:.20);f.cooldown=now+(type==Strong?.7:.45);
        if(type==Strong) f.strongReady=now+4;
        return true;
    }
    void burst(int defender,bool blocked,bool strong) {
        const float x=defender==0?395.f:565.f;
        for(int n=0;n<(strong?34:20);++n) {
            const float a=rnd()*6.283185f,speed=70+rnd()*240;
            particles.push_back({x,283, std::cos(a)*speed,std::sin(a)*speed-80,0,.25f+rnd()*.5f,blocked?Blue:(n%3?Gold:White)});
        }
        if(particles.size()>150) particles.erase(particles.begin(),particles.begin()+static_cast<int>(particles.size()-150));
        impactAt=now;
    }
    void hit(int attacker,Action type) {
        Fighter &a=p[attacker],&d=p[1-attacker];
        const bool strong=type==Strong,blocked=d.guardUntil>now;
        const int damage=blocked?(strong?6:2):(strong?24:9);
        d.hp=std::max(0,d.hp-damage);a.damage+=damage;
        if(blocked) {++d.blocks;d.score+=300;a.combo=0;}
        else {a.combo=now-a.lastHit<1.8?a.combo+1:1;a.lastHit=now;++a.hits;a.score+=(strong?400:100)*a.combo;d.combo=0;d.hitAt=now;}
        burst(1-attacker,blocked,strong);
    }
    void finish() {
        if(phase!=Fighting) return;
        phase=Finished;
        winner=p[0].hp==p[1].hp?-1:(p[0].hp>p[1].hp?0:1);
        if(winner>=0) p[winner].score+=5000+(p[winner].hp==100?10000:0);
        for(int i=0;i<2;++i) {p[i].pending=None;p[i].tapUntil=0;p[i].guardUntil=0;commands[i]=None;}
    }
    void gesture(int i) {
        Fighter &f=p[i];
        if(!f.available || phase!=Fighting) {f.high=f.available&&f.level>=f.threshold;f.tapUntil=0;f.strongUsed=false;return;}
        if(f.level>=f.threshold && !f.high) {
            f.high=true;f.strongUsed=false;
            if(f.tapUntil>now) {f.tapUntil=0;f.strongUsed=true;action(i,Guard);}
            else f.tapUntil=now+.4;
        }
        if(f.high && !f.strongUsed && f.level>=f.strongThreshold) {
            f.tapUntil=0;f.strongUsed=true;action(i,Strong);
        }
        if(f.level<f.threshold*.65f) {f.high=false;f.strongUsed=false;}
        if(f.tapUntil>0 && now>=f.tapUntil) {f.tapUntil=0;action(i,Attack);}
    }
    void update(const BYBGameHost &h) {
        double dt=std::max(0.f,h.delta_seconds);
        visual+=std::min(dt,.1);
        const bool any=h.channel_signal_available[0] || h.channel_signal_available[1];
        const bool missing=(p[0].available && !h.channel_signal_available[0]) ||
                           (p[1].available && !h.channel_signal_available[1]);
        if((phase==Fighting || phase==Countdown) &&
           (dt>.5 || missing || (hadSensor && !any) || (hadSensor && lastSource!=h.signal_source))) {
            beforePause=phase;phase=Paused;
        }
        lastSource=h.signal_source;hadSensor=any;
        for(int i=0;i<2;++i) {
            Fighter &f=p[i];f.available=h.channel_signal_available[i]!=0;
            f.level=f.available?std::max(0.f,h.channel_signal_level[i]):0;
            if(phase==Intro && calibration<1 && f.available)
                f.noise+=(f.level-f.noise)*(1-std::exp(-std::min(dt,.1)*6));
            // The game hub's measured threshold is authoritative. Learning an
            // idle floor again here can mistake an early contraction for noise.
            f.threshold=h.channel_trigger_level[i]>0?std::max(12.f,h.channel_trigger_level[i]):std::max(30.f,f.noise*2.4f+15);
            f.strongThreshold=f.threshold*2.2f;
        }
        if(phase==Intro) calibration+=dt;
        if(pauseRequested) {
            if(phase==Fighting || phase==Countdown) {beforePause=phase;phase=Paused;}
            else if(phase==Paused) {phase=Countdown;countdown=1.2;}
            pauseRequested=false;
        }
        if(resetRequested) {reset();resetRequested=false;}
        if(startRequested) {
            if(phase==Intro && (!any || calibration>=1)) reset();
            else if(phase==Finished) reset();
            else if(phase==Paused) {phase=Countdown;countdown=1.2;}
            startRequested=false;
        }
        if(phase==Paused) {commands[0]=commands[1]=None;return;}
        if(dt>.5) dt=0;
        now+=dt;
        for(size_t i=0;i<particles.size();++i) {Particle &v=particles[i];v.age+=static_cast<float>(dt);v.x+=v.vx*dt;v.y+=v.vy*dt;v.vy+=500*dt;}
        particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Particle &v){return v.age>=v.life;}),particles.end());
        if(phase==Countdown) {countdown-=dt;if(countdown<=0) phase=Fighting;}
        for(int i=0;i<2;++i) {gesture(i);if(commands[i]!=None) {action(i,commands[i]);commands[i]=None;}}
        if(phase!=Fighting) return;
        remaining=std::max(0.,remaining-dt);
        Action landed[2]={None,None};
        for(int i=0;i<2;++i) if(p[i].pending!=None && now>=p[i].landAt) {landed[i]=p[i].pending;p[i].pending=None;}
        for(int i=0;i<2;++i) if(landed[i]!=None) hit(i,landed[i]);
        if(!p[0].hp || !p[1].hp || remaining<=0) finish();
    }
};

struct Point {float x,y;};
struct Canvas {
    const BYBGameHost &h;float scale,ox,oy;std::vector<float> xy;std::vector<uint32_t> colors;
    Canvas(const BYBGameHost &host):h(host),scale(std::min(host.width/960.f,host.height/640.f)),ox((host.width-960*scale)/2),oy((host.height-640*scale)/2) {}
    void triangle(Point a,Point b,Point c,uint32_t color) {const Point pts[]={a,b,c};for(int i=0;i<3;++i) {xy.push_back(ox+pts[i].x*scale);xy.push_back(oy+pts[i].y*scale);}colors.push_back(color);}
    void poly(const std::vector<Point> &p,uint32_t color) {for(size_t i=1;i+1<p.size();++i) triangle(p[0],p[i],p[i+1],color);}
    void rect(float x,float y,float w,float z,uint32_t col) {poly({{x,y},{x+w,y},{x+w,y+z},{x,y+z}},col);}
    void ellipse(float x,float y,float rx,float ry,uint32_t col,int n=24) {for(int i=0;i<n;++i) {float a=i*6.283185f/n,b=(i+1)*6.283185f/n;triangle({x,y},{x+std::cos(a)*rx,y+std::sin(a)*ry},{x+std::cos(b)*rx,y+std::sin(b)*ry},col);}}
    void line(Point a,Point b,float width,uint32_t col) {const float dx=b.x-a.x,dy=b.y-a.y,len=std::max(.01f,std::sqrt(dx*dx+dy*dy)),nx=-dy/len*width/2,ny=dx/len*width/2;poly({{a.x+nx,a.y+ny},{b.x+nx,b.y+ny},{b.x-nx,b.y-ny},{a.x-nx,a.y-ny}},col);}
    void flush() {if(!colors.empty() && h.draw_triangles) h.draw_triangles(h.draw_context,xy.data(),colors.data(),static_cast<int>(colors.size()));xy.clear();colors.clear();}
    void text(const std::string &s,float x,float y,uint32_t color=White,int align=0) {flush();if(h.draw_scaled_text)h.draw_scaled_text(h.draw_context,s.c_str(),ox+x*scale,oy+y*scale,scale,color,align);else if(h.draw_text)h.draw_text(h.draw_context,s.c_str(),static_cast<int>(ox+x*scale),static_cast<int>(oy+y*scale),color,align);}
};
struct Glyph {char c;unsigned r[7];};
inline void pixel(Canvas &c,const std::string &s,float cx,float y,float size,uint32_t color) {
    static const Glyph glyphs[]={
      {'A',{14,17,17,31,17,17,17}},{'D',{30,17,17,17,17,17,30}},{'E',{31,16,16,30,16,16,31}},
      {'F',{31,16,16,30,16,16,16}},{'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},
      {'I',{31,4,4,4,4,4,31}},{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
      {'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},{'O',{14,17,17,17,17,17,14}},
      {'P',{30,17,17,30,16,16,16}},{'R',{30,17,17,30,20,18,17}},{'S',{15,16,16,14,1,1,30}},
      {'T',{31,4,4,4,4,4,4}},{'U',{17,17,17,17,17,17,14}},{'W',{17,17,17,21,21,27,17}},
      {'Y',{17,17,10,4,4,4,4}},{'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},
      {'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},{'!',{4,4,4,4,4,0,4}},
      {'.',{0,0,0,0,0,0,4}}};
    const float left=cx-(s.size()*6-1)*size/2;
    for(size_t ch=0;ch<s.size();++ch) for(size_t g=0;g<sizeof(glyphs)/sizeof(glyphs[0]);++g) if(glyphs[g].c==s[ch])
        for(int row=0;row<7;++row) for(int col=0;col<5;++col) if(glyphs[g].r[row]&(1<<(4-col))) {
            c.rect(left+ch*6*size+col*size+2,y+row*size+3,size,size,Ink);
            c.rect(left+ch*6*size+col*size,y+row*size,size,size,color);
        }
}
inline std::string name(const BYBGameHost &h,int i) {
    const char *value=h.player_names[i];
    if(!value || !*value) return i==0?"플레이어 1":"플레이어 2";
    std::string s=value;size_t bytes=0,chars=0;
    while(bytes<s.size() && chars<12) {unsigned char b=s[bytes];bytes+=b<128?1:(b<224?2:(b<240?3:4));++chars;}
    return bytes<s.size()?s.substr(0,bytes)+"…":s;
}

struct Person {
    Canvas &c;float x,y,d,angle;
    Point point(float a,float b) const {a*=d;return {x+a*std::cos(angle)-b*std::sin(angle),y+a*std::sin(angle)+b*std::cos(angle)};}
    void line(float a,float b,float q,float r,float w,uint32_t color) {c.line(point(a,b),point(q,r),w+5,Ink);c.line(point(a,b),point(q,r),w,color);const Point joint=point(q,r);c.ellipse(joint.x,joint.y,w*.5f,w*.5f,color);}
    void ellipse(float a,float b,float w,float z,uint32_t color) {const Point p=point(a,b);c.ellipse(p.x,p.y,w,z,color);}
    void poly(std::vector<Point> p,uint32_t color) {for(size_t i=0;i<p.size();++i)p[i]=point(p[i].x,p[i].y);c.poly(p,color);}
};
inline void fighter(Canvas &c,const State &s,int i) {
    const Fighter &f=s.p[i];const float d=i==0?1.f:-1.f;const double age=s.now-f.actionAt;
    const bool attack=(f.animation==Attack || f.animation==Strong)&&age<.72;
    const bool strong=f.animation==Strong;
    const float wind=strong?.38f:.20f;
    const float extend=attack?clamp(static_cast<float>(age)/wind,0,1)*clamp(static_cast<float>(.72-age)/(.72f-wind),0,1):0;
    const float punch=attack?std::pow(std::sin(extend*1.570796f),2.f):0;
    const bool guarding=f.guardUntil>s.now;
    const float hurt=clamp(1-static_cast<float>(s.now-f.hitAt)/.34f,0,1);
    const float ko=f.hp==0?clamp(static_cast<float>(s.now-f.hitAt)/.8f,0,1):0;
    const float bob=f.hp>0?std::sin(s.visual*5+i*1.7f)*1.5f:0;
    const uint32_t dark=i==0?0x22366aff:0x77373cff,cloth=i==0?0x397bbcff:0xcc514dff,light=i==0?0x84d4e4ff:0xffa17aff;
    const uint32_t skin=i==0?0xf0bd93ff:0xcd966aff;
    c.ellipse(i==0?365:595,415,65+25*punch,10,0x070e1c90);
    Person p={c,(i==0?365.f:595.f)+d*(punch*50-hurt*16),410+bob,d,-d*ko*1.47f+d*hurt*.08f};
    p.line(-15,-81,-32,-43,24,dark);p.line(-32,-43,-49,-4,22,dark);
    p.poly({{-65,-12},{-30,-12},{-22,2},{-65,2}},Ink);
    p.line(17,-80,34,-42,25,cloth);p.line(34,-42,30,-6,23,cloth);
    p.poly({{15,-11},{48,-11},{57,2},{13,2}},Ink);p.line(17,-9,44,-9,4,light);
    p.line(-23,-143,-42,-117,20,dark);p.line(-42,-117,-9,-103,17,skin);
    p.ellipse(-6,-104,15,13,Ink);p.ellipse(-5,-105,12,10,light);
    p.poly({{-31,-159},{25,-160},{34,-122},{23,-78},{-23,-79},{-33,-123}},Ink);
    p.poly({{-27,-154},{21,-155},{29,-122},{19,-85},{-19,-86},{-28,-121}},cloth);
    p.poly({{-22,-151},{-6,-150},{11,-86},{-18,-86}},light);
    p.poly({{-6,-153},{13,-156},{8,-128}},skin);p.line(-24,-92,23,-92,8,Ink);p.line(7,-91,17,-60,7,White);
    p.line(2,-160,3,-178,17,skin);
    p.ellipse(4,-190,25,29,Ink);p.ellipse(6,-189,22,26,skin);
    p.poly({{-20,-195},{-21,-213},{-4,-221},{15,-219},{29,-207},{24,-196},{-3,-205},{-8,-184}},Ink);
    p.line(-18,-204,24,-204,6,i==0?Blue:Red);
    p.line(11,-193,22,-192,3,Ink);p.line(15,-187,22,-187,3,Ink);p.line(15,-174,23,-175,2,Ink);
    const float handX=guarding?38:(48+punch*83),handY=guarding?-185:(-147-punch*13);
    const float elbowX=guarding?38:39+punch*18,elbowY=guarding?-135:-113-punch*18;
    p.line(22,-145,elbowX,elbowY,21,cloth);p.line(elbowX,elbowY,handX,handY,19,skin);
    p.ellipse(handX,handY,17,15,Ink);p.ellipse(handX+1,handY-1,14,12,light);
    p.line(handX-6,handY+5,handX+8,handY+5,4,White);
    if(guarding) {const Point q=p.point(52,-155);c.ellipse(q.x,q.y,13,43,0x5ad4ff55);c.line({q.x+d*10,q.y-37},{q.x+d*10,q.y+35},3,Blue);}
    if(attack && strong && age<.45) {const Point q=p.point(handX,handY);c.ellipse(q.x,q.y,23,20,0xffd66655);c.line({q.x-d*65,q.y-9},{q.x-d*20,q.y-9},3,Gold);}
}

inline void stage(Canvas &c,const State &s) {
    c.rect(0,0,960,640,0x0e1525ff);c.rect(0,0,960,173,0x17223aff);
    for(int k=0;k<15;++k) {
        const float x=k*74.f-45,height=70+(k*47%90);
        c.rect(x,184-height,62,height,0x202e48ff);
        for(int row=0;row<3;++row)for(int col=0;col<3;++col)
            if((k+row*3+col)%3)c.rect(x+10+col*17,194-height+row*24,6,10,(k+col)%2?0x476078ff:0xb1844dff);
    }
    c.rect(0,184,960,178,0x182232ff);
    for(int i=0;i<34;++i) {float x=i*31.f+4,y=255+(i%4)*16+std::sin(s.visual*1.6+i)*2;const uint32_t coat=i%3==0?0x415775ff:0x30394fff;c.ellipse(x,y,8,9,0x997a70ff,12);c.rect(x-10,y+10,20,42,coat);}
    c.rect(30,216,145,53,0x2c414bff);c.text("LOTTOL",102,233,Blue,1);
    c.rect(785,216,145,53,0x4a333aff);c.text("EMG ARENA",857,233,Red,1);
    c.rect(0,362,960,84,0x34404bff);
    c.poly({{0,446},{960,446},{960,483},{0,483}},0x1a2431ff);
    for(int x=-160;x<1100;x+=105)c.line({480+(x-480)*.62f,362},{static_cast<float>(x),446},1,0x58616bff);
    c.line({0,390},{960,390},1,0x667077ff);c.line({0,423},{960,423},1,0x667077ff);
    c.rect(18,286,14,169,0x687b8fff);c.rect(928,286,14,169,0x687b8fff);
    for(int row=0;row<2;++row) {float y=316+row*35.f;c.line({20,y},{255,y+12},4,row?0xd9dcddff:0x3c88b3ff);c.line({705,y+12},{939,y},4,row?0xd9dcddff:0xb7615fff);}
}
inline void render(State &s,const BYBGameHost &h) {
    if(h.width<=0 || h.height<=0) return;
    if(h.fill_rect)h.fill_rect(h.draw_context,0,0,h.width,h.height,0x0b101aff);
    Canvas c(h);stage(c,s);fighter(c,s,0);fighter(c,s,1);
    for(size_t i=0;i<s.particles.size();++i) {const Particle &v=s.particles[i];c.rect(v.x,v.y,3+(1-v.age/v.life)*4,3,v.color);}
    if(s.now-s.impactAt<.16 && !s.particles.empty()) {
        float x=s.particles[0].x,y=285;for(int n=0;n<8;++n) {float a=n*.785398f;c.line({x+std::cos(a)*10,y+std::sin(a)*10},{x+std::cos(a)*35,y+std::sin(a)*35},3,Gold);}
    }
    c.rect(0,0,960,127,0x0d1421f5);
    for(int i=0;i<2;++i) {
        const Fighter &f=s.p[i];float x=i==0?32.f:558.f;
        c.text(std::string(i==0?"1P  ":"2P  ")+name(h,i),x,21,i==0?Blue:Red);
        c.rect(x,54,370,24,0x050c17ff);c.rect(x+3,57,364*f.hp/100.f,18,i==0?0x55b9d8ff:0xe17e65ff);
        char text[80];std::snprintf(text,sizeof(text),"%d / 100",f.hp);c.text(text,x+185,57,White,1);
        std::snprintf(text,sizeof(text),"점수 %06d",f.score);c.text(text,x,88,White);
        if(f.combo>=2) {std::snprintf(text,sizeof(text),"%d HITS",f.combo);c.text(text,x+367,88,Gold,2);}
    }
    c.text("ROUND 01",480,14,Muted,1);char timer[20];std::snprintf(timer,sizeof(timer),"%02d",static_cast<int>(std::ceil(s.remaining)));
    c.text(timer,480,49,s.remaining<10?Red:Gold,1);c.text("TIME",480,84,Muted,1);
    c.rect(0,483,960,157,0x111a29ff);
    for(int i=0;i<2;++i) {
        const Fighter &f=s.p[i];const float x=i==0?32.f:506.f,w=422;
        c.text(i==0?"1P · 채널 1":"2P · 채널 2",x,498,i==0?Blue:Red);
        c.text(f.available?(h.signal_source==2?"센서 연결됨":"오디오 입력"):"센서 없음 · 키보드 사용",x+w,498,Muted,2);
        const float range=std::max(200.f,f.strongThreshold*1.3f);
        c.rect(x,527,w,18,0x080f1aff);c.rect(x,527,w*clamp(f.level/range,0,1),18,i==0?Blue:Red);
        c.rect(x+w*f.threshold/range,523,2,26,Gold);c.rect(x+w*f.strongThreshold/range,523,2,26,Red);
        const double ready=clamp(static_cast<float>(1-(f.strongReady-s.now)/4),0,1);
        c.rect(x,554,w*ready,3,ready>=1?Gold:0x657289ff);
        c.text(i==0?"A 공격   S 강공격   D 방어":"J 공격   K 강공격   L 방어",x,570,Muted);
    }
    c.text("짧게 힘주기 = 공격   ·   크게 힘주기 = 강공격   ·   두 번 힘주기 = 방어",480,604,White,1);
    c.text("P 일시정지   /   [ ] 감도   /   G 게임 목록",480,621,Muted,1);
    if(s.phase==Intro || s.phase==Paused || s.phase==Finished) {
        c.rect(185,136,590,328,0x050b15ed);c.rect(187,138,586,324,0x172438ff);
        if(s.phase==Intro) {
            pixel(c,"EMG FIGHT",480,163,5,Gold);
            c.text("근육 대결",480,216,White,1);c.text(name(h,0)+"   VS   "+name(h,1),480,251,White,1);
            c.text("한 번 수축 = 공격 / 큰 수축 = 강공격 / 두 번 = 방어",480,288,Muted,1);
            c.text("각 플레이어는 자기 채널의 노란 선을 넘겨 조작하세요.",480,319,Muted,1);
            c.text("센서가 없으면 A S D / J K L로 바로 대전할 수 있어요.",480,348,Muted,1);
            c.rect(335,392,290,42,Gold);c.text((s.hadSensor&&s.calibration<1)?"힘을 빼고 잠시 기다리세요":"대전 시작 · Enter",480,403,Ink,1);
        } else if(s.phase==Paused) {
            pixel(c,"PAUSE",480,169,5,Gold);c.text("일시정지",480,225,White,1);
            c.text("입력 장치와 신호를 확인한 뒤 계속하세요.",480,280,Muted,1);
            c.text("센서가 빠지거나 화면이 오래 멈추면 자동으로 일시정지합니다.",480,319,Muted,1);
            c.rect(335,382,290,42,Gold);c.text("계속하기 · Enter / P",480,393,Ink,1);
        } else {
            pixel(c,s.winner<0?"DRAW":(s.winner==0?"1P WINS!":"2P WINS!"),480,164,5,s.winner==0?Blue:(s.winner==1?Red:Gold));
            c.text(s.winner<0?"무승부":name(h,s.winner)+" 승리!",480,221,White,1);
            for(int i=0;i<2;++i) {char stats[120];float x=i==0?335.f:625.f;c.text(name(h,i),x,264,i==0?Blue:Red,1);std::snprintf(stats,sizeof(stats),"점수 %d / 체력 %d",s.p[i].score,s.p[i].hp);c.text(stats,x,298,White,1);std::snprintf(stats,sizeof(stats),"명중 %d / 방어 %d",s.p[i].hits,s.p[i].blocks);c.text(stats,x,330,Muted,1);}
            c.rect(335,389,290,42,Gold);c.text("다시 대전 · Enter / R",480,400,Ink,1);
        }
    } else if(s.phase==Countdown) pixel(c,s.countdown>1.2?"READY":"FIGHT!",480,198,7,s.countdown>1.2?White:Gold);
    c.flush();
}
inline void *BYB_GAME_CALL create() {return new State();}
inline void BYB_GAME_CALL destroy(void *p) {delete static_cast<State *>(p);}
inline void BYB_GAME_CALL update(void *p,const BYBGameHost *h) {if(p&&h)static_cast<State *>(p)->update(*h);}
inline void BYB_GAME_CALL draw(void *p,const BYBGameHost *h) {if(p&&h)render(*static_cast<State *>(p),*h);}
inline void BYB_GAME_CALL key(void *p,int code,int pressed) {
    if(!p || code<0 || code>=128) return;
    State &s=*static_cast<State *>(p);const bool edge=pressed&&!s.keys[code];s.keys[code]=pressed!=0;
    if(!edge) return;
    if(code==13 || code==' ') s.startRequested=true;
    else if(code=='p') s.pauseRequested=true;
    else if(code=='r' && s.phase==Finished) s.resetRequested=true;
    else {const char *letters="asdjkl";for(int n=0;n<6;++n)if(code==letters[n])s.commands[n/3]=static_cast<Action>(1+n%3);}
}
inline void BYB_GAME_CALL mouse(void *p,int,int,int button) {if(p&&button==1) {State &s=*static_cast<State *>(p);if(s.phase!=Fighting)s.startRequested=true;}}
const BYBGamePlugin descriptor={BYB_GAME_PLUGIN_API_VERSION,"emg-fight","근육 대결 EMG","Two-player muscle fighting in the Recorder window.",create,destroy,update,draw,key,mouse};
}
extern "C" BYB_GAME_EXPORT const BYBGamePlugin *BYB_GAME_CALL BYB_GetGamePlugin(void) {return &EMGFight::descriptor;}
#endif
