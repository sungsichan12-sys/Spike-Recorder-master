// Renders only this test's hidden OpenGL surface; never captures the desktop.
#include "AudioView.h"
#include "widgets/Application.h"
#include "widgets/TextureGL.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <windows.h>
#include <iostream>
#include <cmath>
#include <stdexcept>
#ifdef main
#undef main
#endif
using namespace BackyardBrains;
namespace BackyardBrains {
std::string getRecordingPath(){return "../../tests/core-artifacts";}
std::string getLoggingPath(){return "../../tests/core-artifacts/waveform-render.log";}
std::string getConfigPath(){return "../../tests/core-artifacts/waveform-config.xml";}
}
struct View : AudioView {
    View(RecordingManager &m,AnalysisManager &a):AudioView(NULL,m,a) {}
    void trace(std::vector<std::pair<int16_t,int16_t>> &data) {drawData(data,0,800,80,240,640,0);}
    void paint(){paintEvent();}
};
static int greenPixels() {
    std::vector<unsigned char> pixels(800*480*3);glReadPixels(0,0,800,480,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    int count=0;for(size_t i=0;i<pixels.size();i+=3)if(pixels[i+1]>100 && pixels[i]<80 && pixels[i+2]<100)++count;return count;
}
static void capture(RecordingManager &m,int milliseconds) {
    DWORD start=GetTickCount(),last=start;
    while(GetTickCount()-start<static_cast<DWORD>(milliseconds)) {
        Sleep(20);DWORD now=GetTickCount();m.advance((now-last)*m.sampleRate()/1000);last=now;
    }
}
static int waveformPixels(View &view) {
    glBindTexture(GL_TEXTURE_2D,0);glEnable(GL_TEXTURE_2D);
    glClear(GL_COLOR_BUFFER_BIT);view.paint();glFinish();
    std::vector<unsigned char> pixels(800*480*3);glReadPixels(0,0,800,480,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    int count=0;for(int y=50;y<430;++y)for(int x=90;x<760;++x) {
        const size_t i=(y*800+x)*3;const int r=pixels[i],g=pixels[i+1],b=pixels[i+2];
        if((g>150 && r<110 && b>100) || (r>170 && g<160 && b<170))++count;
    }
    return count;
}
int main() {
    if(SDL_Init(SDL_INIT_VIDEO)<0)return 2;
    SDL_Window *window=SDL_CreateWindow("Hidden waveform check",0,0,800,480,SDL_WINDOW_HIDDEN|SDL_WINDOW_OPENGL);
    SDL_GLContext context=window?SDL_GL_CreateContext(window):NULL;
    if(!context){std::cerr<<SDL_GetError()<<'\n';return 3;}
    // Application owns SDL's lifetime. Its destruction closes this hidden surface.
    Widgets::Application app;RecordingManager manager;AnalysisManager analysis(manager);View view(manager,analysis);
    Widgets::TextureGL::load("data/pin.bmp");Widgets::TextureGL::load("data/threshpin.bmp");
    Widgets::TextureGL::load("data/gainup.bmp");Widgets::TextureGL::load("data/gaindown.bmp");
    view.setGeometry(Widgets::Rect(0,0,800,480));
    if(view.channelCount()==0){std::cerr<<"No test input channels\n";return 4;}
    glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,800,480,0,-100,100);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    std::vector<std::pair<int16_t,int16_t>> data;
    for(int i=0;i<800;++i){const int16_t sample=1000*std::sin(i*.06);data.push_back({sample,sample});}
    GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    const unsigned char transparent[4]={255,255,255,0};
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,transparent);
    glEnable(GL_TEXTURE_2D);glTexCoord2f(0,0);
    glClearColor(.05f,.05f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);glColor4f(0,1,0,1);view.trace(data);glFinish();
    const int pixels=greenPixels();
    std::cout<<"TRACE pixels="<<pixels<<" texture_enabled="<<int(glIsEnabled(GL_TEXTURE_2D))<<'\n';
    glDeleteTextures(1,&texture);
    if(pixels<500){std::cerr<<"FAIL waveform disappeared under inherited transparent texture\n";return 1;}
    std::cout<<"PASS actual OpenGL waveform remains visible after textured UI\n";
    manager.devicesChanged.connect(static_cast<AudioView*>(&view),&AudioView::updateChannels);
    manager.setPaused(false);capture(manager,2300);view.resetLiveDisplay();
    const int microphonePixels=waveformPixels(view);
    std::cout<<"MICROPHONE frames="<<manager.pos()<<" channels="<<view.channelCount()<<" waveform_pixels="<<microphonePixels<<'\n';
    if(microphonePixels<100){std::cerr<<"FAIL real microphone waveform not visible\n";return 5;}
    manager.scanForHIDDevices();
    const auto board=manager.isHIDBoardTypeAvailable(HID_BOARD_TYPE_MUSCLE)?HID_BOARD_TYPE_MUSCLE:HID_BOARD_TYPE_NEURON;
    if(!manager.initHIDUSB(board)){std::cerr<<"FAIL "<<manager.hidError<<'\n';return 6;}
    capture(manager,2300);view.resetLiveDisplay();
    const int hidPixels=waveformPixels(view);
    std::cout<<"HID frames="<<manager.pos()<<" channels="<<view.channelCount()<<" waveform_pixels="<<hidPixels<<" signal_channel="<<manager.signalDeviceIndex()+1<<'\n';
    if(view.channelCount()!=2 || hidPixels<100){std::cerr<<"FAIL two-channel HID waveform not visible\n";return 7;}
    manager.disconnectFromHID();manager.setPaused(false);capture(manager,2300);view.resetLiveDisplay();
    const int restored=waveformPixels(view);
    std::cout<<"RESTORED_MIC frames="<<manager.pos()<<" channels="<<view.channelCount()<<" waveform_pixels="<<restored<<'\n';
    if(restored<100){std::cerr<<"FAIL microphone waveform lost after HID disconnect\n";return 8;}
    std::cout<<"PASS full OpenGL microphone / two-channel HID / restored microphone waveforms\n";return 0;
}
