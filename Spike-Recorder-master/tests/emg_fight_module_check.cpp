#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "games/GamePlugin.h"
#include <iostream>
#include <string>
static int triangles=0,labels=0;
void BYB_GAME_CALL rect(void *,int,int,int,int,uint32_t) {}
void BYB_GAME_CALL text(void *,const char *,float,float,float,uint32_t,int) {++labels;}
void BYB_GAME_CALL mesh(void *,const float *,const uint32_t *,int count) {triangles+=count;}
int main() {
    HMODULE module=LoadLibraryW(L"win/build/game/emg_fight/EMGFight.dll");
    if(!module) {std::cerr<<"LoadLibrary failed: "<<GetLastError()<<'\n';return 1;}
    BYBGetGamePluginFn get=reinterpret_cast<BYBGetGamePluginFn>(GetProcAddress(module,BYB_GAME_PLUGIN_ENTRY));
    if(!get) return 2;
    const BYBGamePlugin *game=get();
    if(!game || game->api_version!=BYB_GAME_PLUGIN_API_VERSION || std::string(game->id)!="emg-fight") return 3;
    void *state=game->create();if(!state) return 4;
    BYBGameHost host={};host.width=960;host.height=640;host.delta_seconds=.02f;host.player_count=2;
    host.player_names[0]="1P 테스트";host.player_names[1]="2P 테스트";
    host.fill_rect=rect;host.draw_scaled_text=text;host.draw_triangles=mesh;
    game->render(state,&host);game->key_event(state,13,1);game->update(state,&host);game->key_event(state,13,0);
    for(int i=0;i<125;++i)game->update(state,&host);
    game->key_event(state,'a',1);game->key_event(state,'j',1);
    for(int i=0;i<20;++i)game->update(state,&host);
    game->render(state,&host);game->destroy(state);FreeLibrary(module);
    if(triangles<1000 || labels<20) return 5;
    std::cout<<"PASS installed EMGFight.dll: load, descriptor, two players, update, keyboard, rendering, destroy\n";
}
