#ifndef NIGHT_PULSE_MOTION_H
#define NIGHT_PULSE_MOTION_H
#include "../../src/games/Scene3D.h"
#include "Rhythm.h"
namespace NightPulse {
using namespace BackyardBrains::Scene3D;
struct Target {
    Vec3 position;
    float radius;
    int instrument;
};
inline Target noteTarget(int index) {
    if(index>=0 && index%8==7) return {Vec3(1.75f,1.65f,1.25f),.52f,2};
    if(index>=0 && index%8==3) return {Vec3(-.85f,1.22f,1.35f),.50f,1};
    return {Vec3(0,1.0f,0),.72f,0};
}
inline float smooth(float value) {
    const float t=std::max(0.f,std::min(1.f,value));
    return t*t*(3-2*t);
}
inline float instrumentAge(const Rhythm &r,int instrument) {
    return static_cast<float>(r.elapsed-r.instrumentAt[instrument]);
}
inline float membraneDip(const Rhythm &r,int instrument) {
    const float t=instrumentAge(r,instrument);
    return t>=0 && t<.32f?-.025f*r.instrumentPower[instrument]*std::exp(-18*t)*std::cos(65*t):0;
}
inline float cymbalAngle(const Rhythm &r) {
    const float t=instrumentAge(r,2);
    return t>=0 && t<1.5f?.19f*r.instrumentPower[2]*std::exp(-3.5f*t)*std::sin(31*t):0;
}
struct StickPose { Vec3 grip,tip,butt; };
inline Vec3 liftDirection(Vec3 direction,float angle) {
    const Vec3 up(0,1,0);
    const Vec3 tangent=unit(up-direction*dot(direction,up));
    return unit(direction*std::cos(angle)+tangent*std::sin(angle));
}
inline StickPose stickPose(const Rhythm &r,int hand) {
    const int side=hand?1:-1;
    const Vec3 rest(side*.30f,1.83f,.04f);
    const float age=static_cast<float>(r.elapsed-r.stickAt[hand]);
    const Vec3 anchor(side*.82f,.78f,-.92f);
    const Vec3 restingDirection=unit(rest-anchor);
    const Vec3 restingGrip=rest-restingDirection*1.48f;
    Vec3 grip=restingGrip,direction=restingDirection;
    if(r.started && !r.finished) {
        // Readable preparation before a note, but never an automatic strike.
        for(size_t i=0;i<r.notes.size();++i) if(!r.notes[i].judged) {
            const float until=static_cast<float>(r.notes[i].time-r.elapsed);
            if(until<-.20f) continue;
            if(until>.24f) break;
            const Target t=noteTarget(static_cast<int>(i));
            const int nextHand=t.instrument==2?1:(t.instrument==1?0:r.strokes%2);
            if(nextHand==hand) {
                const Vec3 contact=t.position+Vec3(side*.12f,.048f,0);
                const Vec3 contactDirection=unit(contact-anchor);
                const Vec3 contactGrip=contact-contactDirection*1.48f;
                const float preparation=until>=0?smooth((.24f-until)/.24f):1-smooth(-until/.20f);
                grip=restingGrip+(contactGrip-restingGrip)*preparation;
                direction=unit(restingDirection+(liftDirection(contactDirection,.62f)-restingDirection)*preparation);
            }
            break;
        }
    }
    if(age>=0 && age<.42f) {
        const Target t=noteTarget(r.stickNote[hand]);
        const Vec3 contact=t.position+Vec3(side*.12f,.048f,0);
        const Vec3 contactDirection=unit(contact-anchor);
        const Vec3 contactGrip=contact-contactDirection*1.48f;
        const Vec3 raisedDirection=liftDirection(contactDirection,.70f*r.stickPower[hand]);
        // Immediate contact keeps sound, input and judgement aligned. Recover
        // into the next preparation pose so consecutive same-hand hits flow.
        if(age<.09f) {
            direction=liftDirection(contactDirection,.70f*r.stickPower[hand]*std::sin(age/.09f*1.5707963f));
            grip=contactGrip;
        } else {
            const float recovery=smooth((age-.09f)/.33f);
            grip=contactGrip+(grip-contactGrip)*recovery;
            direction=unit(raisedDirection+(direction-raisedDirection)*recovery);
        }
    }
    // Constant-length sticks rotate at the grip instead of stretching to reach
    // distant toms. Moving the wrist forward supplies the reach.
    return {grip,grip+direction*1.48f,grip-direction*.29f};
}
inline Vec3 cameraImpulse(const Rhythm &r) {
    Vec3 result;
    for(int i=0;i<3;++i) {
        const float t=instrumentAge(r,i);
        if(t<0 || t>.23f) continue;
        const float kick=r.instrumentPower[i]*std::exp(-22*t)*std::sin(42*t);
        result=result+Vec3((i==2?1:-1)*.008f*kick,-.035f*kick,.016f*kick);
    }
    return result;
}
}
#endif
