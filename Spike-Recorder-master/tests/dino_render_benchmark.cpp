#include "../game/dino/DinoScene.h"
#include <chrono>
#include <iostream>
int main() {
    using namespace BackyardBrains::Dino3D;
    Runner r; size_t peak=0,total=0;
    auto start=std::chrono::steady_clock::now();
    for(int i=0;i<120;++i) { r.distance=i*.6f; r.height=(i%4)*.65f;
        r.dragon=i%2; r.fireTime=r.dragon?.5f:0; r.time=i*.1f;
        r.burstTime=.4f; r.burstZ=8;
        r.stage=i%5==0?Runner::Desert:(i%5==1?Runner::Ascending:(i%5==2?Runner::Sky:(i%5==3?Runner::Storm:Runner::Citadel)));
        r.ascentTarget=i%3==0?Runner::Citadel:(i%3==1?Runner::Storm:Runner::Sky); r.stageTime=3;
        r.dragon=r.stage!=Runner::Desert; r.ascentTime=2; r.ascentStartHeight=1;
        r.magicTime=r.stage==Runner::Storm?.6f:0; r.magicHeight=r.height+4.5f; r.magicZ=10;
        r.laserTime=r.stage==Runner::Citadel?.8f:0; r.laserHeight=r.height+4.5f;
        const Scene s=makeScene(r,1000,680); peak=std::max(peak,s.faces.size()); total+=s.faces.size(); }
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/120;
    std::cout << "CPU geometry: " << ms << " ms/frame; max visible faces: " << peak << "; checksum: " << total << "\n";
    return peak>24000?1:0;
}
