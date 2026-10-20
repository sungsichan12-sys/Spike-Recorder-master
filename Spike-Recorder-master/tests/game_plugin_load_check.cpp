#include <windows.h>
#include "games/GamePlugin.h"
#include <cmath>
#include <iostream>
static int countSeen=0;
static bool valid=true;
static void BYB_GAME_CALL rect(void *,int,int,int,int,uint32_t) {}
static void BYB_GAME_CALL text(void *,const char *,int,int,uint32_t,int) {}
static void BYB_GAME_CALL triangles(void *,const float *xy,const uint32_t *colors,int n) {
    valid=valid && xy && colors && n>0 && n<=BYB_GAME_MAX_TRIANGLES;
    if(!valid) return;
    countSeen+=n;
    for(int i=0;i<n*6;++i) valid=valid && std::isfinite(xy[i]);
}
static void BYB_GAME_CALL gradient(void *context,const float *xy,const uint32_t *colors,const float *depth,int n) {
    triangles(context,xy,colors,n);
    valid=valid && depth;
    if(depth) for(int i=0;i<n*3;++i) valid=valid && std::isfinite(depth[i]) && depth[i]>=0 && depth[i]<=3.334f;
}
int main() {
    // Rendering only: no window, recorder, microphone or audio initialization.
    HMODULE lib=LoadLibraryA("game/night-pulse/NightPulse.dll");
    if(!lib) { std::cerr<<"LoadLibrary error "<<GetLastError()<<"\n"; return 1; }
    BYBGetGamePluginFn entry=reinterpret_cast<BYBGetGamePluginFn>(GetProcAddress(lib,BYB_GAME_PLUGIN_ENTRY));
    const BYBGamePlugin *p=entry?entry():NULL;
    if(!p || p->api_version!=BYB_GAME_PLUGIN_API_VERSION) { FreeLibrary(lib); return 2; }
    void *state=p->create();
    BYBGameHost h={}; h.width=1200; h.height=760; h.fill_rect=rect; h.draw_text=text;
    h.draw_gradient_triangles=gradient; h.draw_triangles=triangles;
    p->render(state,&h); p->destroy(state); FreeLibrary(lib);
    std::cout<<"Plugin API "<<BYB_GAME_PLUGIN_API_VERSION<<": "<<countSeen<<" triangles, valid="<<valid<<"\n";
    return valid && countSeen>500?0:3;
}
