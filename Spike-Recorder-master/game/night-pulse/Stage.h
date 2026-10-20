#ifndef NIGHT_PULSE_STAGE_H
#define NIGHT_PULSE_STAGE_H
#include "../../src/games/Scene3D.h"
#include "Motion.h"
#include "Stick.h"
#include "../../src/games/ModelMesh.h"
namespace NightPulse {
using namespace BackyardBrains::Scene3D;
namespace Mesh=BackyardBrains::ModelMesh;
inline void drum(Scene &s,Vec3 c,float radius,float depth,float dip=0) {
    const float oldGloss=s.gloss; s.gloss=.28f;
    const Tint shell(160,105,64),rim(157,162,158),skin(209,199,176);
    Mesh::revolve(s,c,Vec3(0,1,0),{{radius*.92f,0},{radius,0},{radius*1.006f,-.04f},
        {radius*.99f,-depth*.5f},{radius,-depth+.03f},{radius*.98f,-depth},{radius*.92f,-depth}},shell,32,true);
    Mesh::hoop(s,c+Vec3(0,.014f,0),radius*1.015f,.025f,rim);
    Mesh::hoop(s,c-Vec3(0,depth-.015f,0),radius*1.01f,.020f,rim.shade(.9f));
    // The skin flexes independently of the solid shell and hoop.
    const float head=radius*.98f;
    for(int row=0;row<6;++row) for(int i=0;i<64;++i) {
        std::vector<Vec3> points,normals;
        const int rows[]={row,row+1,row+1,row},cols[]={i,i,i+1,i+1};
        for(int j=0;j<4;++j) {
            const float rr=rows[j]/6.f,a=-cols[j]*6.2831853f/64;
            const float x=std::cos(a)*rr,z=std::sin(a)*rr;
            points.push_back(c+Vec3(x*head,.033f+dip*(1-rr*rr),z*head));
            normals.push_back(unit(Vec3(2*dip*x/head,1,2*dip*z/head)));
        }
        s.smoothPolygon(points,normals,skin);
    }
    const int lugs=radius>.65f?8:6;
    for(int i=0;i<lugs;++i) {
        const float a=i*6.2831853f/lugs;
        const Vec3 radial(std::cos(a),0,std::sin(a)),p=radial*radius*1.02f;
        s.ellipsoid(c+p-Vec3(0,depth*.50f,0),Vec3(.043f,.075f,.043f),rim,8,4);
        s.link(c+p-Vec3(0,depth*.5f,0),c+p+Vec3(0,.015f,0),.011f,.011f,rim,6);
        s.link(c+p+Vec3(0,.008f,0),c+p+Vec3(0,.035f,0),.022f,.022f,rim,6);
    }
    s.gloss=oldGloss;
}
inline void ring(Scene &s,Vec3 c,float radius,float thickness,Tint tint);
inline float cymbalHeight(float r) {
    if(r<.15f) return .073f-(r-.10f)*.54f;
    if(r<.21f) return .046f-(r-.15f)*.566667f;
    if(r<.36f) return .012f-(r-.21f)*.113333f;
    return -.005f-(r-.36f)*.125f;
}
inline void cymbal(Scene &s,float x,float y,float z,float tilt=0,bool hiHat=false) {
    const float oldGloss=s.gloss; s.gloss=.55f;
    const Tint metal(110,117,119),brass(196,165,91);
    s.link(Vec3(x,.08f,z),Vec3(x,y+.08f,z),.025f,.018f,metal,6);
    for(int i=0;i<3;++i) {
        const float a=i*6.2831853f/3;
        s.link(Vec3(x,.28f,z),Vec3(x+std::cos(a)*.36f,.04f,z+std::sin(a)*.36f),.02f,.02f,metal,5);
    }
    // Only the suspended metal tilts; its stand remains planted on the floor.
    const float oldPitch=s.modelPitch; const Vec3 oldOrigin=s.modelOrigin;
    s.modelOrigin=Vec3(x,y,z); s.modelPitch=tilt;
    std::vector<Mesh::ProfilePoint> profile={{.012f,.084f},{.055f,.082f},{.10f,.073f},{.15f,.046f},
        {.21f,.012f},{.36f,-.005f},{.48f,-.020f},{.52f,-.025f},{.522f,-.031f},
        {.48f,-.036f},{.36f,-.018f},{.21f,-.001f},{.14f,.034f},{.09f,.060f},{.012f,.067f}};
    if(hiHat) for(auto &p:profile) p.radius*=.88f;
    Mesh::revolve(s,Vec3(x,y,z),Vec3(0,1,0),profile,brass,32);
    if(hiHat) {
        std::vector<Mesh::ProfilePoint> bottom=profile;
        for(auto &p:bottom) p.height=-p.height-.075f;
        std::reverse(bottom.begin(),bottom.end());
        Mesh::revolve(s,Vec3(x,y,z),Vec3(0,1,0),bottom,brass.shade(.86f),24);
    }
    for(int i=0;i<7;++i) {
        const float r=.19f+i*.046f;
        ring(s,Vec3(x,y+cymbalHeight(r)+.002f,z),r*(hiHat?.88f:1.f),.004f,Tint(164,137,77));
    }
    s.link(Vec3(x,y+.083f,z),Vec3(x,y+.11f,z),.043f,.036f,Tint(43,43,39),10);
    s.ellipsoid(Vec3(x,y+.12f,z),Vec3(.057f,.014f,.018f),metal,8,4);
    s.modelPitch=oldPitch; s.modelOrigin=oldOrigin;
    s.gloss=oldGloss;
}
inline void ring(Scene &s,Vec3 c,float radius,float thickness,Tint tint) {
    for(int i=0;i<40;++i) {
        const float a=i*6.2831853f/40,b=(i+1)*6.2831853f/40;
        const Vec3 u(std::cos(a),0,std::sin(a)),v(std::cos(b),0,std::sin(b));
        const size_t before=s.faces.size();
        s.polygon({c+u*radius,c+v*radius,c+v*(radius-thickness),c+u*(radius-thickness)},tint,4,false);
        // Coplanar decoration must sort as one surface, not as independent
        // far/near strips that the drum head would partially cover.
        if(s.faces.size()>before) s.faces.back().depth=s.camera(c).z-.015f;
    }
}
inline Scene makeStage(const Rhythm &r,int width,int height) {
    Scene s(width,height);
    s.meshDetail=2;
    // Player's seated view, looking down into the kit; there is no avatar.
    const Vec3 kick=cameraImpulse(r);
    s.setCamera(Vec3(0,3.5f,-2.8f)+kick,Vec3(0,.85f,2.4f)+kick*.25f,std::min(width*.66f,height*1.12f));
    s.centerY=height*.30f;
    s.fogColor=Tint(23,25,27); s.fogStart=10; s.fogRange=24;
    const Tint wood(66,59,49),metal(116,123,125),black(25,29,31);
    for(int i=0;i<28;++i) {
        const float x=-7+i*.5f;
        s.ground(x,x+.493f,-3,16,wood.shade(.96f+(i%4)*.015f));
    }
    s.box(Vec3(0,2.3f,13),Vec3(14,4.6f,.2f),Tint(33,36,38));
    for(int i=0;i<12;++i) s.box(Vec3(-6+i*1.1f,2.1f,12.8f),Vec3(.78f,3.5f,.1f),black);
    s.box(Vec3(0,3.6f,12.6f),Vec3(8,.045f,.05f),Tint(161,143,108));
    // A rug and stands anchor the close-up instruments to the floor.
    s.ground(-2.7f,2.7f,-1.8f,3.2f,Tint(33,37,38),1);
    for(int side=-1;side<=1;side+=2) {
        const float x=side*3.5f;
        Mesh::roundedBox(s,Vec3(x,.65f,5.2f),Vec3(.7f,1.3f,.6f),.06f,black);
        for(int i=0;i<2;++i) {
            s.link(Vec3(x,.36f+i*.56f,4.90f),Vec3(x,.36f+i*.56f,4.87f),.22f,.22f,metal,20);
            s.link(Vec3(x,.36f+i*.56f,4.865f),Vec3(x,.36f+i*.56f,4.84f),.16f,.11f,black,20);
            s.ellipsoid(Vec3(x,.36f+i*.56f,4.83f),Vec3(.055f,.055f,.03f),Tint(56,61,62),10,5);
        }
    }
    // Kick is partially hidden under the rack toms, as from a drummer's seat.
    s.link(Vec3(0,.57f,2.3f),Vec3(0,.57f,1.4f),.57f,.57f,Tint(108,64,45),24);
    s.link(Vec3(0,.57f,1.39f),Vec3(0,.57f,1.37f),.52f,.52f,Tint(51,54,52),24);
    Mesh::revolve(s,Vec3(0,.57f,1.36f),Vec3(0,0,-1),{{.50f,0},{.52f,.026f},{.56f,.024f},{.58f,0},{.56f,-.025f},{.52f,-.025f}},metal,32,true);
    s.link(Vec3(.19f,.40f,1.35f),Vec3(.19f,.40f,1.33f),.105f,.105f,black,18);
    s.link(Vec3(0,.05f,.9f),Vec3(0,.55f,1.38f),.035f,.035f,metal,6);
    s.box(Vec3(0,.08f,.64f),Vec3(.15f,.10f,.50f),metal);
    s.link(Vec3(0,.62f,1.7f),Vec3(0,1.02f,1.7f),.043f,.035f,metal,10);
    s.link(Vec3(-.85f,1.02f,1.35f),Vec3(.85f,1.02f,1.35f),.033f,.033f,metal,10);
    for(int side=-1;side<=1;side+=2) {
        s.link(Vec3(side*.85f,.1f,1.35f),Vec3(side*.85f,1.0f,1.35f),.035f,.035f,metal,8);
        drum(s,Vec3(side*.85f,1.22f,1.35f),.50f,.42f,side<0?membraneDip(r,1):0);
    }
    drum(s,Vec3(1.48f,.84f,.05f),.62f,.60f);
    for(int i=0;i<3;++i) {
        const float a=i*6.2831853f/3;
        const Vec3 p=Vec3(1.48f,.42f,.05f)+Vec3(std::cos(a)*.58f,0,std::sin(a)*.58f);
        Mesh::sweep(s,{{p,.022f},{p+Vec3(0,-.20f,0),.022f},{p+Vec3(std::cos(a)*.09f,-.37f,std::sin(a)*.09f),.022f}},metal,8);
    }
    cymbal(s,-1.7f,1.45f,.45f,0,true);
    cymbal(s,1.75f,1.65f,1.25f,cymbalAngle(r));
    drum(s,Vec3(0,1.0f,0),.72f,.32f,membraneDip(r,0));
    for(int side=-1;side<=1;side+=2)
        s.link(Vec3(side*.18f,.70f,0),Vec3(side*.48f,.05f,side*.2f),.027f,.027f,metal,6);
    // Each note arrives at its instrument. One signal still plays every drum.
    for(int instrument=0;instrument<3;++instrument) {
        const Target t=noteTarget(instrument==0?0:(instrument==1?3:7));
        if(instrument==2) { s.modelOrigin=t.position; s.modelPitch=cymbalAngle(r); }
        const float dip=instrument==2?0:membraneDip(r,instrument)*.45f;
        ring(s,t.position+Vec3(0,.045f+dip,0),t.instrument==2?.36f:t.radius*.67f,.018f,Tint(187,197,186));
        const float age=instrumentAge(r,instrument);
        if(age>=0 && age<.24f) {
            const float fade=1-age/.24f;
            ring(s,t.position+Vec3(0,.058f,0),t.radius*(.20f+.65f*(1-fade)),.035f*fade,
                Tint(187+40*fade,178+35*fade,140+25*fade));
        }
        s.modelPitch=0; s.modelOrigin=Vec3();
    }
    for(size_t i=0;i<r.notes.size();++i) {
        if(!r.started || r.notes[i].judged) continue;
        const float remaining=r.notes[i].time-static_cast<float>(r.elapsed);
        if(remaining>Travel || remaining< -HitWindow) continue;
        const Target t=noteTarget(static_cast<int>(i));
        const float travel=std::max(0.f,remaining/Travel);
        const float lift=travel*.75f+.35f*std::sin(std::min(1.f,travel)*3.14159265f);
        const Vec3 c(t.position.x*(1-travel*.65f),t.position.y+.08f+lift,t.position.z+remaining*4.3f);
        s.box(c,Vec3(.54f,.075f,.18f),t.instrument==2?Tint(217,188,124):Tint(144,190,179));
        s.box(c+Vec3(0,.041f,0),Vec3(.18f,.008f,.14f),Tint(225,230,211));
    }
    for(int hand=0;hand<2;++hand) {
        const StickPose pose=stickPose(r,hand);
        const float age=static_cast<float>(r.elapsed-r.stickAt[hand]);
        // Brief narrow sweep lines show the fast rebound without obscuring notes.
        if(age>.012f && age<.10f) for(int j=1;j<=2;++j) {
            Rhythm previous=r; previous.elapsed-=j*.009f;
            const StickPose trail=stickPose(previous,hand);
            s.link(trail.grip+(trail.tip-trail.grip)*.60f,trail.tip,.007f,.003f,Tint(132,117,88),5);
        }
        drawStick(s,pose,hand);
    }
    s.sort(); return s;
}
}
#endif
