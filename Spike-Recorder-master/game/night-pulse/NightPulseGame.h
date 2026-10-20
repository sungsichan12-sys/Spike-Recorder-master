#ifndef NIGHT_PULSE_GAME_H
#define NIGHT_PULSE_GAME_H
#include "../../src/games/GamePlugin.h"
#include "Stage.h"
#include <cstdio>
#include <string>
#if defined(_WIN32) && !defined(NIGHTPULSE_TEST)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "bass.h"
#endif

namespace {
using namespace NightPulse;
struct State {
    Rhythm rhythm;
    bool pendingTap,keyHeld,musicEnabled,inputConfigured,audioWarning;
    double fallbackClock,lastAudioClock;
    float staleAudio,resultWait,visualTime,resultAge,nextReportAt;
    uint64_t roundId;
    bool resultReported;
    unsigned long stream;
    unsigned long hitSamples[3];
    State():pendingTap(false),keyHeld(false),musicEnabled(true),inputConfigured(false),
        audioWarning(false),fallbackClock(0),lastAudioClock(0),staleAudio(0),
        resultWait(0),visualTime(0),resultAge(0),nextReportAt(0),roundId(0),resultReported(false),stream(0) { hitSamples[0]=hitSamples[1]=hitSamples[2]=0; }
};
uint32_t color(unsigned r,unsigned g,unsigned b,unsigned a=255) { return (r<<24)|(g<<16)|(b<<8)|a; }
const uint32_t Ink=color(235,232,220),Muted=color(154,159,157),Accent=color(146,191,179),Amber=color(216,184,125);
void rect(const BYBGameHost *h,int x,int y,int w,int height,uint32_t c) {
    if(w>0 && height>0) h->fill_rect(h->draw_context,x,y,w,height,c);
}
void text(const BYBGameHost *h,const char *s,int x,int y,uint32_t c=Ink,int alignment=0) {
    h->draw_text(h->draw_context,s,x,y,c,alignment);
}
void stopMusic(State *s) {
#if defined(_WIN32) && !defined(NIGHTPULSE_TEST)
    if(s->stream) BASS_StreamFree(s->stream);
    for(int i=0;i<3;++i) if(s->hitSamples[i]) BASS_SampleFree(s->hitSamples[i]);
#endif
    s->stream=0;
    s->hitSamples[0]=s->hitSamples[1]=s->hitSamples[2]=0;
}
void start(State *s,const BYBGameHost *h) {
    stopMusic(s); s->rhythm.reset(); s->fallbackClock=s->lastAudioClock=0;
    ++s->roundId; s->resultReported=false; s->nextReportAt=0;
    s->staleAudio=s->resultWait=s->resultAge=0; s->audioWarning=false;
#if defined(_WIN32) && !defined(NIGHTPULSE_TEST)
    if(s->musicEnabled && h->asset_root && *h->asset_root) {
        const std::string path=std::string(h->asset_root)+"/assets/night_pulse.wav";
        s->stream=BASS_StreamCreateFile(FALSE,path.c_str(),0,0,0);
        if(!s->stream || !BASS_ChannelPlay(s->stream,TRUE)) {
            stopMusic(s); s->audioWarning=true;
        } else {
            const char *names[3]={"snare.wav","tom.wav","ride.wav"};
            for(int i=0;i<3;++i) {
                const std::string sample=std::string(h->asset_root)+"/assets/"+names[i];
                s->hitSamples[i]=BASS_SampleLoad(FALSE,sample.c_str(),0,0,4,0);
            }
        }
    }
#else
    (void)h;
#endif
}
void playHit(State *s) {
#if defined(_WIN32) && !defined(NIGHTPULSE_TEST)
    if(s->musicEnabled && s->rhythm.hitNote>=0) {
        const int instrument=noteTarget(s->rhythm.hitNote).instrument;
        if(s->hitSamples[instrument]) {
            const HCHANNEL channel=BASS_SampleGetChannel(s->hitSamples[instrument],FALSE);
            if(channel) { BASS_ChannelSetAttribute(channel,BASS_ATTRIB_VOL,.5f); BASS_ChannelPlay(channel,TRUE); }
        }
    }
#else
    (void)s;
#endif
}
double songClock(State *s,float delta) {
    s->fallbackClock+=delta;
#if defined(_WIN32) && !defined(NIGHTPULSE_TEST)
    if(s->stream) {
        const QWORD bytes=BASS_ChannelGetPosition(s->stream,BASS_POS_BYTE);
        const double position=bytes==static_cast<QWORD>(-1)?-1:BASS_ChannelBytes2Seconds(s->stream,bytes);
        if(position>=0 && std::isfinite(position)) {
            if(position>s->lastAudioClock+.001) s->staleAudio=0;
            else s->staleAudio+=delta;
            s->lastAudioClock=position;
            if(s->staleAudio<.75f) { s->fallbackClock=position; return position; }
        }
        stopMusic(s); s->audioWarning=true;
    }
#endif
    return s->fallbackClock;
}
void *BYB_GAME_CALL createGame() { return new State; }
void BYB_GAME_CALL destroyGame(void *p) { if(p) { stopMusic(static_cast<State *>(p)); delete static_cast<State *>(p); } }
void reportResult(State *s,const BYBGameHost *h) {
    if(!s->rhythm.finished || !s->roundId || s->resultReported || !h->report_result || s->visualTime<s->nextReportAt) return;
    s->resultReported=h->report_result(h->draw_context,s->roundId,s->rhythm.score,h->signal_source==3)!=0;
    s->nextReportAt=s->visualTime+1;
}
void BYB_GAME_CALL updateGame(void *p,const BYBGameHost *h) {
    if(!p || !h) return;
    State *s=static_cast<State *>(p);
    // Speaker playback would trigger a microphone-controlled game by itself.
    if(!s->inputConfigured) {
        if(h->signal_source!=2) s->musicEnabled=false;
        s->inputConfigured=true;
    }
    const float dt=std::isfinite(h->delta_seconds)?std::max(0.f,h->delta_seconds):0;
    s->visualTime+=std::min(dt,.1f);
    s->resultWait=std::max(0.f,s->resultWait-dt);
    if(s->rhythm.finished) s->resultAge=std::min(2.f,s->resultAge+dt);
    const bool tapped=h->signal_edge || s->pendingTap;
    s->pendingTap=false;
    if(!s->rhythm.started || s->rhythm.finished) {
        reportResult(s,h);
        if(tapped && s->resultWait<=0) start(s,h);
        return;
    }
    const int previousStrokes=s->rhythm.strokes;
    s->rhythm.update(songClock(s,dt),tapped);
    if(s->rhythm.strokes>previousStrokes) playHit(s);
    if(s->rhythm.finished) { stopMusic(s); s->resultWait=.7f; reportResult(s,h); }
}
void drawStage(const BYBGameHost *h,const Rhythm &r,int top,int height) {
    const Scene scene=makeStage(r,h->width,height);
    std::vector<float> xy,depth; std::vector<uint32_t> colors,vertices;
    size_t triangleCount=0;
    for(const auto &f:scene.faces) triangleCount+=f.points.size()-2;
    xy.reserve(triangleCount*6); depth.reserve(triangleCount*3);
    colors.reserve(triangleCount); vertices.reserve(triangleCount*3);
    for(size_t i=0;i<scene.faces.size();++i) {
        const Face &f=scene.faces[i];
        const uint32_t c=color(static_cast<unsigned>(f.tint.r),static_cast<unsigned>(f.tint.g),static_cast<unsigned>(f.tint.b));
        for(size_t j=1;j+1<f.points.size();++j) {
            const Vec3 v[3]={f.points[0],f.points[j],f.points[j+1]};
            const size_t indices[]={0,j,j+1};
            for(int k=0;k<3;++k) {
                xy.push_back(v[k].x); xy.push_back(v[k].y+top);
                depth.push_back(f.layer==4?v[k].z:0);
                const Tint shade=f.color(indices[k]);
                vertices.push_back(color(static_cast<unsigned>(shade.r),static_cast<unsigned>(shade.g),static_cast<unsigned>(shade.b)));
            }
            colors.push_back(c);
        }
    }
    if(h->draw_gradient_triangles && !colors.empty())
        h->draw_gradient_triangles(h->draw_context,xy.data(),vertices.data(),depth.data(),static_cast<int>(colors.size()));
    else if(h->draw_triangles && !colors.empty())
        h->draw_triangles(h->draw_context,xy.data(),colors.data(),static_cast<int>(colors.size()));
}
const char *judgementName(Judgement j) {
    switch(j) {
        case Perfect:return "PERFECT"; case Great:return "GREAT";
        case Good:return "GOOD"; case Miss:return "MISS"; case Early:return "첫 박자를 기다리세요";
        default:return "";
    }
}
void BYB_GAME_CALL renderGame(void *p,const BYBGameHost *h) {
    if(!p || !h || !h->fill_rect || !h->draw_text) return;
    const State *s=static_cast<State *>(p); const Rhythm &r=s->rhythm;
    const int w=h->width,height=h->height;
    if(w<620 || height<440) { rect(h,0,0,w,height,color(22,25,27)); text(h,"게임 창을 조금 더 크게 해주세요",18,60); return; }
    const int top=92,footer=height-88,sceneHeight=footer-top;
    rect(h,0,0,w,height,color(22,25,27));
    Rhythm pose=r; if(!r.started) pose.elapsed=s->visualTime;
    if(r.finished) pose.elapsed+=s->resultAge;
    drawStage(h,pose,top,sceneHeight);
    rect(h,0,0,w,top,color(19,22,24)); rect(h,0,top-1,w,1,color(60,64,63));
    text(h,"NIGHT PULSE",26,22); text(h,"DRUM KIT   /   120 BPM",26,51,Muted);
    char label[120];
    if(w>=760) {
        std::snprintf(label,sizeof(label),"%06d",r.score); text(h,label,w-330,22,Ink,2);
        text(h,"SCORE",w-330,51,Muted,2);
        std::snprintf(label,sizeof(label),"%d",r.combo); text(h,label,w-205,22,Accent,2);
        text(h,"COMBO",w-205,51,Muted,2);
    } else {
        std::snprintf(label,sizeof(label),"%06d   COMBO %d",r.score,r.combo);
        text(h,label,w-26,66,Ink,2);
    }
    const float progress=std::min(1.f,static_cast<float>(r.elapsed)/EndTime);
    rect(h,0,top-3,static_cast<int>(w*progress),2,Accent);
    for(int i=0;i<MissLimit;++i) rect(h,w-198+i*13,top+18,9,4,i<MissLimit-r.misses?Amber:color(63,64,61));
    std::snprintf(label,sizeof(label),"남은 기회  %d",MissLimit-r.misses);
    text(h,label,w-30,top+33,Muted,2);
    if(r.started && !r.finished) {
        if(r.elapsed<FirstNote) {
            std::snprintf(label,sizeof(label),"%d",std::max(1,4-static_cast<int>(r.elapsed/Beat)));
            text(h,label,w/2,top+sceneHeight/4,Ink,1);
        } else if(r.feedbackTime>0) text(h,judgementName(r.judgement),w/2,top+sceneHeight/5,
                r.judgement==Miss?color(212,141,120):Ink,1);
    }
    rect(h,0,footer,w,88,color(19,22,24)); rect(h,0,footer,w,1,color(60,64,63));
    const int meterWidth=std::min(w/3,340),meterX=26,meterY=footer+20;
    rect(h,meterX,meterY,meterWidth,8,color(49,56,56));
    const float denominator=std::max(1.f,h->trigger_level*2.5f);
    const float level=h->signal_available?std::max(0.f,std::min(1.f,h->signal_level/denominator)):0;
    rect(h,meterX,meterY,static_cast<int>(meterWidth*level),8,Accent);
    rect(h,meterX+static_cast<int>(meterWidth*.4f),meterY-4,2,16,Ink);
    text(h,h->signal_available?"신호가 표시선을 넘으면 연주":"신호 없음 · Space로 연습",meterX,meterY+18,Muted);
    text(h,"[ / ] 감도 조절",meterX,meterY+40,Muted);
    text(h,s->audioWarning?"오디오를 열지 못해 무음 연습 중":(s->musicEnabled?"반주 켜짐":"반주 꺼짐 · 마이크 반주는 이어폰 + M"),w-26,footer+18,Muted,2);
    text(h,(!r.started || r.finished)?"M 반주 설정 · 신호 / Space / 클릭으로 시작":
        "북 위 표시원에 닿을 때 연주 · 힘을 빼면 다음 입력 준비",w-26,footer+43,Ink,2);
    if(!r.started || r.finished) {
        const int cardWidth=std::min(420,w-52),cardX=26;
        const int cardY=top+std::max(20,(sceneHeight-208)/2);
        rect(h,cardX,cardY,cardWidth,208,color(24,28,30,246));
        rect(h,cardX,cardY,3,208,r.failed?color(173,110,92):Accent);
        text(h,r.finished?(r.failed?"다시 맞춰볼까요":"연주 완료"):"드럼 연습",cardX+22,cardY+22);
        if(r.finished) {
            std::snprintf(label,sizeof(label),"점수 %d   정확도 %.0f%%",r.score,r.accuracy()); text(h,label,cardX+22,cardY+62,Ink);
            std::snprintf(label,sizeof(label),"최고 콤보 %d   PERFECT %d",r.bestCombo,r.perfect); text(h,label,cardX+22,cardY+94,Muted);
        } else {
            text(h,"노트가 북 위 원에 닿을 때 힘을 주세요.",cardX+22,cardY+62,Ink);
            text(h,"한 번 힘주기 = 한 번 연주",cardX+22,cardY+94,Muted);
            text(h,"타이밍 실수 12번이면 연주가 끝납니다.",cardX+22,cardY+122,Muted);
        }
        text(h,"신호 / Space / 클릭으로 시작",cardX+22,cardY+166,Accent);
    }
}
void BYB_GAME_CALL keyEvent(void *p,int key,int pressed) {
    if(!p) return;
    State *s=static_cast<State *>(p);
    if(key==' ' || key==13) {
        if(pressed && !s->keyHeld) s->pendingTap=true;
        s->keyHeld=pressed!=0;
    } else if(key=='m' && pressed && (!s->rhythm.started || s->rhythm.finished)) {
        s->inputConfigured=true; s->musicEnabled=!s->musicEnabled;
    }
}
void BYB_GAME_CALL mouseEvent(void *p,int,int,int button) { if(p && button==1) static_cast<State *>(p)->pendingTap=true; }
const BYBGamePlugin plugin={BYB_GAME_PLUGIN_API_VERSION,"night-pulse","Night Pulse 3D",
    "Play a drum session with one signal.",createGame,destroyGame,updateGame,renderGame,keyEvent,mouseEvent};
}
extern "C" BYB_GAME_EXPORT const BYBGamePlugin *BYB_GAME_CALL BYB_GetGamePlugin(void) { return &plugin; }
#endif
