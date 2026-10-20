#ifndef LOTTOL_DINO_GAME_H
#define LOTTOL_DINO_GAME_H
#include "games/GamePlugin.h"
#include "DinoScene.h"
#include "DinoPeak.h"
#include <set>
#include <sstream>
#include <iomanip>
#include <stdint.h>
namespace BackyardBrains { namespace DinoPlugin {
const float kLongJumpThresholdMultiplier=1.8f,kMagicThresholdMultiplier=2.6f,kLaserThresholdMultiplier=3.4f;
const size_t kSignalHistorySize=100;
const int kSignalPanelWidth=220;
inline float clampFloat(float x,float lo,float hi) {return std::max(lo,std::min(hi,x));}
inline uint32_t rgba(int r,int g,int b,int a=255) {return (uint32_t(r)<<24)|(uint32_t(g)<<16)|(uint32_t(b)<<8)|uint32_t(a);}
namespace Palette {
const uint32_t background=0x131416ff,panel=0x191a1cff,border=0x303134ff,white=0xe8eaeeff,
 gray=0x85909bff,warning=0xdead70ff,button=0x242528ff,lightblue=0xadc1caff,plot=0x131416ff,grid=0x1d1e20ff;
}
struct Renderer {
    const BYBGameHost &h;uint32_t ink;int thickness;bool strip;std::vector<std::pair<float,float>> points;
    Renderer(const BYBGameHost &host):h(host),ink(Palette::white),thickness(1),strip(false) {}
    void color(uint32_t c) {ink=c;}
    void rect(int x,int y,int w,int z) {if(h.fill_rect && w>0 && z>0)h.fill_rect(h.draw_context,x,y,w,z,ink);}
    void text(const char *s,int x,int y,int align=0) {if(h.draw_text)h.draw_text(h.draw_context,s,x,y,ink,align);}
    void lineWidth(float w) {thickness=std::max(1,static_cast<int>(w));}
    void begin(bool continuous) {points.clear();strip=continuous;}
    void vertex(float x,float y) {points.push_back(std::make_pair(x,y));}
    void end() {
        if(h.draw_line)for(size_t i=1;i<points.size();i+=strip?1:2)
            h.draw_line(h.draw_context,points[i-1].first,points[i-1].second,points[i].first,points[i].second,thickness,ink);
        points.clear();
    }
};
struct State {
    Dino3D::Runner _run;Dino3D::PeakInput _peakInput;std::set<int> _heldKeys;
    bool _developerMode,_resultReported,_sensorAvailable,_sensorAboveThreshold,_calibrationComplete;
    uint64_t _runId;double _clock;uint32_t _gameStartedAt,_lastJumpAt,_lastResultAttempt,_historyAt;
    float _sensorLevel,_triggerLevel,_sensitivity;int _source,_width,_height;
    std::vector<float> _signalHistory;
    State():_developerMode(false),_resultReported(false),_sensorAvailable(false),_sensorAboveThreshold(false),_calibrationComplete(false),_runId(0),_clock(0),_gameStartedAt(0),_lastJumpAt(0),_lastResultAttempt(0),_historyAt(0),_sensorLevel(0),_triggerLevel(100),_sensitivity(1),_source(-1),_width(1280),_height(720) {resetGame();}
    int width() const {return _width;} int height() const {return _height;}
    uint32_t ticks() const {return static_cast<uint32_t>(_clock*1000);}
    void resetGame() {
        _run.reset();if(_developerMode)_run.practice=true;
        ++_runId;_resultReported=false;_lastResultAttempt=0;_peakInput.reset();
        _signalHistory.clear();_gameStartedAt=ticks();_lastJumpAt=0;
    }
    void jump(bool longJump) {_run.jump(longJump);}
    void update(const BYBGameHost &h) {
        const float dt=std::isfinite(h.delta_seconds)?std::max(0.f,std::min(10.f,h.delta_seconds)):0;_clock+=dt;
        const uint32_t now=ticks();
        if(_source!=h.signal_source) {_peakInput.reset();_source=h.signal_source;_gameStartedAt=now;_calibrationComplete=false;_signalHistory.clear();}
        _sensorAvailable=h.signal_available!=0;
        _sensorLevel=_sensorAvailable?std::max(0.f,h.signal_level):0;
        _triggerLevel=std::max(1.f,h.trigger_level);
        _sensitivity=h.input_sensitivity>0?h.input_sensitivity:1;
        if(now-_historyAt>=50) {_historyAt=now;_signalHistory.push_back(_sensorLevel);if(_signalHistory.size()>kSignalHistorySize)_signalHistory.erase(_signalHistory.begin());}
        _calibrationComplete=now-_gameStartedAt>=600;
        _sensorAboveThreshold=_sensorAvailable && _sensorLevel>=_triggerLevel;
        if(!_sensorAvailable || !_calibrationComplete) _peakInput.reset();
        else {
            const int action=_peakInput.sample(_sensorLevel/_triggerLevel,now,_run.magicUnlocked(),_run.laserUnlocked());
            if(action && now-_lastJumpAt>=350) {
                if(_run.canRetry())resetGame();
                if(action==4 && _run.laserUnlocked())_run.castLaser();
                else if(action==3 && _run.magicUnlocked())_run.castMagic();
                else jump(action>=2);
                _peakInput.held=true;_lastJumpAt=now;
            }
        }
        if(now-_gameStartedAt>=600)_run.advance(std::min(dt,.05f));
        if((_run.over || _run.won) && !_resultReported && (!_lastResultAttempt || now-_lastResultAttempt>=1000)) {
            _lastResultAttempt=now;
            if(!std::isfinite(_run.score) || _run.score<0 || _run.score>1000000000.f)_resultReported=true;
            else if(h.report_result)_resultReported=h.report_result(h.draw_context,_runId,static_cast<int64_t>(_run.score),_run.practice?1:0)!=0;
        }
    }
    void key(int code,bool pressed) {
        if(!pressed) {_heldKeys.erase(code);return;}
        if(!_heldKeys.insert(code).second)return;
        if(code==1073741883) {_developerMode=!_developerMode;if(_developerMode)_run.practice=true;else _run.invincible=false;}
        else if(_developerMode && ((code>='1'&&code<='4') || code=='e'||code=='i'||code=='c'||code=='r')) {
            if(code>='1'&&code<='4')_run.debugStage(code=='1'?Dino3D::Runner::Desert:code=='2'?Dino3D::Runner::Sky:code=='3'?Dino3D::Runner::Storm:Dino3D::Runner::Citadel);
            if(code=='e')_run.debugEvolve();
            if(code=='i')_run.debugInvincibility();
            if(code=='c')_run.debugRecharge();
            if(code=='r')resetGame();
            _peakInput.reset();
        } else if(code=='l')_run.castLaser();
        else if(code=='m')_run.castMagic();
        else if(code==' '||code==1073741906) {if(_run.canRetry())resetGame();jump(code==1073741906);}
    }
    void mouse(int x,int y,int button) {if(x<width()-kSignalPanelWidth && (button==1||button==4)) {if(_run.canRetry())resetGame();jump(button==4);}}
    void drawScene(const BYBGameHost &h,int playWidth) {
        const Dino3D::Scene scene=Dino3D::makeScene(_run,playWidth,height());
        std::vector<float> xy,depth;std::vector<uint32_t> colors,flat;
        auto flush=[&]() {if(colors.empty())return;if(h.draw_gradient_triangles)h.draw_gradient_triangles(h.draw_context,xy.data(),colors.data(),depth.data(),static_cast<int>(colors.size()/3));else if(h.draw_triangles)h.draw_triangles(h.draw_context,xy.data(),flat.data(),static_cast<int>(flat.size()));xy.clear();depth.clear();colors.clear();flat.clear();};
        for(size_t i=0;i<scene.faces.size();++i) {
            const Dino3D::Face &f=scene.faces[i];
            for(size_t j=1;j+1<f.points.size();++j) {
                if(flat.size()>=BYB_GAME_MAX_TRIANGLES)flush();
                const size_t indices[]={0,j,j+1};
                for(int v=0;v<3;++v) {const size_t k=indices[v];const Dino3D::Tint c=f.color(k);xy.push_back(f.points[k].x);xy.push_back(f.points[k].y);depth.push_back(f.layer==4?f.points[k].z:0);colors.push_back(rgba(clampFloat(c.r,0,255),clampFloat(c.g,0,255),clampFloat(c.b,0,255)));}
                flat.push_back(colors[colors.size()-3]);
            }
        }
        flush();
        if(h.draw_gradient_triangles) {
            const float fade[]={0,0,float(playWidth),0,float(playWidth),100,0,0,float(playWidth),100,0,100,
                0,float(height()-100),float(playWidth),float(height()-100),float(playWidth),float(height()),0,float(height()-100),float(playWidth),float(height()),0,float(height())};
            const float z[12]={};const uint32_t a=rgba(15,24,27,165),b=rgba(15,24,27,0),d=rgba(15,24,27,210);
            const uint32_t c[]={a,a,b,a,b,b,b,b,d,b,d,d};h.draw_gradient_triangles(h.draw_context,fade,c,z,4);
        }
    }
    void render(const BYBGameHost &h) {
        _width=h.width;_height=h.height;Renderer r(h);

    const int panelLeft = std::max(0, width() - kSignalPanelWidth);
    const int panelWidth = width() - panelLeft;
    const int graphLeft = panelLeft + 14;
    const int graphRight = std::max(graphLeft + 1, width() - 14);
    const int graphTop = 176;
    const int graphBottom = std::max(graphTop + 1, std::min(height() - (_run.laserUnlocked()?250:225), 360));
    const bool magic=_run.magicUnlocked();
    const bool laser=_run.laserUnlocked();
    const int extra=laser?92:(magic?46:0);
    const float graphHeight = static_cast<float>(graphBottom - graphTop);
    const float displayMax = std::max(80.0f,
        std::max(_triggerLevel * (laser?4.1f:3.2f), _sensorLevel * 1.15f));
    const float jumpRatio = clampFloat(_triggerLevel / displayMax, 0.02f, 0.96f);
    const float longRatio = clampFloat(
        (_triggerLevel * kLongJumpThresholdMultiplier) / displayMax, 0.02f, 0.96f);
    const float jumpLineY = static_cast<float>(graphBottom) - jumpRatio * graphHeight;
    const float longLineY = static_cast<float>(graphBottom) - longRatio * graphHeight;
    const float magicLineY=graphBottom-clampFloat(_triggerLevel*kMagicThresholdMultiplier/displayMax,0.02f,0.96f)*graphHeight;

    
    r.color(Palette::background);
    r.rect(0,0,width(),height());
    if(panelLeft > 0 && height() > 0)
        drawScene(h,panelLeft);

    // Paint the side panel after the playfield so incoming obstacles cannot
    // draw over the live signal meter before they reach the game area.
    r.color(Palette::panel);
    r.rect(panelLeft, 0, panelWidth, height());
    r.color(Palette::border);
    r.begin(false);
    r.vertex(panelLeft, 0);
    r.vertex(panelLeft, height());
    r.end();

    
    std::ostringstream scoreText;
    r.color(Palette::white);
    scoreText << "SCORE " << std::setw(5) << std::setfill('0')
              << static_cast<int>(_run.score);
    r.text(laser ? "STAGE 4 / DEMON KING" : _run.stage==Dino3D::Runner::Storm ? "STAGE 3 / STORM GUARDIAN" : (_run.stage==Dino3D::Runner::Sky ? "STAGE 2 / SKYBOUND" :
        (_run.stage==Dino3D::Runner::Ascending ? "DRAGON ASCENT" : "STAGE 1 / DESERT")), 22, 24);
    r.text(scoreText.str().c_str(), panelLeft - 20, 24, 2);
    std::ostringstream bestText;
    bestText << (_run.practice?"PRACTICE / BEST ":"BEST ") << static_cast<int>(_run.best);
    r.color(Palette::gray);
    r.text(bestText.str().c_str(), panelLeft - 20, 47, 2);
    std::ostringstream evolutionText;
    if(_run.bossStage()) evolutionText << (laser?"DEMON KING HP ":"GUARDIAN HP ") << _run.bossHealth << " / " << _run.bossMaxHealth();
    else if(_run.stage==Dino3D::Runner::Sky) evolutionText << "CRYSTALS " << _run.skyDestroyed << " / " << Dino3D::Runner::CrystalsToStorm;
    else evolutionText << "PEARLS " << _run.pearls << " / " << Dino3D::Runner::PearlsToEvolve;
    r.color(Palette::warning);
    r.text(evolutionText.str().c_str(), 22, 49);
    if(_run.stage==Dino3D::Runner::Ascending)
        r.text(_run.ascentTarget==Dino3D::Runner::Citadel ? "THE GUARDIAN BESTOWS ITS POWER / DIVINE AWAKENING" : _run.ascentTarget==Dino3D::Runner::Storm ? "MEGA EVOLUTION / ARCANE POWER AWAKENING" :
            "DRAGON AWAKENED! RISE INTO THE SKY...", 22, 74);
    else if(_run.stage==Dino3D::Runner::Sky && _run.stageTime<2.5f)
        r.text("BURN 8 CRYSTALS TO ENTER THE STORM", 22, 74);
    else if(_run.bossStage())
    {
        
        r.color(Palette::button);
        r.rect(22, 76, 192, 8);
        r.color(Palette::warning);
        r.rect(22, 76, 192*_run.bossHealth/_run.bossMaxHealth(), 8);
        
    }
    else if(_run.pickupTime > 0)
        r.text("PEARL COLLECTED!", 22, 74);

    
    r.color(Palette::button);
    r.rect(std::max(0, width() - 138), 12, 126, 36);
    
    r.color(Palette::white);
    r.text("GAME LIST", width() - 75, 22, 1);

    r.color(Palette::gray);
    r.text(laser ? (_run.bossHealth<=Dino3D::Runner::DemonMaxHealth/2?"ENRAGED / Watch the red seal. Golden peak: solar laser!":"DIVINE DRAGON / Golden peak: SOLAR LASER") : _run.stage==Dino3D::Runner::Ascending ? (_run.ascentTarget==Dino3D::Runner::Citadel?"The guardian lives on in you. Face the Demon King.":_run.ascentTarget==Dino3D::Runner::Storm?
        "Through the clouds... Get ready for Stage 3.":"Taking flight... Get ready for Stage 2.") :
        (_run.stage==Dino3D::Runner::Storm ? "MAGIC UNLOCKED / Highest peak: energy beam" :
        (_run.stage==Dino3D::Runner::Sky ? "Burn 8 crystals. Normal peak: flap / Strong peak: fire" :
        "Jump to collect 5 pearls and unlock the sky.")), 22, height() - 48);
    if(!_sensorAvailable)
        r.text(laser ? "SPACE: flap / UP: fire / M: magic / L: laser / F2: dev" : magic ? "SPACE: flap / UP: fire / M: magic / F2: developer" : (_run.dragon ? "SPACE / click: flap    UP / right click: FIRE / F2: dev" :
            "SPACE: jump / UP: long jump / F2: developer"), 22, height() - 26);
    else if(!_calibrationComplete)
        r.text("Relax for a moment. Measuring your baseline...", 22, height() - 26);
    else
        r.text("Release between peaks / [ ] sensitivity / F2: dev / ESC: list", 22, height() - 26);

    if(_developerMode) {
        
        r.color(rgba(17,26,38));
        r.rect(16,height()-155,std::min(550,std::max(1,panelLeft-32)),91);
        
        r.color(Palette::lightblue);
        r.text(_run.invincible?"DEVELOPER / INVINCIBLE ON / PRACTICE":"DEVELOPER / INVINCIBLE OFF / PRACTICE",26,height()-145);
        r.color(Palette::white);
        r.text("1 / 2 / 3 / 4: stage   E: evolve   I: invincible",26,height()-121);
        r.text("C: recharge   R: restart   M: magic   L: laser",26,height()-97);
    }

    // Right-hand live meter and scrolling signal trace.
    
    r.color(Palette::white);
    
    r.text("LIVE SIGNAL", panelLeft + 14, 68);
    std::ostringstream peakText;
    peakText << static_cast<int>(_sensorLevel) << " ADC";
    r.text(peakText.str().c_str(), panelLeft + 14, 91);
    
    r.color(Palette::button);
    r.rect(graphLeft, 116,
                                              std::max(1, graphRight - graphLeft), 15);
    const float meterFraction = _sensorAvailable
        ? clampFloat(_sensorLevel / displayMax, 0.0f, 1.0f) : 0.0f;
    r.color(_sensorAboveThreshold
        ? Palette::warning : Palette::lightblue);
    r.rect(graphLeft, 116,
        static_cast<int>((graphRight - graphLeft) * meterFraction), 15);

    r.color(Palette::plot);
    r.rect(graphLeft, graphTop,
                                              graphRight - graphLeft, graphBottom - graphTop);
    r.color(Palette::grid);
    r.lineWidth(1.0f);
    r.begin(false);
    for(int grid = 1; grid < 4; ++grid)
    {
        const float y = static_cast<float>(graphTop) + graphHeight * grid / 4.0f;
        r.vertex(static_cast<float>(graphLeft), y);
        r.vertex(static_cast<float>(graphRight), y);
    }
    r.end();

    r.color(Palette::lightblue);
    r.lineWidth(2.0f);
    r.begin(false);
    r.vertex(static_cast<float>(graphLeft), jumpLineY);
    r.vertex(static_cast<float>(graphRight), jumpLineY);
    r.end();
    r.color(Palette::warning);
    r.begin(false);
    r.vertex(static_cast<float>(graphLeft), longLineY);
    r.vertex(static_cast<float>(graphRight), longLineY);
    r.end();
    if(magic) {
        r.color(rgba(172,132,255));
        r.begin(false);
        r.vertex(static_cast<float>(graphLeft),magicLineY);
        r.vertex(static_cast<float>(graphRight),magicLineY);
        r.end();
    }

    if(laser) {
        const float y=graphBottom-clampFloat(_triggerLevel*kLaserThresholdMultiplier/displayMax,.02f,.96f)*graphHeight;
        r.color(rgba(255,217,94));
        r.begin(false); r.vertex(graphLeft,y); r.vertex(graphRight,y); r.end();
    }
    if(_signalHistory.size() > 1)
    {
        r.color(Palette::white);
        r.lineWidth(2.0f);
        r.begin(true);
        for(size_t i = 0; i < _signalHistory.size(); ++i)
        {
            const float x = static_cast<float>(graphLeft) +
                static_cast<float>(graphRight - graphLeft) * i /
                static_cast<float>(kSignalHistorySize - 1);
            const float level = clampFloat(_signalHistory[i] / displayMax, 0.0f, 1.0f);
            const float y = static_cast<float>(graphBottom) - level * graphHeight;
            r.vertex(x, y);
        }
        r.end();
    }

    
    // The matching legend below the plot stays readable even when the two
    // thresholds are close together or a large peak expands the graph range.

    std::ostringstream jumpText;
    jumpText << (_run.flyingStage() ? "FLAP  " : "JUMP  ") << static_cast<int>(_triggerLevel) << " ADC";
    r.color(Palette::lightblue);
    r.text(jumpText.str().c_str(), panelLeft + 14, graphBottom + 26);
    std::ostringstream longText;
    longText << (_run.dragon ? "FIRE  " : "LONG  ") << static_cast<int>(_triggerLevel * kLongJumpThresholdMultiplier)
             << " ADC";
    r.color(Palette::warning);
    r.text(longText.str().c_str(), panelLeft + 14, graphBottom + 49);
    if(magic) {
        std::ostringstream magicText;
        magicText << "MAGIC " << static_cast<int>(_triggerLevel*kMagicThresholdMultiplier) << " ADC";
        r.color(rgba(172,132,255));
        r.text(magicText.str().c_str(),panelLeft+14,graphBottom+72);
        std::ostringstream chargeText;
        if(_run.magicTime>0) chargeText << "ENERGY BEAM!";
        else if(_run.magicCooldown>0) chargeText << "Magic " << std::fixed << std::setprecision(1) << _run.magicCooldown << "s";
        else chargeText << "MAGIC READY / M";
        r.text(chargeText.str().c_str(),panelLeft+14,graphBottom+95);
    }
    if(laser) {
        std::ostringstream laserText;
        laserText << "LASER " << static_cast<int>(_triggerLevel*kLaserThresholdMultiplier) << " ADC";
        r.color(rgba(255,217,94));
        r.text(laserText.str().c_str(),panelLeft+14,graphBottom+118);
        std::ostringstream status;
        if(_run.laserTime>0) status << (Dino3D::Runner::laserDuration()-_run.laserTime<Dino3D::Runner::laserCharge()?"SOLAR CHARGING":"SOLAR LASER!");
        else if(_run.laserCooldown>0) status << "Laser " << std::fixed << std::setprecision(1) << _run.laserCooldown << "s";
        else status << "LASER READY / L";
        r.text(status.str().c_str(),panelLeft+14,graphBottom+141);
    }
    std::ostringstream sensitivityText;
    sensitivityText << "Sensitivity x" << std::fixed << std::setprecision(1)
                    << _sensitivity << "  [ / ]";
    r.color(Palette::gray);
    r.text(sensitivityText.str().c_str(), panelLeft + 14, graphBottom + 72+extra);
    if(_run.stage==Dino3D::Runner::Ascending)
    {
        r.color(Palette::warning);
        r.text("TAKING FLIGHT...", panelLeft + 14, graphBottom + 96+extra);
    }
    else if(_run.dragon)
    {
        r.color(Palette::warning);
        r.text(_run.fireTime > 0 ? "FIRE BREATH!" :
            (_run.fireCooldown > 0 ? "FIRE RECHARGING..." : "FIRE READY"),
            panelLeft + 14, graphBottom + 96+extra);
    }
    else if(_run.longJump)
    {
        r.color(Palette::warning);
        r.text("LONG JUMP!", panelLeft + 14, graphBottom + 96+extra);
    }
    if(!_sensorAvailable)
    {
        r.color(Palette::gray);
        r.text("No live input", panelLeft + 14, graphBottom + 120+extra);
    }

    if(_run.over || _run.won)
    {
        const int playAreaCenter = panelLeft / 2;
        
        r.color(rgba(29, 32, 38, 245));
        r.rect(playAreaCenter - 190,
                                                  height() / 2 - 48, 380, 96);
        r.color(Palette::white);
        
        r.text(_run.won ? "ALL 4 STAGES CLEAR!" : "GAME OVER", playAreaCenter, height() / 2 - 25,
                   1);
        r.text(_run.won && !_run.canRetry() ? "THE DEMON KING IS DEFEATED" : "PEAK, SPACE OR CLICK TO RETRY", playAreaCenter,
                   height() / 2 + 3, 1);
    }

    }
};
inline void *BYB_GAME_CALL create() {return new State;}
inline void BYB_GAME_CALL destroy(void *p) {delete static_cast<State *>(p);}
inline void BYB_GAME_CALL update(void *p,const BYBGameHost *h) {if(p&&h)static_cast<State *>(p)->update(*h);}
inline void BYB_GAME_CALL render(void *p,const BYBGameHost *h) {if(p&&h)static_cast<State *>(p)->render(*h);}
inline void BYB_GAME_CALL key(void *p,int code,int pressed) {if(p)static_cast<State *>(p)->key(code,pressed!=0);}
inline void BYB_GAME_CALL mouse(void *p,int x,int y,int button) {if(p)static_cast<State *>(p)->mouse(x,y,button);}
const BYBGamePlugin descriptor={BYB_GAME_PLUGIN_API_VERSION,"dino","Signal Dino 3D","Four-stage dinosaur and dragon adventure.",create,destroy,update,render,key,mouse};
} }
extern "C" BYB_GAME_EXPORT const BYBGamePlugin *BYB_GAME_CALL BYB_GetGamePlugin(void) {return &BackyardBrains::DinoPlugin::descriptor;}
#endif
