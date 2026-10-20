// Exercises the native game-start UI on a hidden, isolated OpenGL surface.
#include "GameHubView.h"
#include "widgets/Application.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_opengl.h>
#include <windows.h>
#include <iostream>
#include <stdexcept>
#ifdef main
#undef main
#endif
using namespace BackyardBrains;
namespace BackyardBrains {
std::string getRecordingPath(){return "../../tests/core-artifacts";}
std::string getLoggingPath(){return "../../tests/core-artifacts/calibration-flow.log";}
std::string getConfigPath(){return "../../tests/core-artifacts/calibration-config.xml";}
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
struct GameCalibrationFixture {
    static void key(GameHubView &hub,int code,bool down=true) {
        Widgets::KeyboardEvent event(static_cast<Widgets::KeyCode>(code),0);
        if(down)hub.keyPressEvent(&event);else hub.keyReleaseEvent(&event);
    }
    static void namedGame(GameHubView &hub,const char *id) {
        hub.open();int index=-1;
        for(size_t i=0;i<hub._games.size();++i)if(hub._games[i].id==id)index=static_cast<int>(i);
        check(index>=0,"Test game not found");hub._selectedGame=index;hub.startSelectedGame();
        hub._nameEntry.insert("기준선 테스트");hub.confirmPlayerName();
        if(hub._nameEntry.open){hub._nameEntry.enterReady=true;hub._nameEntry.insert("두번째 테스트");hub.confirmPlayerName();}
        check(hub._inputSetup.open() && !hub._plugin,"Name confirmation launched game before calibration");
        key(hub,SDLK_RETURN,false);
    }
    static void readyReview(GameHubView &hub) {
        auto &c=hub._inputSetup;
        const uint32_t start=100;
        c.start(start);for(int i=0;i<=60;++i)c.feed(200+i%5,start+i*50,true);
        c.start(3200);for(int i=0;i<=60;++i)c.feed(2200+i%5,3200+i*50,true);
        for(int i=0;i<=12;++i)c.feed(200,6300+i*50,true);
        hub._setupAvailable[c.player]=true;hub._setupLevels[c.player]=200;
        check(c.stage==GameInput::Calibration::Review && c.quiet,"Test calibration not ready");
        hub._setupHistory.clear();
        for(int i=0;i<120;++i)hub._setupHistory.push_back(i>75?200+float(i%5)*3:300+1400*std::sin(float(i)*.04f)*std::sin(float(i)*.04f));
    }
    static void picture(GameHubView &hub,const char *path,int w=800,int h=600) {
        hub.setGeometry(Widgets::Rect(0,0,w,h));glViewport(0,0,w,h);
        glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,w,h,0,-100,100);
        glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        hub.paintCalibration();glFinish();
        std::vector<unsigned char> pixels(w*h*4),flipped(w*h*4);
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        for(int y=0;y<h;++y)std::copy(pixels.begin()+(h-1-y)*w*4,pixels.begin()+(h-y)*w*4,flipped.begin()+y*w*4);
        SDL_Surface *surface=SDL_CreateRGBSurfaceFrom(flipped.data(),w,h,32,w*4,0xff,0xff00,0xff0000,0xff000000);
        check(surface!=NULL,"Cannot create preview surface");
        const int result=IMG_SavePNG(surface,path);SDL_FreeSurface(surface);check(result==0,"Cannot save preview");
    }
    static void run(GameHubView &hub,RecordingManager &manager) {
        namedGame(hub,"dino");
        picture(hub,"../../tests/core-artifacts/calibration-rest.png");
        hub._setupAvailable[0]=true;key(hub,SDLK_RETURN);key(hub,SDLK_RETURN,false);
        check(hub._inputSetup.stage==GameInput::Calibration::Rest,"First Enter after mouse name confirmation was ignored");hub._inputSetup.retry();
        key(hub,SDLK_SPACE);check(!hub._plugin,"Space bypassed calibration");key(hub,SDLK_SPACE,false);
        readyReview(hub);picture(hub,"../../tests/core-artifacts/calibration-review.png");
        picture(hub,"../../tests/core-artifacts/calibration-small.png",640,480);
        key(hub,SDLK_UP);key(hub,SDLK_UP,false);check(hub._inputSetup.profiles[0].fraction>.25f,"Threshold controls failed");
        hub._inputSetup.quiet=false;key(hub,SDLK_RETURN);key(hub,SDLK_RETURN,false);check(!hub._plugin,"Held input bypassed review release");
        hub._inputSetup.quiet=true;key(hub,SDLK_RETURN);
        check(hub._plugin && hub._profilesReady && !hub._keyboardOnly && !hub._inputSetup.open(),"Calibrated Dino did not launch");
        auto *state=hub._pluginState;key(hub,SDLK_RETURN);check(hub._pluginState==state,"Enter repeat leaked through launch");key(hub,SDLK_RETURN,false);
        hub._gameSignatures[0]="changed source";hub.updateSignal(SDL_GetTicks());
        check(hub._inputSetup.open() && hub._pluginState==state && !hub._profilesReady,"Source change reused an old calibration or destroyed game state");
        hub.finishCalibration(true);check(hub._pluginState==state,"Input setup did not resume the existing game");
        hub.toggle();check(!hub._profilesReady && !hub._channelLocked && manager.autoSignalChannel(),"Leaving game retained channel lock");
        namedGame(hub,"dino");key(hub,SDLK_k);
        check(hub._plugin && hub._keyboardOnly && !hub._profilesReady,"Keyboard-only launch failed");
        hub.updateSignal(SDL_GetTicks());check(!hub._sensorAvailable && hub._signalLevel==0 && !hub._signalEdge,"Keyboard-only mode still accepted noise");key(hub,SDLK_k,false);
        hub.toggle();namedGame(hub,"emg-fight");check(hub._inputSetup.players==2,"Two-player calibration missing");
        readyReview(hub);hub.calibrationAction();check(!hub._plugin && hub._inputSetup.player==1,"Second player skipped");
        readyReview(hub);hub._setupAvailable[0]=true;hub._inputSetup.profiles[0].released=SDL_GetTicks()-600;
        hub.calibrationAction();check(hub._plugin && hub._profilesReady && hub._inputSetup.profiles[0].valid && hub._inputSetup.profiles[1].valid,"Independent two-player setup did not launch");
        hub.toggle();namedGame(hub,"dino");key(hub,SDLK_ESCAPE);key(hub,SDLK_ESCAPE,false);
        check(!hub._inputSetup.open() && !hub._plugin && hub._menuOpen && !hub._channelLocked,"Cancel did not restore menu");
        std::cout<<"PASS names / calibration gate / release / threshold controls / Enter latch / channel unlock / keyboard-only / two players / cancellation\n";
    }
};
}
int main(){
    try {
        SetEnvironmentVariableW(L"LOTTOL_SCORE_OUTBOX",L"../../tests/core-artifacts/calibration-outbox");
        if(SDL_Init(SDL_INIT_VIDEO)<0)return 2;
        SDL_Window *window=SDL_CreateWindow("Hidden calibration check",0,0,800,600,SDL_WINDOW_HIDDEN|SDL_WINDOW_OPENGL);
        if(!window || !SDL_GL_CreateContext(window))return 3;
        Widgets::Application app;RecordingManager manager;GameHubView hub(manager);
        GameCalibrationFixture::run(hub,manager);
        std::cout<<"RESULT native calibration flow passed; previews saved\n";return 0;
    } catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
