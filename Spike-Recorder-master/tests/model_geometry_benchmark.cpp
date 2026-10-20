#include "../game/dino/DinoScene.h"
#include "../game/night-pulse/Stage.h"
#include <chrono>
#include <functional>
#include <iostream>
using namespace BackyardBrains::Scene3D;
namespace Dino=BackyardBrains::Dino3D;
static void measure(const char *name,const std::function<Scene()> &build) {
    size_t faces=0,triangles=0;
    const auto begin=std::chrono::steady_clock::now();
    for(int i=0;i<12;++i) {
        const Scene scene=build(); faces=scene.faces.size(); triangles=0;
        for(const auto &f:scene.faces) triangles+=f.points.size()-2;
    }
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/12;
    std::cout<<name<<","<<faces<<","<<triangles<<","<<ms<<"\n";
}
int main() {
    std::cout<<"model,visible_faces,visible_triangles,cpu_geometry_ms\n";
    for(int form=0;form<5;++form) measure(form==0?"dinosaur":form==1?"dragon":form==2?"mega_dragon":form==3?"divine_dragon":"guardian",[form] {
        Dino::Runner r;
        if(form) r.debugStage(form==1?Dino::Runner::Sky:form==2||form==4?Dino::Runner::Storm:Dino::Runner::Citadel);
        r.height=0; r.distance=2; r.time=.35f;
        Scene s(1000,680); s.setCamera(Vec3(4,2.6f,1.8f),Vec3(0,1.1f,-.1f),590); s.fogStart=50;
        Dino::dinosaur(s,r,form==4); return s;
    });
    measure("demon_king",[] {
        Dino::Runner r; r.debugStage(Dino::Runner::Citadel); r.time=1.1f; r.bossShotCooldown=.3f;
        Scene s(1000,680); s.setCamera(Vec3(5,6.3f,5),Vec3(0,5.3f,16),720); s.fogStart=50;
        Dino::demonKing(s,r); return s;
    });
    measure("drum_kit",[] { NightPulse::Rhythm r; r.reset(); r.update(2.065,true); return NightPulse::makeStage(r,1200,580); });
}
