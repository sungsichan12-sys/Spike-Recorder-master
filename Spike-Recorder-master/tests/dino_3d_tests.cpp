#include "../game/dino/DinoScene.h"
#include "../game/dino/DinoPeak.h"
#include <iostream>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <limits>
#include <sstream>
#include <iomanip>
using namespace BackyardBrains::Dino3D;
static int passed=0,failed=0;
// Foreground meshes now contain roughly three times as many triangles. Keep a
// bounded whole-scene budget, including two creatures, effects and scenery.
static const size_t SceneFaceBudget=24000;
static void check(bool ok,const char *why) { if(!ok) throw std::runtime_error(why); }
static void test(const char *name,const std::function<void()> &f) {
    try { f(); ++passed; std::cout << "PASS " << name << "\n"; }
    catch(const std::exception &e) { ++failed; std::cout << "FAIL " << name << ": " << e.what() << "\n"; }
}
static void step(Runner &r,float seconds,float dt=1.f/120) {
    for(int i=0;i<static_cast<int>(seconds/dt+.5f);++i) r.advance(dt);
}
static void exportScene(const char *name,const Scene &s) {
    std::ofstream out(name);
    if(!out) throw std::runtime_error("Cannot write preview frame");
    out << "{\"width\":1000,\"height\":680,\"faces\":[";
    for(size_t i=0;i<s.faces.size();++i) {
        const Face &f=s.faces[i];
        if(i) out << ',';
        out << "{\"rgb\":[" << f.tint.r << ',' << f.tint.g << ',' << f.tint.b << "],\"points\":[";
        for(size_t j=0;j<f.points.size();++j) {
            if(j) out << ',';
            out << '[' << f.points[j].x << ',' << f.points[j].y << ']';
        }
        out << "],\"layer\":" << f.layer << ",\"depth\":[";
        for(size_t k=0;k<f.points.size();++k) { if(k) out << ','; out << f.points[k].z; }
        out << "]";
        if(!f.colors.empty()) {
            out << ",\"colors\":[";
            for(size_t k=0;k<f.colors.size();++k) {
                if(k) out << ',';
                out << '[' << f.colors[k].r << ',' << f.colors[k].g << ',' << f.colors[k].b << ']';
            }
            out << ']';
        }
        out << '}';
    }
    out << "]}";
}
static void exportFrame(const char *name,const Runner &r) {
    exportScene(name,makeScene(r,1000,680));
}
int main(int argc,char **argv) {
    if(argc>1 && std::string(argv[1])=="--ascent-preview") {
        Runner r; r.height=1.2f; r.pearls=Runner::PearlsToEvolve; r.distance=17; r.beginAscent();
        for(int i=0;i<72;++i) {
            std::ostringstream path; path<<"tests/dino-cinematic/"<<std::setw(3)<<std::setfill('0')<<i<<".json";
            exportFrame(path.str().c_str(),r); r.advance(1.f/15);
        }
        std::cout<<"Exported 72 actual cinematic frames\n";
        return 0;
    }
    test("Unjumped obstacle causes game over",[] {
        Runner r; step(r,4); check(r.over,"Player ran through a cactus");
        const float score=r.score; step(r,1); check(r.score==score,"Score advanced after death");
    });
    test("Timed short jump clears cactus",[] {
        Runner r; r.obstacleZ=3.4f; r.jump(false); step(r,1);
        check(!r.over && r.passed==1 && r.height==0,"A reachable obstacle cannot be cleared");
    });
    test("Late jump collides",[] {
        Runner r; r.obstacleZ=.75f; r.jump(false); step(r,.3f);
        check(r.over,"Late jump incorrectly clears obstacle");
    });
    test("Long jump lasts longer and rises higher",[] {
        Runner a,b; a.jump(false); b.jump(true); step(a,.4f); step(b,.4f);
        check(b.height>a.height+1,"Long jump is not higher");
        step(a,.5f); step(b,.5f);
        check(a.height==0 && b.height>.5f,"Long jump duration incorrect");
    });
    test("No double jump or jumping after death",[] {
        Runner r; check(r.jump(false),"Ground jump rejected");
        check(!r.jump(true),"Double jump allowed at takeoff");
        step(r,.2f); check(!r.jump(true),"Midair jump allowed");
        r.over=true; check(!r.jump(false),"Dead player jumped");
    });
    test("Physics agrees at 20 and 120 fps",[] {
        Runner a,b; a.obstacleZ=b.obstacleZ=3.4f; a.jump(false); b.jump(false);
        step(a,1,.05f); step(b,1,1.f/120);
        check(a.over==b.over && a.passed==b.passed && std::fabs(a.distance-b.distance)<.01f,"Frame-rate-dependent result");
    });
    test("Reset keeps best and clears death",[] {
        Runner r; step(r,4); float best=r.best; r.reset();
        check(!r.over && r.score==0 && r.height==0 && r.best==best && best>0,"Bad retry state");
    });
    test("Invalid time cannot corrupt state",[] {
        Runner r; r.advance(-1); r.advance(std::numeric_limits<float>::quiet_NaN());
        check(r.distance==0 && !r.over,"Invalid time accepted");
    });
    test("Pearls require an airborne pickup",[] {
        Runner r; r.obstacleZ=50; r.pearlZ=.4f;
        step(r,.3f);
        check(r.pearls==0 && !r.dragon,"Ground running collected an airborne pearl");
    });
    test("Short and long jumps collect once while clearing the obstacle",[] {
        for(int extended=0;extended<2;++extended) {
            Runner r; r.obstacleZ=3.4f; r.pearlZ=4;
            r.jump(extended!=0); step(r,.65f);
            check(!r.over && r.pearls==1 && !r.pearlActive,"Jump failed to collect the pearl safely");
            step(r,.1f); check(r.pearls==1,"Pearl awarded twice");
        }
    });
    test("Five naturally spawned pearls evolve the runner",[] {
        Runner r;
        for(int i=0;i<3600 && !r.dragon && !r.over;++i) {
            if(r.height==0 && r.obstacleZ<=r.speed()*.32f) r.jump(false);
            r.advance(1.f/120);
        }
        check(!r.over && r.dragon && r.stage==Runner::Ascending && !r.obstacleActive && r.pearls==Runner::PearlsToEvolve && r.evolutionTime>0,
            "Normal play could not reach dragon evolution");
    });
    test("Dragon strong input breathes and normal input still jumps",[] {
        Runner r; r.dragon=true; r.stage=Runner::Sky;
        check(r.jump(true) && r.fireTime>0 && r.velocity==0 && !r.longJump,"Strong input still jumps after evolution");
        check(!r.jump(true),"Fire cooldown bypassed");
        check(r.jump(false) && r.velocity>0,"Normal dragon jump disabled");
        step(r,1.1f); check(r.jump(true),"Breath did not recharge");
    });
    test("Breath removes a nearby obstacle before collision",[] {
        Runner r; r.dragon=true; r.stage=Runner::Sky; r.obstacleZ=.5f; r.jump(true); step(r,.1f);
        check(!r.over && !r.obstacleActive && r.destroyed==1,"Timely breath did not prevent collision");
        step(r,.1f); check(r.destroyed==1,"Burned obstacle scored twice");
        step(r,.5f); check(r.obstacleActive && r.obstacleZ>20,"Destroyed slot never respawned");
    });
    test("Breath range and expiry do not grant invincibility",[] {
        Runner r; r.dragon=true; r.stage=Runner::Sky; r.obstacleZ=30; r.jump(true); step(r,.8f);
        check(r.obstacleActive && r.destroyed==0 && r.fireTime==0,"Out-of-range obstacle burned");
        r.obstacleZ=.4f; step(r,.02f); check(r.over,"Dragon survived contact without breath");
        check(!r.jump(true),"Dead dragon fired");
    });
    test("Missed pearl is replaced and retry clears evolution",[] {
        Runner r; r.obstacleActive=false; r.obstacleZ=-3.4f; r.pearlActive=false; r.pearlZ=-5;
        step(r,.05f); check(r.pearlActive && r.pearlZ>20,"Missed pearl blocked future collection");
        r.dragon=true; r.pearls=Runner::PearlsToEvolve; r.destroyed=4; r.fireTime=.6f; r.burstTime=.4f; r.best=777;
        r.reset();
        check(!r.dragon && r.pearls==0 && r.destroyed==0 && r.fireTime==0 && r.burstTime==0 &&
            r.fireCooldown==0 && r.evolutionTime==0 && r.obstacleActive && r.pearlActive && r.best==777,"Retry kept previous-round powers");
    });
    test("Full five-pearl campaign reaches the Stage 4 victory",[] {
        Runner r;
        for(int i=0;i<30000 && !r.won && !r.over;++i) {
            if(r.dragon) {
                if(r.laserUnlocked() && r.laserCooldown==0 && (!r.obstacleActive || r.obstacleZ>14)) r.castLaser();
                else if(r.magicUnlocked() && r.magicCooldown==0 && (!r.obstacleActive || r.obstacleZ>10)) r.castMagic();
                else if(r.fireCooldown==0 && ((r.obstacleActive && r.obstacleZ<10) ||
                    (r.bossStage() && !r.obstacleActive))) r.jump(true);
            } else if(r.height==0 && r.obstacleZ<=r.speed()*.32f) r.jump(false);
            r.advance(1.f/120);
        }
        check(!r.over && r.won && r.stage==Runner::Citadel && r.pearls==5 && r.skyDestroyed==8 && r.bossHealth==0,
            "Four-stage campaign stalled or became impossible");
    });
    test("Ascent is protected, ignores input and enters Stage 2",[] {
        Runner r; r.height=1.2f; r.pearls=Runner::PearlsToEvolve; r.score=123; r.beginAscent();
        r.obstacleZ=.1f; // Even a stale nearby obstacle cannot interrupt the transition.
        check(!r.jump(false) && !r.jump(true),"Transition accepts gameplay input");
        step(r,3.4f);
        check(!r.over && r.stage==Runner::Ascending && r.height>7 && !r.obstacleActive,"Launch did not climb safely");
        step(r,.3f);
        check(r.stage==Runner::Sky && r.dragon && r.height==0 && r.obstacleZ>39 &&
            r.obstacleActive && !r.pearlActive && r.fireTime==0 && r.score>123,"Stage 2 did not initialize cleanly");
    });
    test("Sky flap clears crystals and returns to safe cruise altitude",[] {
        Runner r; r.stage=Runner::Sky; r.dragon=true; r.pearlActive=false; r.obstacleZ=3.4f;
        r.jump(false); step(r,1.1f);
        check(!r.over && r.passed==1 && r.height==0,"Flap cannot clear a crystal or failed to settle");
        r.obstacleActive=false; step(r,1);
        check(r.height==0 && !r.over,"Idle flight falls out of the sky");
    });
    test("Ascent timing agrees at 20 and 120 fps",[] {
        Runner a,b; a.height=b.height=1.1f; a.beginAscent(); b.beginAscent();
        step(a,4,.05f); step(b,4,1.f/120);
        check(a.stage==Runner::Sky && b.stage==Runner::Sky && std::fabs(a.obstacleZ-b.obstacleZ)<.06f,
            "Stage transition depends on frame rate");
    });
    test("Retry during ascent or sky starts a fresh desert run",[] {
        for(int state=0;state<2;++state) {
            Runner r; r.pearls=Runner::PearlsToEvolve; r.beginAscent(); step(r,state?4.f:1.f); const float best=r.best;
            r.reset(); check(r.stage==Runner::Desert && r.ascentTime==0 && r.stageTime==0 &&
                !r.dragon && r.height==0 && r.pearls==0 && r.pearlActive && r.best==best,"Retry retained stage state");
        }
    });
    test("Camera projection has perspective",[] {
        Scene s(1000,680);
        Vec3 near=s.project(Vec3(1,0,5)),far=s.project(Vec3(1,0,10));
        check(std::fabs((near.x-500)/(far.x-500)-2)<.001f,"No perspective foreshortening");
    });
    test("Evolution now requires exactly five pearls",[] {
        Runner r; r.obstacleZ=50; r.height=1.2f; r.pearlZ=.2f; r.pearls=3;
        r.advance(.01f); check(r.pearls==4 && !r.dragon,"Four pearls already evolved");
        r.pearlActive=true; r.pearlZ=.2f; r.advance(.01f);
        check(r.pearls==5 && r.stage==Runner::Ascending,"Fifth pearl did not trigger evolution");
    });
    test("Eight sky crystals unlock a protected Stage 3 transition",[] {
        Runner r; r.stage=Runner::Sky; r.dragon=true; r.skyDestroyed=7;
        r.obstacleZ=5; r.jump(true); r.advance(.01f);
        check(r.stage==Runner::Ascending && r.ascentTarget==Runner::Storm && !r.obstacleActive &&
            !r.jump(true),"Storm transition failed or accepted input");
        step(r,4);
        check(r.stage==Runner::Storm && r.bossHealth==Runner::BossMaxHealth && !r.obstacleActive &&
            r.bossShotCooldown>1,"Boss entry had no grace period");
    });
    test("One breath damages the boss once and respects incoming attacks",[] {
        Runner r; r.stage=Runner::Storm; r.dragon=true; r.obstacleActive=false; r.bossShotCooldown=100;
        r.jump(true); step(r,.6f);
        check(r.bossHealth==Runner::BossMaxHealth-1,"Breath dealt frame-dependent boss damage");
        step(r,.5f); r.obstacleActive=true; r.obstacleZ=7; r.jump(true); step(r,.2f);
        check(!r.obstacleActive && r.bossHealth==Runner::BossMaxHealth-1,"Blocked breath damaged boss too");
    });
    test("Guardian attacks can be dodged, burned, or cause failure",[] {
        for(int action=0;action<3;++action) {
            Runner r; r.stage=Runner::Storm; r.dragon=true; r.obstacleZ=3.4f; r.bossShotCooldown=100;
            if(action==1) r.jump(false);
            if(action==2) r.jump(true);
            step(r,.6f);
            check(r.over==(action==0),"Guardian attack collision or defense is incorrect");
        }
    });
    test("Final boss defeat freezes score, delays retry, and resets the campaign",[] {
        Runner r; r.stage=Runner::Citadel; r.dragon=true; r.obstacleActive=false; r.bossShotCooldown=100;
        r.bossHealth=1; r.jump(true); r.advance(.02f);
        check(r.won && !r.over && r.bossHealth==0 && !r.jump(true) && !r.canRetry(),"Victory or input lock failed");
        const float score=r.score; step(r,2);
        check(r.score==score && r.canRetry() && r.victoryTime>=1.8f,"Victory score or delayed retry failed");
        r.reset(); check(!r.won && r.stage==Runner::Desert && r.bossHealth==Runner::BossMaxHealth &&
            r.skyDestroyed==0 && r.pearls==0,"Retry retained boss progress");
    });
    test("All three cinematic timelines remain finite and bounded",[] {
        for(int destination=0;destination<3;++destination) {
            Runner r; r.height=1.2f; r.beginAscent(destination==2?Runner::Citadel:(destination?Runner::Storm:Runner::Sky));
            for(int frame=0;frame<90;++frame) {
                const Scene s=makeScene(r,640,480);
                check(s.faces.size()<SceneFaceBudget,"Cinematic exceeded the geometry budget");
                for(const auto &face:s.faces) for(const auto &p:face.points)
                    check(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=-.01f&&p.x<=640.01f&&
                        p.y>=-.01f&&p.y<=480.01f,"Cinematic escaped its viewport");
                r.advance(1.f/15);
            }
        }
    });
    test("Near plane clipping is finite",[] {
        Scene s(800,600);
        const Vec3 c=s.eye+s.forward*.2f;
        s.polygon({c-s.right,c+s.right,c+s.forward*2+s.up},Tint(255,255,255),4,false);
        check(!s.faces.empty(),"Visible part of near-plane polygon disappeared");
        for(const auto &f:s.faces) for(const auto &p:f.points)
            check(std::isfinite(p.x)&&std::isfinite(p.y),"Invalid projection");
    });
    test("Scene is finite and clipped at all supported sizes",[] {
        const int sizes[][2]={{480,400},{1000,680},{1700,1000},{1,1}};
        for(const auto &size:sizes) for(int frame=0;frame<8;++frame) {
            Runner r; r.distance=frame*13.0f; r.height=(frame%4)*.9f; r.obstacleZ=36-frame*5;
            r.dragon=frame%2; r.fireTime=frame%2?.5f:0; r.evolutionTime=frame%3?0:1;
            r.burstTime=frame%2?.4f:0; r.burstZ=7; r.time=frame*.3f;
            if(frame%3==1) { r.dragon=true; r.stage=Runner::Sky; }
            if(frame%3==2) { r.beginAscent(); step(r,frame*.4f); }
            if(frame==4) { r.stage=Runner::Storm; r.dragon=true; r.stageTime=3; }
            if(frame==5) { r.beginAscent(Runner::Storm); step(r,2); }
            const Scene s=makeScene(r,size[0],size[1]);
            check(!s.faces.empty(),"Empty scene");
            for(const auto &f:s.faces) for(const auto &p:f.points)
                check(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=-.01f&&p.y>=-.01f&&
                    p.x<=size[0]+.01f&&p.y<=size[1]+.01f,"Polygon escaped playfield");
        }
    });
    test("Render does not change gameplay",[] {
        Runner r; r.jump(true); step(r,.3f); Runner before=r;
        makeScene(r,480,400); makeScene(r,1600,900);
        check(r.height==before.height && r.obstacleZ==before.obstacleZ && r.score==before.score,"Rendering changes physics");
    });
    test("Magic unlocks only after the second evolution",[] {
        Runner r; check(!r.castMagic(),"Desert spell unlocked");
        r.debugStage(Runner::Sky); check(!r.castMagic(),"Sky spell unlocked");
        r.beginAscent(Runner::Storm); step(r,1.8f);
        check(r.megaProgress()>0 && r.megaProgress()<1 && !r.castMagic(),"Evolution skipped its morph or input lock");
        step(r,2); check(r.megaProgress()==1 && r.castMagic(),"Storm spell remained locked");
    });
    test("Magic charges, travels, pierces an attack and hits only once",[] {
        Runner r; r.debugStage(Runner::Storm); r.bossShotCooldown=100;
        r.obstacleActive=true; r.obstacleZ=12; check(r.castMagic(),"Cast failed");
        step(r,.2f); check(r.magicZ==2.25f && r.bossHealth==8,"Spell hit during charge");
        step(r,.4f); check(!r.obstacleActive && r.bossHealth==8,"Travel or attack interception failed");
        step(r,.6f); check(r.bossHealth==6 && !r.over,"Beam missed, multi-hit, or failed to protect");
        check(!r.castMagic(),"Cooldown ignored");
        step(r,2.4f); check(r.castMagic(),"Cooldown never cleared");
    });
    test("Magic damage is frame independent and can win",[] {
        for(int rate=20;rate<=120;rate+=100) {
            Runner r; r.debugStage(Runner::Citadel); r.bossShotCooldown=100; r.bossHealth=2;
            r.castMagic(); step(r,1.2f,1.f/rate);
            check(r.won && !r.over && r.bossHealth==0 && !r.castMagic(),"Beam victory differs by frame rate");
            step(r,2); check(r.magicTime==0 && r.canRetry(),"Victory left a permanent beam");
        }
    });
    test("Magic excludes simultaneous breath and resets cleanly",[] {
        Runner r; r.debugStage(Runner::Storm); r.jump(true); r.castMagic();
        check(r.fireTime==0 && !r.jump(true) && !r.castMagic(),"Overlapping attacks accepted");
        r.reset(); check(r.magicTime==0 && r.magicCooldown==0 && !r.magicUnlocked(),"Reset retained spell state");
    });
    test("Developer shortcuts maintain valid stage and score state",[] {
        Runner r; r.best=123; r.debugStage(Runner::Storm); r.debugInvincibility();
        r.score=999; r.obstacleActive=true; r.obstacleZ=.1f; r.advance(.02f);
        check(!r.over && r.practice && r.best==123,"Cheat run collided or overwrote best");
        r.castMagic(); r.debugRecharge(); check(r.magicCooldown==0,"Debug recharge failed");
        r.debugStage(Runner::Sky); check(!r.invincible && r.magicTime==0 && r.pearls==5 && r.obstacleActive,"Stage warp retained state");
        r.debugEvolve(); check(r.stage==Runner::Ascending && r.ascentTarget==Runner::Storm && r.skyDestroyed==8,"Debug evolution skipped cinematic");
        r.debugStage(Runner::Desert); r.debugEvolve(); check(r.ascentTarget==Runner::Sky && r.pearls==5,"First evolution failed");
        r.reset(); check(!r.practice && !r.invincible && r.best==123,"Normal restart retained cheats");
    });
    test("Peak onset selects one action after a ramp to magic",[] {
        PeakInput p;
        check(p.sample(1.1f,1000,true)==0 && p.sample(1.9f,1050,true)==0 &&
            p.sample(2.7f,1100,true)==3,"Rising peak fired early or missed magic");
        check(p.sample(3,1400,true)==0 && p.sample(.8f,1500,true)==0 && p.sample(3,1700,true)==0,"Held input repeated");
        p.sample(.5f,1800,true); p.sample(1.2f,1900,true);
        check(p.sample(.4f,1950,true)==1,"Release lost a brief jump pulse");
    });
    test("Peak classifier respects stage, reset, hysteresis and tick wrap",[] {
        PeakInput p; p.sample(3,100,false); check(p.sample(3,200,false)==2,"Locked magic ate a strong jump");
        p.reset(); p.sample(1.9f,0xfffffff0u,true);
        check(p.sample(2,90,true)==2,"Tick wrap lost a peak");
        p.reset(); p.sample(3,500,true); p.sample(std::numeric_limits<float>::quiet_NaN(),550,true);
        check(!p.pending && !p.held,"Invalid source retained pending action");
    });
    test("Mega magic geometry remains finite throughout the cast",[] {
        Runner r; r.debugStage(Runner::Storm); r.stageTime=3; r.bossShotCooldown=100; r.castMagic();
        for(int frame=0;frame<24;++frame) {
            Scene s=makeScene(r,640,480); check(s.faces.size()<SceneFaceBudget,"Magic geometry budget exceeded");
            for(const auto &f:s.faces) for(const auto &p:f.points)
                check(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=-.01f&&p.x<=640.01f&&p.y>=-.01f&&p.y<=480.01f,"Magic escaped viewport");
            r.advance(.05f);
        }
    });
    test("Guardian death transfers power without ending the campaign",[] {
        for(int attack=0;attack<2;++attack) {
            Runner r; r.debugStage(Runner::Storm); r.bossHealth=1; r.bossShotCooldown=100;
            if(attack) r.castMagic(); else r.jump(true);
            for(int i=0;i<150 && r.stage==Runner::Storm;++i) r.advance(.01f);
            check(r.stage==Runner::Ascending && r.ascentTarget==Runner::Citadel && !r.won && !r.canRetry(),"Guardian death ended the game");
            check(!r.castLaser() && !r.castMagic() && !r.jump(false) && !r.obstacleActive,"Transfer accepted combat or spawned hazards");
            step(r,2.8f); check(r.divineProgress()>0 && r.divineProgress()<1,"Divine morph skipped");
            step(r,2.3f); check(r.stage==Runner::Citadel && r.bossHealth==20 && r.divineProgress()==1 && r.laserUnlocked() && !r.obstacleActive,"Demon entry failed");
        }
    });
    test("Solar laser locks to Stage 4, charges, sweeps and damages once",[] {
        Runner r; r.debugStage(Runner::Storm); check(!r.castLaser(),"Early laser unlocked");
        r.debugStage(Runner::Citadel); r.bossShotCooldown=100; r.obstacleActive=true; r.obstacleZ=18;
        check(r.castLaser() && !r.castLaser() && !r.castMagic() && !r.jump(true),"Laser did not exclude overlapping attacks");
        step(r,.5f); check(r.bossHealth==20 && r.obstacleActive,"Laser hit before charging");
        step(r,.2f); check(r.bossHealth==16 && !r.obstacleActive,"Laser missed boss or incoming attack");
        r.obstacleActive=true; r.obstacleZ=12; step(r,.5f);
        check(!r.obstacleActive && r.bossHealth==16,"Sustained beam did repeated damage or missed a new attack");
        step(r,.5f); check(!r.castLaser(),"Laser ignored cooldown");
        step(r,4.4f); check(r.castLaser(),"Laser did not recharge");
    });
    test("Laser victory and restart agree at 20 and 120 fps",[] {
        for(int rate=20;rate<=120;rate+=100) {
            Runner r; r.debugStage(Runner::Citadel); r.bossHealth=4; r.bossShotCooldown=100;
            r.castLaser(); step(r,1,1.f/rate);
            check(r.won && !r.over && r.bossHealth==0 && !r.castLaser(),"Laser victory failed");
            const float score=r.score; step(r,2); check(r.canRetry() && r.score==score && r.laserTime==0,"Victory retained laser or changed score");
            r.reset(); check(!r.laserUnlocked() && r.laserCooldown==0 && r.laserTime==0 && !r.laserHit && r.divineProgress()==0,"Reset retained divine powers");
        }
    });
    test("Charging the solar laser does not grant early invincibility",[] {
        Runner r; r.debugStage(Runner::Citadel); r.obstacleActive=true; r.obstacleZ=1;
        r.castLaser(); step(r,.2f);
        check(r.over && r.bossHealth==20 && !r.castLaser(),"Late charge granted invincibility or death accepted another cast");
    });
    test("Demon enrages and remains vulnerable to old skills",[] {
        Runner r; r.debugStage(Runner::Citadel); r.bossHealth=11; r.bossShotCooldown=100;
        r.jump(true); step(r,.02f); check(r.bossHealth==10,"Fire cannot hurt the demon");
        r.bossShotCooldown=0; step(r,.02f); check(r.obstacleActive && r.bossShotCooldown<=1.6f,"Demon did not enrage");
        r.obstacleActive=false; r.bossShotCooldown=100; step(r,1); r.castMagic(); step(r,1.1f);
        check(r.bossHealth==8,"Existing magic cannot hurt demon");
    });
    test("Divine developer warp and replay do not contaminate best",[] {
        Runner r; r.best=123; r.debugStage(Runner::Citadel); r.castLaser(); r.debugRecharge();
        check(r.practice && r.bossHealth==20 && r.laserCooldown==0,"Stage 4 debug state invalid");
        r.debugEvolve(); check(r.stage==Runner::Ascending && r.ascentTarget==Runner::Citadel && r.laserTime==0,"Divine replay retained a beam");
        step(r,5.1f); check(r.stage==Runner::Citadel && r.best==123,"Replay skipped or changed best");
        r.debugStage(Runner::Desert); check(!r.laserUnlocked() && r.laserTime==0,"Warp retained powers");
    });
    test("Fourth peak is one laser action and falls back before unlock",[] {
        PeakInput p; p.sample(1.1f,100,true,true); p.sample(2.7f,150,true,true);
        check(p.sample(3.5f,200,true,true)==4 && p.sample(4,500,true,true)==0,"Laser ramp repeated or fired magic first");
        p.reset(); p.sample(4,100,true,false); check(p.sample(4,200,true,false)==3,"Locked laser swallowed magic");
    });
    test("Divine form and solar seal stay clipped at all sizes",[] {
        for(int w: {1,480,1000}) {
            Runner r; r.debugStage(Runner::Citadel); r.stageTime=3; r.bossShotCooldown=100; r.castLaser();
            for(int frame=0;frame<32;++frame) {
                Scene s=makeScene(r,w,480); check(s.faces.size()<SceneFaceBudget,"Solar geometry exceeded budget");
                for(const auto &f:s.faces) for(const auto &p:f.points)
                    check(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=-.01f&&p.x<=w+.01f&&p.y>=-.01f&&p.y<=480.01f,"Solar geometry escaped viewport");
                r.advance(.05f);
            }
        }
    });
    test("Clipping interpolates vertex colors at the boundary",[] {
        std::vector<Scene::ShadedVertex> input={ {Vec3(-1,0,1),Tint(255,0,0)},
            {Vec3(1,0,1),Tint(0,0,255)},{Vec3(1,1,1),Tint(0,255,0)} };
        auto clipped=Scene::clipShaded(input,0,0,true); bool midpoint=false;
        for(const auto &v:clipped) if(std::fabs(v.p.x)<.001f && std::fabs(v.p.y)<.001f) {
            midpoint=std::fabs(v.c.r-127.5f)<.01f && std::fabs(v.c.b-127.5f)<.01f;
        }
        check(midpoint && clipped.size()==4,"Clipping lost the interpolated edge color");
    });
    test("Smooth normals rotate with the posed model",[] {
        Scene s(800,600); s.modelYaw=1.5707963f;
        const Vec3 n=s.rotateNormal(Vec3(0,0,1));
        check(std::fabs(n.x-1)<.0001f && std::fabs(n.z)<.0001f,"Pose rotated geometry but not lighting");
    });
    test("Smooth faces preserve finite lighting and cinematic blends",[] {
        Runner r; r.debugStage(Runner::Citadel); r.stageTime=3;
        Scene s=makeScene(r,640,480); size_t lit=0;
        for(const auto &f:s.faces) if(!f.colors.empty()) {
            ++lit; check(f.colors.size()==f.points.size(),"Clipped color count mismatch");
            for(const auto &c:f.colors) check(std::isfinite(c.r)&&std::isfinite(c.g)&&std::isfinite(c.b)&&
                c.r>=0&&c.r<=255&&c.g>=0&&c.g<=255&&c.b>=0&&c.b<=255,"Invalid lighting color");
        }
        check(lit>500,"Scene still uses flat shading only");
        transitionVeil(s,1);
        for(const auto &f:s.faces) for(const auto &c:f.colors)
            check(std::fabs(c.r-255)<.01f && std::fabs(c.g-243)<.01f,"Veil omitted vertex lighting");
    });
    test("Curved and hollow model meshes remain valid when posed and near-clipped",[] {
        for(int near=0;near<2;++near) {
            Scene s(640,480);
            s.meshDetail=2;
            s.setCamera(Vec3(0,.6f,near?-.2f:-3.f),Vec3(0,.6f,2),400);
            s.modelPitch=.4f; s.modelYaw=-.7f;
            BackyardBrains::ModelMesh::sweep(s,{{Vec3(-.5f,0,0),.15f},
                {Vec3(-.3f,.8f,.1f),.12f},{Vec3(.2f,1.2f,.2f),.06f},{Vec3(.6f,1.0f,.4f),.001f}},Tint(180,130,80));
            BackyardBrains::ModelMesh::hoop(s,Vec3(0,.2f,.5f),.6f,.035f,Tint(180,180,180));
            BackyardBrains::ModelMesh::roundedBox(s,Vec3(.4f,.4f,1),Vec3(.6f,.8f,.5f),.06f,Tint(60,80,100));
            check(s.faces.size()>20,"Model mesh culled away");
            for(const auto &f:s.faces) {
                check(f.points.size()==f.colors.size(),"Clipping lost model lighting");
                for(const auto &p:f.points) check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
                    p.x>=-.01f&&p.x<=640.01f&&p.y>=-.01f&&p.y<=480.01f&&p.z>0&&p.z<=3.334f,"Invalid posed model point");
                for(const auto &c:f.colors) check(std::isfinite(c.r)&&std::isfinite(c.g)&&std::isfinite(c.b),"Invalid curved mesh lighting");
            }
        }
    });
    test("Shared-vertex meshes match per-face lighting, pose and near clipping",[] {
        Scene indexed(200,160),reference(200,160);
        for(Scene *s: {&indexed,&reference}) {
            s->setCamera(Vec3(0,0,-1),Vec3(0,0,1),150);
            s->modelPitch=.2f; s->modelYaw=-.3f; s->modelScale=1.1f; s->modelTranslation=Vec3(.03f,.02f,0);
        }
        const std::vector<Vec3> points={Vec3(-1,-.5f,-.9f),Vec3(0,-.5f,0),Vec3(1,-.5f,.4f),
            Vec3(-1,.5f,-.9f),Vec3(0,.5f,0),Vec3(1,.5f,.4f)};
        const std::vector<Vec3> normals(6,Vec3(0,0,-1));
        const std::vector<std::array<size_t,4>> faces={{{0,3,4,1}},{{1,4,5,2}}};
        indexed.indexedQuads(points,normals,faces,Tint(120,160,80));
        for(const auto &q:faces) reference.smoothPolygon({points[q[0]],points[q[1]],points[q[2]],points[q[3]]},
            {normals[q[0]],normals[q[1]],normals[q[2]],normals[q[3]]},Tint(120,160,80));
        check(!indexed.faces.empty() && indexed.faces.size()==reference.faces.size(),"Indexed culling changed");
        for(size_t i=0;i<indexed.faces.size();++i) {
            const auto &a=indexed.faces[i],&b=reference.faces[i];
            check(a.points.size()==b.points.size(),"Indexed clipping changed");
            for(size_t j=0;j<a.points.size();++j) {
                const Vec3 delta=a.points[j]-b.points[j];
                check(dot(delta,delta)<.000001f && std::fabs(a.colors[j].r-b.colors[j].r)<.001f &&
                    std::fabs(a.colors[j].g-b.colors[j].g)<.001f,"Cached pose, depth or lighting differs");
            }
        }
    });
    Runner r; r.obstacleZ=7; r.pearlZ=7.6f; r.distance=17; exportFrame("tests/dino-ground.json",r);
    r.jump(true); step(r,.3f); exportFrame("tests/dino-jump.json",r);
    r.height=0; r.velocity=0; r.dragon=true; r.pearls=Runner::PearlsToEvolve; r.pearlActive=false;
    r.obstacleActive=false; r.fireTime=.5f; r.time=1; r.burstTime=.4f; r.burstZ=9;
    exportFrame("tests/dino-dragon.json",r);
    r.height=1.2f; r.beginAscent(); step(r,.9f); exportFrame("tests/dino-launch.json",r);
    step(r,1.7f); exportFrame("tests/dino-ascent.json",r);
    step(r,1.8f); r.obstacleZ=9; r.jump(true); step(r,.05f); exportFrame("tests/dino-sky.json",r);
    r.beginAscent(Runner::Storm); step(r,2); exportFrame("tests/dino-storm-ascent.json",r);
    step(r,2.2f); r.obstacleZ=8; r.obstacleActive=true; r.fireTime=0; r.stageTime=3;
    exportFrame("tests/dino-storm.json",r);
    r.obstacleActive=false; r.bossShotCooldown=100; r.castMagic(); step(r,.20f);
    exportFrame("tests/dino-magic-seal.json",r); step(r,.40f);
    exportFrame("tests/dino-magic-beam.json",r);
    r.debugStage(Runner::Storm); r.bossHealth=1; r.bossShotCooldown=100; r.jump(true); r.advance(.02f);
    step(r,1.6f); exportFrame("tests/dino-bestow.json",r);
    step(r,1.6f); exportFrame("tests/dino-divine-ascent.json",r);
    step(r,3); r.stageTime=3; r.obstacleActive=false; r.bossShotCooldown=100;
    exportFrame("tests/dino-demon.json",r); r.castLaser(); step(r,.45f);
    exportFrame("tests/dino-solar-seal.json",r); step(r,.4f); exportFrame("tests/dino-solar-laser.json",r);
    Scene portrait(1000,680);
    portrait.setCamera(Vec3(5,6.3f,5),Vec3(0,5.3f,16),720);
    portrait.fogStart=50; sky(portrait,1,1);
    r.debugStage(Runner::Citadel); r.time=1.1f; r.bossShotCooldown=.3f;
    demonKing(portrait,r); portrait.sort(); exportScene("tests/dino-demon-detail.json",portrait);
    for(int form=0;form<3;++form) {
        Runner model;
        if(form) model.debugStage(form==1?Runner::Sky:Runner::Citadel);
        model.height=0; model.distance=2; model.time=.35f;
        Scene detail(1000,680);
        detail.setCamera(Vec3(4,2.6f,1.8f),Vec3(0,1.1f,-.1f),form==0?590:(form==1?480:410));
        detail.fogStart=50; sky(detail,form?1.f:0.f,form==2?.45f:0);
        detail.ground(-6,6,-6,6,Tint(104,115,115),0);
        detail.shadow(0,0,.55f,1.7f,Tint(76,85,84));
        dinosaur(detail,model); detail.sort();
        exportScene(form==0?"tests/dino-character-detail.json":(form==1?"tests/dino-dragon-detail.json":"tests/dino-divine-detail.json"),detail);
    }
    std::cout << "RESULT " << passed << " passed, " << failed << " failed\n";
    return failed?1:0;
}
