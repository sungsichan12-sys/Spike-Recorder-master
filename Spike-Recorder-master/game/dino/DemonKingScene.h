#ifndef BYB_DEMON_KING_SCENE_H
#define BYB_DEMON_KING_SCENE_H
#include "DinoRunner.h"
#include "games/Scene3D.h"
#include "games/ModelMesh.h"
namespace BackyardBrains { namespace Dino3D {
using namespace Scene3D;

// Sculpted rings, bevelled plates and continuous curved horns keep the boss
// model independent from the dragon model and from combat simulation.
struct DemonSlice {
    Vec3 c; float x,z;
    DemonSlice(Vec3 center,float width,float depth):c(center),x(width),z(depth) {}
};
inline void demonLoft(Scene &s,const std::vector<DemonSlice> &source,Tint color,int sides=14) {
    if(source.size()<2) return;
    const int samples=s.meshDetail>1?3:2;
    if(s.meshDetail>1) sides=std::max(sides,24);
    std::vector<DemonSlice> sections;
    for(size_t i=0;i+1<source.size();++i) {
        const DemonSlice &a=source[i?i-1:i],&b=source[i],&c=source[i+1],&d=source[std::min(i+2,source.size()-1)];
        for(int j=0;j<samples;++j) {
            const float t=j/static_cast<float>(samples);
            sections.push_back(DemonSlice(ModelMesh::curve(a.c,b.c,c.c,d.c,t),
                std::max(.003f,ModelMesh::curve(a.x,b.x,c.x,d.x,t)),std::max(.003f,ModelMesh::curve(a.z,b.z,c.z,d.z,t))));
        }
    }
    sections.push_back(source.back());
    std::vector<Vec3> points,vertexNormals;
    for(size_t row=0;row<sections.size();++row) {
        std::vector<Vec3> current,normals;
        const DemonSlice &before=sections[row?row-1:row],&after=sections[std::min(row+1,sections.size()-1)];
        for(int i=0;i<sides;++i) {
            const float a=(i+.5f)*6.2831853f/sides;
            current.push_back(sections[row].c+Vec3(std::cos(a)*sections[row].x,0,std::sin(a)*sections[row].z));
            const Vec3 along=after.c-before.c+Vec3(std::cos(a)*(after.x-before.x),0,std::sin(a)*(after.z-before.z));
            normals.push_back(unit(cross(along,Vec3(-std::sin(a)*sections[row].x,0,std::cos(a)*sections[row].z))));
        }
        if(row==0) s.polygon(current,color);
        if(row+1==sections.size()) {
            std::vector<Vec3> cap=current; std::reverse(cap.begin(),cap.end()); s.polygon(cap,color);
        }
        points.insert(points.end(),current.begin(),current.end()); vertexNormals.insert(vertexNormals.end(),normals.begin(),normals.end());
    }
    ModelMesh::ringMesh(s,points,vertexNormals,sides,color,true);
}
inline Vec3 demonBezier(Vec3 a,Vec3 b,Vec3 c,Vec3 d,float t) {
    const float u=1-t;
    return a*(u*u*u)+b*(3*u*u*t)+c*(3*u*t*t)+d*(t*t*t);
}
inline void demonHorn(Scene &s,Vec3 a,Vec3 b,Vec3 c,Vec3 d,float radius,Tint color,int steps=12) {
    if(s.meshDetail>1 && radius>.04f) steps=steps*3/2;
    std::vector<ModelMesh::SweepPoint> path;
    for(int j=0;j<=steps;++j) {
        const float t=j/static_cast<float>(steps);
        const Vec3 center=demonBezier(a,b,c,d,t);
        path.push_back(ModelMesh::SweepPoint(center,radius*std::pow(1-t,.85f)+.0008f));
    }
    ModelMesh::sweep(s,path,color,radius>.12f?12:8);
}
inline void demonPlate(Scene &s,const std::vector<Vec3> &rim,float bevel,Tint metal,Tint trim) {
    if(rim.size()<3) return;
    Vec3 center;
    for(const auto &p:rim) center=center+p;
    center=center*(1.f/rim.size());
    for(size_t i=0;i<rim.size();++i) {
        const Vec3 a=rim[i],b=rim[(i+1)%rim.size()];
        const Vec3 innerA=center+(a-center)*.89f-Vec3(0,0,bevel*.55f);
        const Vec3 innerB=center+(b-center)*.89f-Vec3(0,0,bevel*.55f);
        const Vec3 na=unit(Vec3((a.x-center.x)*.38f,(a.y-center.y)*.38f,-1));
        const Vec3 nb=unit(Vec3((b.x-center.x)*.38f,(b.y-center.y)*.38f,-1));
        const int divisions=s.meshDetail>1?3:1;
        for(int row=0;row<divisions;++row) for(int col=0;col<divisions;++col) {
            std::vector<Vec3> points,normals;
            const int rs[]={row,row+1,row+1,row},cs[]={col,col,col+1,col+1};
            for(int k=0;k<4;++k) {
                const float radial=rs[k]/static_cast<float>(divisions),edge=cs[k]/static_cast<float>(divisions);
                const Vec3 border=innerA*(1-edge)+innerB*edge;
                Vec3 p=center+(border-center)*radial;
                p.z=(center.z-bevel)*(1-radial*radial)+border.z*radial*radial;
                points.push_back(p); normals.push_back(unit(Vec3((p.x-center.x)*.38f,(p.y-center.y)*.38f,-1)));
            }
            s.smoothPolygon(points,normals,metal,4,true);
        }
        // A narrow bevel and a dark side wall give the plate real thickness.
        // The old cylindrical outline read as a wire drawn over flat cardboard.
        s.smoothPolygon({a,b,innerB,innerA},{na,nb,nb,na},trim.shade(.82f),4,true);
        s.polygon({a,a+Vec3(0,0,.065f),b+Vec3(0,0,.065f),b},metal.shade(.45f),4,false);
    }
}
inline Vec3 capePoint(float u,float t,float time,float y,float z) {
    const float hem=1.7f+.32f*std::cos(u*11)+.18f*std::sin(u*27);
    return Vec3((u-.5f)*(1.6f+2*t)+.12f*t*std::sin(time*2+u*8),
        y+6.6f*(1-t)+hem*t,z+.4f+t*(.7f+.15f*std::sin(time*2.4f+u*18))+std::cos(u*43)*.09f);
}
inline Vec3 demonWebPoint(Vec3 tip,Vec3 a,Vec3 b,float beat,float u,float t) {
    const Vec3 edge=a*(1-u)+b*u+Vec3(0,.42f*std::sin(u*3.14159f),0);
    return tip*(1-t)+edge*t+Vec3(0,beat*t,std::sin(u*3.14159f)*std::sin(t*3.14159f)*.28f);
}
inline void demonKing(Scene &s,const Runner &run) {
    const int previousDetail=s.meshDetail; s.meshDetail=2;
    const float y=.09f*std::sin(run.time*1.8f),z=16;
    const bool enraged=run.bossHealth<=Runner::DemonMaxHealth/2;
    const float cast=!run.obstacleActive?std::max(0.f,1-run.bossShotCooldown/.65f):0;
    const Tint iron(64,63,77),dark(27,26,36),steel(113,111,123),brass(163,118,76);
    const Tint horn(203,180,143),flesh(87,62,66),ember(enraged?255:236,87,42);
    const size_t first=s.faces.size();
    const float previousGloss=s.gloss; s.gloss=.38f;

    // Pleated split cape: independent strips flutter and end at uneven hems.
    for(int strip=0;strip<28;++strip) for(int row=0;row<14;++row) {
        std::vector<Vec3> points,normals;
        for(int k=0;k<4;++k) {
            const float u=(strip+(k==1||k==2?1:0))/28.f;
            const float t=(row+(k>=2?1:0))/14.f;
            points.push_back(capePoint(u,t,run.time,y,z));
            normals.push_back(unit(cross(capePoint(u+.001f,t,run.time,y,z)-capePoint(u-.001f,t,run.time,y,z),
                capePoint(u,t+.001f,run.time,y,z)-capePoint(u,t-.001f,run.time,y,z))));
        }
        s.smoothPolygon(points,normals,Tint(69,25,37),4,true);
    }
    // Broad rib cage tapers into a narrow, articulated waist rather than a ball.
    demonLoft(s,{{Vec3(0,4.35f+y,z),.52f,.38f},{Vec3(0,4.9f+y,z),.60f,.43f},
        {Vec3(0,5.65f+y,z),.95f,.56f},{Vec3(0,6.25f+y,z),1.12f,.58f},
        {Vec3(0,6.65f+y,z),.78f,.43f}},iron);
    demonLoft(s,{{Vec3(0,6.45f+y,z),.31f,.30f},{Vec3(0,7.05f+y,z),.30f,.28f}},dark);
    for(int side=-1;side<=1;side+=2) {
        // Chest plates have their own bevels, collar trim and overlapped ribs.
        demonPlate(s,{Vec3(side*.06f,6.25f+y,z-.57f),Vec3(side*.52f,6.57f+y,z-.47f),
            Vec3(side*1.03f,6.2f+y,z-.43f),Vec3(side*.91f,5.69f+y,z-.49f),Vec3(side*.16f,5.60f+y,z-.6f)},.18f,steel,brass);
        for(int rib=0;rib<3;++rib) {
            const float h=5.45f-rib*.29f,w=.77f-rib*.08f;
            demonPlate(s,{Vec3(side*.10f,h+.22f+y,z-.52f),Vec3(side*w,h+.25f+y,z-.40f),
                Vec3(side*(w-.03f),h-.06f+y,z-.40f),Vec3(side*.12f,h-.14f+y,z-.55f)},.07f,iron,brass.shade(.65f));
        }
        // Belt-mounted tassets overlap the upper legs.
        demonPlate(s,{Vec3(side*.08f,4.62f+y,z-.45f),Vec3(side*.68f,4.66f+y,z-.30f),
            Vec3(side*.92f,3.85f+y,z-.40f),Vec3(side*.33f,3.65f+y,z-.57f)},.08f,dark,brass);
    }
    s.ellipsoid(Vec3(0,5.85f+y,z-.79f),Vec3(.14f,.26f,.11f),ember,10,6);
    for(int i=0;i<5;++i) {
        const float h=5.7f+i*.18f;
        s.link(Vec3(-.08f,h+y,z-.76f),Vec3(.08f,h+.08f+y,z-.76f),.012f,.008f,ember,4);
    }

    // A narrow skull with a brow, recessed eyes, cheek planes and a pointed jaw.
    demonLoft(s,{{Vec3(0,6.95f+y,z-.12f),.17f,.23f},{Vec3(0,7.12f+y,z-.13f),.32f,.34f},
        {Vec3(0,7.45f+y,z-.08f),.41f,.36f},{Vec3(0,7.76f+y,z),.43f,.34f},
        {Vec3(0,7.94f+y,z+.04f),.28f,.25f}},flesh);
    for(int side=-1;side<=1;side+=2) {
        demonPlate(s,{Vec3(side*.04f,7.74f+y,z-.35f),Vec3(side*.45f,7.85f+y,z-.22f),
            Vec3(side*.47f,7.59f+y,z-.30f),Vec3(side*.08f,7.48f+y,z-.49f)},.07f,iron,brass.shade(.7f));
        s.polygon({Vec3(side*.08f,7.46f+y,z-.47f),Vec3(side*.34f,7.54f+y,z-.42f),
            Vec3(side*.31f,7.44f+y,z-.44f),Vec3(side*.11f,7.40f+y,z-.49f)},Tint(255,123,54),4,false);
        demonPlate(s,{Vec3(side*.37f,7.40f+y,z-.34f),Vec3(side*.34f,7.15f+y,z-.37f),
            Vec3(side*.10f,6.97f+y,z-.34f),Vec3(side*.17f,7.29f+y,z-.46f)},.045f,steel,brass.shade(.5f));
        // Swept ram horns curl outward, back and up, with a second crown tine.
        demonHorn(s,Vec3(side*.34f,7.80f+y,z+.06f),Vec3(side*1.36f,7.9f+y,z+.45f),
            Vec3(side*1.25f,8.88f+y,z+.38f),Vec3(side*.79f,9.18f+y,z-.15f),.23f,horn,16);
        demonHorn(s,Vec3(side*.18f,7.92f+y,z+.06f),Vec3(side*.23f,8.24f+y,z+.08f),
            Vec3(side*.35f,8.35f+y,z+.02f),Vec3(side*.31f,8.55f+y,z-.15f),.09f,brass,8);
    }
    demonPlate(s,{Vec3(0,7.62f+y,z-.45f),Vec3(-.085f,7.24f+y,z-.49f),
        Vec3(0,7.17f+y,z-.55f),Vec3(.085f,7.24f+y,z-.49f)},.06f,iron,iron);
    s.link(Vec3(-.16f,7.11f+y,z-.45f),Vec3(.16f,7.11f+y,z-.45f),.027f,.027f,dark,5);
    for(int tooth=0;tooth<4;++tooth) {
        const float x=-.12f+tooth*.08f;
        s.polygon({Vec3(x-.022f,7.14f+y,z-.48f),Vec3(x+.022f,7.14f+y,z-.48f),Vec3(x,7.08f+y,z-.49f)},horn,4,false);
    }

    for(int side=-1;side<=1;side+=2) {
        const float lift=side<0?.42f+cast*.55f:0;
        const Vec3 shoulder(side*1.09f,6.35f+y,z),elbow(side*1.55f,5.40f+y+lift,z-.08f-lift*.3f);
        const Vec3 wrist(side*1.72f,4.68f+y+lift*1.6f,z-.32f-lift*.7f);
        s.ellipsoid(shoulder,Vec3(.40f,.41f,.42f),dark,10,6);
        ModelMesh::sweep(s,{{shoulder,.29f},{shoulder*.65f+elbow*.35f,.32f},
            {shoulder*.22f+elbow*.78f,.25f},{elbow,.22f}},iron,12);
        s.ellipsoid(elbow,Vec3(.23f,.23f,.25f),brass.shade(.55f),10,5);
        ModelMesh::sweep(s,{{elbow,.23f},{elbow*.72f+wrist*.28f,.28f},{elbow*.25f+wrist*.75f,.22f},{wrist,.18f}},iron,12);
        // Layered shoulder shells replace cylindrical shoulders and random spikes.
        for(int plate=0;plate<3;++plate) {
            const float x=side*(1.05f+plate*.20f),h=6.60f-plate*.12f;
            demonPlate(s,{Vec3(x-side*.28f,h+.12f+y,z-.29f),Vec3(x+side*.28f,h+.05f+y,z-.18f),
                Vec3(x+side*.36f,h-.36f+y,z-.29f),Vec3(x,h-.47f+y,z-.48f),Vec3(x-side*.27f,h-.23f+y,z-.35f)},.10f,iron,brass);
        }
        for(int spur=0;spur<2;++spur) {
            Vec3 a=shoulder+Vec3(side*spur*.27f,.25f,.05f);
            demonHorn(s,a,a+Vec3(side*.25f,.34f,0),a+Vec3(side*.58f,.66f,.20f),a+Vec3(side*.72f,.52f,.35f),.13f,horn,8);
        }
        demonPlate(s,{elbow+Vec3(-.19f,-.10f,-.22f),elbow+Vec3(.19f,-.10f,-.22f),
            wrist+Vec3(.16f,0,-.19f),wrist+Vec3(0,-.17f,-.25f),wrist+Vec3(-.16f,0,-.19f)},.08f,steel,brass);
        const Vec3 palm=wrist+Vec3(0,-.16f,-.03f);
        s.ellipsoid(palm,Vec3(.22f,.25f,.14f),dark,10,6);
        for(int finger=0;finger<4;++finger) {
            const Vec3 knuckle=palm+Vec3((finger-1.5f)*.105f,-.13f,-.03f);
            const float curl=side>0?.12f:.02f;
            const Vec3 joint=knuckle+Vec3((finger-1.5f)*.025f,-.18f,curl-.06f);
            s.link(knuckle,joint,.044f,.035f,steel,6);
            demonHorn(s,joint,joint+Vec3(0,-.11f,-.06f),joint+Vec3(0,-.16f,-.14f),joint+Vec3(0,-.13f,-.22f),.037f,horn,5);
        }
        s.link(palm+Vec3(side*.19f,.07f,0),palm+Vec3(side*.29f,-.12f,-.15f),.065f,.025f,steel,6);
        // Hip, knee and ankle articulate instead of a straight pole leg.
        const Vec3 hip(side*.44f,4.35f+y,z);
        const Vec3 knee(side*(side<0?.68f:.56f),3.28f+y,z+(side<0?-.38f:.10f));
        const Vec3 ankle(side*(side<0?.75f:.53f),y+(side<0?2.18f:2.35f),z+(side<0?-.15f:.54f));
        s.link(hip,knee,.33f,.24f,iron,10); s.ellipsoid(knee,Vec3(.25f,.28f,.28f),dark,10,6);
        s.link(knee,ankle,.24f,.16f,iron,10);
        demonPlate(s,{knee+Vec3(-.26f,.16f,-.24f),knee+Vec3(0,.36f,-.28f),knee+Vec3(.26f,.16f,-.24f),
            knee+Vec3(.16f,-.25f,-.27f),knee+Vec3(0,-.34f,-.32f),knee+Vec3(-.16f,-.25f,-.27f)},.13f,steel,brass);
        demonPlate(s,{knee+Vec3(-.18f,-.25f,-.21f),knee+Vec3(.18f,-.25f,-.21f),
            ankle+Vec3(.14f,.05f,-.16f),ankle+Vec3(0,-.13f,-.31f),ankle+Vec3(-.14f,.05f,-.16f)},.07f,iron,brass);
        demonLoft(s,{{ankle+Vec3(0,-.18f,-.22f),.24f,.50f},{ankle+Vec3(0,.08f,-.12f),.21f,.38f},
            {ankle+Vec3(0,.28f,0),.16f,.19f}},iron,10);

        // Wing fingers bend independently; scalloped webbing fills curved fans.
        const float beat=.16f*std::sin(run.time*1.7f);
        const Vec3 root(side*.7f,6.3f+y,z+.5f),bend(side*2.55f,7.85f+y+beat,z+.8f),tip(side*4.55f,8.6f+y+beat,z+.8f);
        demonHorn(s,root,root+Vec3(side*.6f,.7f,0),bend,tip,.16f,iron,14);
        demonHorn(s,tip,tip+Vec3(side*.3f,.38f,0),tip+Vec3(side*.6f,.32f,0),tip+Vec3(side*.68f,.66f,-.1f),.12f,horn,8);
        const Vec3 ends[]={Vec3(side*4.8f,5.7f+y,z+1),Vec3(side*3.35f,4.75f+y,z+.9f),
            Vec3(side*2.1f,4.3f+y,z+.75f),Vec3(side*1.05f,4.7f+y,z+.5f)};
        for(int finger=0;finger<4;++finger) {
            demonHorn(s,tip,tip*.66f+ends[finger]*.34f+Vec3(side*.15f,.18f,0),
                ends[finger]+Vec3(side*.1f,.65f,.1f),ends[finger],.09f,brass.shade(.65f),10);
            if(finger==3) break;
            for(int row=0;row<10;++row) for(int col=0;col<8;++col) {
                std::vector<Vec3> points,normals;
                for(int k=0;k<4;++k) {
                    const float u=(col+(k==1||k==2?1:0))/8.f;
                    const float t=(row+(k>=2?1:0))/10.f;
                    points.push_back(demonWebPoint(tip,ends[finger],ends[finger+1],beat,u,t));
                    const float nt=std::max(.01f,t);
                    normals.push_back(unit(cross(
                        demonWebPoint(tip,ends[finger],ends[finger+1],beat,u+.001f,nt)-demonWebPoint(tip,ends[finger],ends[finger+1],beat,u-.001f,nt),
                        demonWebPoint(tip,ends[finger],ends[finger+1],beat,u,nt+.001f)-demonWebPoint(tip,ends[finger],ends[finger+1],beat,u,nt-.001f))));
                }
                s.smoothPolygon(points,normals,Tint(92,30,44),4,true);
            }
        }
    }
    // A broad, bevelled black-iron greatsword with an ember channel and hooked guard.
    const float sx=1.73f,sz=z-.48f;
    s.link(Vec3(sx,4.78f+y,sz),Vec3(sx,4.05f+y,sz),.085f,.085f,dark,8);
    for(int wrap=0;wrap<5;++wrap)
        s.link(Vec3(sx-.07f,4.65f+y-wrap*.11f,sz-.04f),Vec3(sx+.07f,4.61f+y-wrap*.11f,sz-.04f),.018f,.018f,brass,5);
    for(int side=-1;side<=1;side+=2)
        demonHorn(s,Vec3(sx,4.03f+y,sz),Vec3(sx+side*.32f,4.08f+y,sz),Vec3(sx+side*.62f,3.95f+y,sz),
            Vec3(sx+side*.72f,4.18f+y,sz),.10f,brass,8);
    demonPlate(s,{Vec3(sx-.19f,3.98f+y,sz),Vec3(sx-.37f,3.65f+y,sz),Vec3(sx-.28f,1.85f+y,sz),
        Vec3(sx,1.15f+y,sz),Vec3(sx+.28f,1.85f+y,sz),Vec3(sx+.37f,3.65f+y,sz),Vec3(sx+.19f,3.98f+y,sz)},.16f,steel,brass.shade(.65f));
    for(int i=0;i<7;++i) {
        const float h=3.6f-i*.27f;
        s.polygon({Vec3(sx-.04f,h+y,sz-.17f),Vec3(sx+.04f,h+y,sz-.17f),
            Vec3(sx+.02f,h-.21f+y,sz-.17f),Vec3(sx-.01f,h-.25f+y,sz-.17f)},ember,4,false);
    }
    if(enraged) for(int i=0;i<10;++i) {
        const float a=i*2.39996f,t=std::fmod(run.time*.5f+i*.13f,1.f);
        s.ellipsoid(Vec3(std::cos(a)*(1+t),6+y+t*2,z+std::sin(a)*.65f),Vec3(.025f,.06f,.025f),ember,5,3);
    }
    if(run.bossHitTime>0) for(size_t i=first;i<s.faces.size();++i) {
        const float t=run.bossHitTime/.35f*.7f;
        s.faces[i].blend(Tint(255,238,176),t);
    }
    s.gloss=previousGloss;
    if(cast>0) for(int i=0;i<32;++i) {
        const float a=i*6.2831853f/32+run.time*3,b=a+6.2831853f/32,r=.45f+cast*.65f;
        const Vec3 c(0,5.85f+y,z-.94f),u(std::cos(a),std::sin(a),0),v(std::cos(b),std::sin(b),0);
        s.polygon({c+u*r,c+v*r,c+v*(r-.035f),c+u*(r-.035f)},ember,4,false);
    }
    s.meshDetail=previousDetail;
}
} }
#endif
