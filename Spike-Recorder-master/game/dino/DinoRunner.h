#ifndef BYB_DINO_RUNNER_H
#define BYB_DINO_RUNNER_H
#include <algorithm>
#include <cmath>
namespace BackyardBrains { namespace Dino3D {
// Gameplay state in world units; rendering never advances the simulation.
struct Runner {
    enum { PearlsToEvolve=5, CrystalsToStorm=8, BossMaxHealth=8, DemonMaxHealth=20 };
    enum Stage { Desert, Ascending, Sky, Storm, Citadel };
    float height,velocity,obstacleZ,distance,score,best,time;
    int passed;
    bool over,longJump;
    float pearlZ,fireTime,fireCooldown,evolutionTime,pickupTime,burstTime,burstZ;
    int pearls,destroyed,skyDestroyed,bossHealth;
    bool dragon,pearlActive,obstacleActive,won,breathHitBoss;
    Stage stage,ascentTarget;
    float ascentTime,ascentStartHeight,stageTime,bossShotCooldown,bossHitTime,victoryTime;
    bool practice,invincible,magicHit;
    float magicTime,magicCooldown,magicZ,magicHeight;
    float laserTime,laserCooldown,laserHeight;
    bool laserHit;
    static float laserDuration() { return 1.5f; }
    static float laserCharge() { return .6f; }
    bool bossStage() const { return stage==Storm || stage==Citadel; }
    int bossMaxHealth() const { return stage==Citadel?DemonMaxHealth:BossMaxHealth; }
    bool laserUnlocked() const { return dragon && stage==Citadel; }
    float transitionDuration() const { return ascentTarget==Citadel?5.f:ascentDuration(); }
    float divineProgress() const {
        return stage==Citadel?1.f:(stage==Ascending && ascentTarget==Citadel?
            std::min(1.f,std::max(0.f,(ascentProgress()-.35f)/.45f)):0.f);
    }
    static float magicDuration() { return 1.15f; }
    static float magicCharge() { return .28f; }
    float megaProgress() const {
        return (bossStage() || (stage==Ascending && ascentTarget==Citadel))?1.f:(stage==Ascending && ascentTarget==Storm?
            std::min(1.f,std::max(0.f,(ascentProgress()-.18f)/.52f)):0.f);
    }
    bool magicUnlocked() const { return bossStage() && dragon; }
    void recordBest() { if(!practice) best=std::max(best,score); }
    static float ascentDuration() { return 3.6f; }
    static float pearlHeight() { return 2.45f; }
    static float fireRange() { return 14.f; }
    bool flyingStage() const { return stage==Sky || bossStage(); }
    bool canRetry() const { return over || (won && victoryTime>=1.8f); }
    float ascentProgress() const { return std::min(1.f,ascentTime/transitionDuration()); }
    Runner():best(0) { reset(); }
    void reset() {
        height=velocity=distance=score=time=0; obstacleZ=36; passed=0;
        over=longJump=dragon=won=breathHitBoss=false;
        pearlZ=obstacleZ+.6f; pearlActive=obstacleActive=true;
        fireTime=fireCooldown=evolutionTime=pickupTime=burstTime=burstZ=0;
        pearls=destroyed=skyDestroyed=0; bossHealth=BossMaxHealth;
        stage=Desert; ascentTarget=Sky;
        ascentTime=ascentStartHeight=stageTime=bossShotCooldown=bossHitTime=victoryTime=0;
        practice=invincible=magicHit=false;
        magicTime=magicCooldown=magicZ=magicHeight=0;
        laserTime=laserCooldown=laserHeight=0; laserHit=false;
    }
    // Explicit debug entry points; every altered run is excluded from best scores.
    void debugStage(Stage destination) {
        reset(); practice=true;
        if(destination==Desert) return;
        stage=destination==Citadel?Citadel:(destination==Storm?Storm:Sky); dragon=true; pearls=PearlsToEvolve;
        pearlActive=false; obstacleActive=stage==Sky; obstacleZ=42;
        skyDestroyed=bossStage()?CrystalsToStorm:0; bossShotCooldown=2; bossHealth=bossMaxHealth();
    }
    void debugEvolve() {
        if(over || won) debugStage(Desert);
        practice=true;
        if(stage==Ascending) return;
        if(stage==Citadel) debugStage(Storm);
        if(stage==Desert) pearls=PearlsToEvolve;
        else skyDestroyed=CrystalsToStorm;
        beginAscent(stage==Desert?Sky:(stage==Sky?Storm:Citadel));
    }
    void debugInvincibility() { practice=true; invincible=!invincible; }
    void debugRecharge() { practice=true; fireCooldown=magicCooldown=laserCooldown=0; }
    bool castMagic() {
        if(!magicUnlocked() || over || won || magicCooldown>0 || magicTime>0 || laserTime>0) return false;
        magicTime=magicDuration(); magicCooldown=3.5f; magicZ=2.25f;
        magicHeight=height+4.5f; magicHit=false; fireTime=0;
        return true;
    }
    bool castLaser() {
        if(!laserUnlocked() || over || won || laserCooldown>0 || laserTime>0 || magicTime>0) return false;
        laserTime=laserDuration(); laserCooldown=6.f; laserHeight=height+4.5f;
        laserHit=false; fireTime=0; return true;
    }
    void damageBoss(int damage) {
        bossHealth=std::max(0,bossHealth-damage); bossHitTime=.35f; score+=50*damage;
        if(bossHealth==0) {
            score+=250;
            if(stage==Storm) beginAscent(Citadel);
            else { won=true; obstacleActive=false; fireTime=0; victoryTime=0; }
            recordBest();
        }
    }
    void beginAscent(Stage destination=Sky) {
        dragon=true; stage=Ascending; ascentTarget=destination; ascentTime=0; ascentStartHeight=height;
        evolutionTime=2.4f; obstacleActive=pearlActive=false;
        fireTime=fireCooldown=burstTime=0; velocity=0; longJump=false; breathHitBoss=false;
        magicTime=magicCooldown=laserTime=laserCooldown=0; magicHit=laserHit=false;
        if(destination==Sky) ++passed;
    }
    bool jump(bool extended) {
        if(over || won || stage==Ascending) return false;
        if(dragon && extended) {
            if(fireCooldown>0 || magicTime>0 || laserTime>0) return false;
            fireTime=.70f; fireCooldown=1.f; breathHitBoss=false;
            return true;
        }
        if(height>.001f || velocity>0) return false;
        longJump=extended;
        velocity=flyingStage()?6.6f:(extended?10.1f:7.6f);
        return true;
    }
    float speed() const { return 10.f+std::min(5.f,score*.025f); }
    float obstacleHeight() const { return bossStage()?.95f:(passed%3==2?.72f:1.f); }
    void advance(float seconds) {
        if(over || !std::isfinite(seconds) || seconds<=0) return;
        float remaining=std::min(seconds,.1f);
        if(won) { time+=remaining; victoryTime+=remaining;
            magicTime=std::max(0.f,magicTime-remaining); magicZ+=24*remaining; laserTime=std::max(0.f,laserTime-remaining); return; }
        while(remaining>.000001f && !over && !won) {
            const float dt=std::min(remaining,1.f/240); remaining-=dt;
            const float movement=speed()*dt;
            distance+=movement; time+=dt; score+=dt*10;
            fireTime=std::max(0.f,fireTime-dt); fireCooldown=std::max(0.f,fireCooldown-dt);
            magicTime=std::max(0.f,magicTime-dt); magicCooldown=std::max(0.f,magicCooldown-dt);
            laserTime=std::max(0.f,laserTime-dt); laserCooldown=std::max(0.f,laserCooldown-dt);
            evolutionTime=std::max(0.f,evolutionTime-dt); pickupTime=std::max(0.f,pickupTime-dt);
            burstTime=std::max(0.f,burstTime-dt); bossHitTime=std::max(0.f,bossHitTime-dt); burstZ-=movement;
            if(stage==Ascending) {
                ascentTime=std::min(transitionDuration(),ascentTime+dt);
                const float p=ascentProgress(); height=ascentStartHeight+(8-ascentStartHeight)*p*p*(3-2*p);
                if(ascentTime>=transitionDuration()) {
                    stage=ascentTarget; stageTime=0; height=velocity=0;
                    obstacleZ=bossStage()?24.f:42.f; obstacleActive=!bossStage(); pearlActive=false;
                    fireTime=fireCooldown=pickupTime=burstTime=0;
                    if(bossStage()) { bossHealth=bossMaxHealth(); bossShotCooldown=2.f; }
                }
                recordBest(); continue;
            }
            if(flyingStage()) stageTime+=dt;
            if(bossStage()) {
                bossShotCooldown=std::max(0.f,bossShotCooldown-dt);
                if(!obstacleActive && bossShotCooldown==0) {
                    obstacleZ=24; obstacleActive=true; bossShotCooldown=stage==Citadel?(bossHealth<=DemonMaxHealth/2?1.6f:2.1f):2.7f;
                }
            }
            if(height>0 || velocity>0) {
                const float gravity=flyingStage()?16.f:(longJump?20.f:22.f);
                height+=velocity*dt-.5f*gravity*dt*dt; velocity-=gravity*dt;
                if(height<=0) { height=velocity=0; longJump=false; }
            }
            if(!bossStage() || obstacleActive) obstacleZ-=movement;
            pearlZ-=movement;
            // Charge at the forward seal, then sweep a travelling beam toward the boss.
            // Swept intervals prevent missed hits even at the slowest frame rate.
            if(bossStage() && magicTime>0 && magicDuration()-magicTime>=magicCharge()) {
                const float previous=magicZ;
                magicZ=2.25f+std::max(0.f,magicDuration()-magicTime-magicCharge())*24.f;
                if(obstacleActive && obstacleZ>=previous-4-movement && obstacleZ<=magicZ+.5f) {
                    obstacleActive=false; ++destroyed; score+=10; burstTime=.65f; burstZ=obstacleZ;
                }
                if(!magicHit && magicZ>=16) { magicHit=true; damageBoss(2); }
                if(won) break;
                if(stage==Ascending) continue;
            }
            if(stage==Citadel && laserTime>0 && laserDuration()-laserTime>=laserCharge()) {
                if(obstacleActive && obstacleZ>=-.3f && obstacleZ<=24) {
                    obstacleActive=false; ++destroyed; score+=10; burstTime=.65f; burstZ=obstacleZ;
                }
                if(!laserHit) { laserHit=true; damageBoss(4); }
                if(won) break;
            }
            // A breath stops at the incoming attack; it cannot hit the boss too.
            if(dragon && fireTime>0 && (!bossStage() || !breathHitBoss) && obstacleActive && obstacleZ>=-.3f && obstacleZ<=fireRange()) {
                obstacleActive=false; ++destroyed; score+=10; burstTime=.65f; burstZ=obstacleZ;
                if(bossStage()) { breathHitBoss=true; fireTime=std::min(fireTime,.35f); }
                if(stage==Sky && ++skyDestroyed>=CrystalsToStorm) {
                    beginAscent(Storm); recordBest(); continue;
                }
            }
            // One hit per deliberate breath, never one hit per rendered frame.
            if(bossStage() && fireTime>0 && !breathHitBoss && (!obstacleActive || obstacleZ>fireRange())) {
                breathHitBoss=true; damageBoss(1);
                if(won) break;
                if(stage==Ascending) continue;
            }
            if(!invincible && obstacleActive && std::fabs(obstacleZ)<.62f && height<obstacleHeight()-.10f) over=true;
            if(!over && pearlActive && std::fabs(pearlZ)<.85f && height>.55f &&
                height<pearlHeight()+.25f && height+1.9f>pearlHeight()-.25f) {
                pearlActive=false; ++pearls; score+=25; pickupTime=.65f;
                if(pearls>=PearlsToEvolve) { beginAscent(); recordBest(); continue; }
            }
            if(pearlZ<-3.5f) pearlActive=false;
            if(obstacleActive && obstacleZ<-3.5f && bossStage()) {
                obstacleActive=false; ++passed;
            } else if(!bossStage() && obstacleZ<-3.5f) {
                ++passed; obstacleZ=30.f+static_cast<float>((passed*7)%11); obstacleActive=true;
                pearlZ=obstacleZ+.6f; pearlActive=!dragon;
            }
            recordBest();
        }
    }
};
} }
#endif
