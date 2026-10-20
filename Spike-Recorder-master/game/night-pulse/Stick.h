#ifndef NIGHT_PULSE_STICK_H
#define NIGHT_PULSE_STICK_H
#include "Motion.h"
#include "../../src/games/ModelMesh.h"
namespace NightPulse {
// One continuous turned-wood surface: rounded butt, straight grip, long tapered
// shoulder, narrow neck and an oval wooden tip. Values run from butt to tip.
struct StickSection { float length,radius; };
inline void drawStick(Scene &scene,const StickPose &pose,int hand) {
    static const StickSection source[]={
        {0,0},{.006f,.015f},{.018f,.026f},{.040f,.032f},
        {.29f,.032f},{.83f,.0315f},{1.12f,.030f},{1.34f,.025f},
        {1.48f,.018f},{1.56f,.0125f},{1.61f,.012f},{1.635f,.019f},
        {1.665f,.025f},{1.705f,.026f},{1.743f,.020f},{1.763f,.010f},{1.77f,0}
    };
    const int count=sizeof(source)/sizeof(source[0]),segments=32;
    std::vector<StickSection> profile;
    for(int i=0;i<count-1;++i) for(int j=0;j<2;++j) {
        const auto &a=source[std::max(0,i-1)],&b=source[i],&c=source[i+1],&d=source[std::min(count-1,i+2)];
        const float t=j*.5f;
        const float r=BackyardBrains::ModelMesh::curve(a.radius,b.radius,c.radius,d.radius,t);
        profile.push_back({b.length+(c.length-b.length)*t,std::max(std::min(b.radius,c.radius),std::min(std::max(b.radius,c.radius),r))});
    }
    profile.push_back(source[count-1]);
    const int rings=static_cast<int>(profile.size());
    const Vec3 axis=unit(pose.tip-pose.butt);
    const Vec3 u=unit(cross(axis,std::fabs(axis.y)>.95f?Vec3(1,0,0):Vec3(0,1,0)));
    const Vec3 v=cross(axis,u);
    const float oldGloss=scene.gloss; scene.gloss=.16f;
    std::vector<Vec3> points,normals; std::vector<Tint> tints;
    for(int row=0;row<rings;++row) for(int col=0;col<segments;++col) {
        const int before=std::max(0,row-1),after=std::min(rings-1,row+1);
        const float angle=col*6.2831853f/segments;
        const Vec3 radial=u*std::cos(angle)+v*std::sin(angle);
        const float slope=(profile[after].radius-profile[before].radius)/(profile[after].length-profile[before].length);
        points.push_back(pose.butt+axis*profile[row].length+radial*profile[row].radius);
        normals.push_back(row==0?axis*-1:(row==rings-1?axis:unit(radial-axis*slope)));
        const float along=profile[row].length;
        const float grain=1+.018f*std::sin(angle*5+.22f*std::sin(along*4+hand))+
            .009f*std::sin(angle*11+along*.65f);
        tints.push_back(Tint(211,177,122).shade(grain));
    }
    BackyardBrains::ModelMesh::ringMesh(scene,points,normals,segments,Tint(211,177,122),false,false,&tints);
    scene.gloss=oldGloss;
}
}
#endif
