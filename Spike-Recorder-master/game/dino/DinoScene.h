#ifndef BYB_DINO_SCENE_H
#define BYB_DINO_SCENE_H
#include "DinoRunner.h"
#include "games/Scene3D.h"
#include "DemonKingScene.h"
#include "games/ModelMesh.h"
namespace BackyardBrains { namespace Dino3D {
using namespace Scene3D;

// The game and offline previews use exactly the same artwork polygons.
inline Tint mix(Tint a,Tint b,float t) {
    return Tint(a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t);
}
inline void transitionVeil(Scene &s,float amount) {
    amount=std::max(0.f,std::min(1.f,amount));
    if(amount<=0) return;
    for(size_t i=0;i<s.faces.size();++i) s.faces[i].blend(Tint(255,243,213),amount);
}
inline void skyFace(Scene &s,std::vector<Vec3> points,Tint color) {
    points=Scene::clip(points,0,0,true); points=Scene::clip(points,0,s.width,false);
    points=Scene::clip(points,1,0,true); points=Scene::clip(points,1,s.height,false);
    if(points.size()<3) return;
    Face f; f.points=points; f.tint=color; f.depth=0; f.layer=-10;
    s.faces.push_back(f);
}
inline void sky(Scene &s,float flight=0,float storm=0) {
    const float w=s.width,h=s.height;
    for(int i=0;i<192;++i) {
        const float t=i/191.f;
        const Tint desert=t<.48f?mix(Tint(40,75,87),Tint(201,169,139),t/.48f):
            mix(Tint(201,169,139),Tint(238,196,143),(t-.48f)/.52f);
        const Tint blue=mix(desert,mix(Tint(25,81,136),Tint(220,239,240),std::min(1.f,t*1.4f)),flight);
        const Tint color=mix(blue,mix(Tint(25,26,52),Tint(126,112,151),t),storm);
        skyFace(s,{Vec3(0,h*i/192),Vec3(w,h*i/192),Vec3(w,h*(i+1)/192),Vec3(0,h*(i+1)/192)},color);
    }
    const float cx=w*.73f,cy=h*.23f,r=std::min(w,h)*.052f;
    for(int ring=40;ring>=0;--ring) {
        const float radius=r*(1+ring*.025f);
        std::vector<Vec3> points;
        for(int i=0;i<48;++i) {
            const float a=i*6.2831853f/48;
            points.push_back(Vec3(cx+std::cos(a)*radius,cy+std::sin(a)*radius));
        }
        skyFace(s,points,mix(mix(Tint(255,234,182),Tint(218,200,244),storm),
            mix(mix(Tint(118,120,112),Tint(88,131,169),flight),Tint(57,53,82),storm),std::pow(ring/40.f,.30f)));
    }
    for(int bank=0;bank<3;++bank) {
        const float x=w*(.12f+bank*.29f),y=h*(.14f+(bank%2)*.05f);
        skyFace(s,{Vec3(x-w*.09f,y),Vec3(x-w*.04f,y-h*.008f),Vec3(x+w*.04f,y-h*.003f),
            Vec3(x+w*.11f,y+h*.004f),Vec3(x+w*.04f,y+h*.007f)},mix(Tint(159,160,149),Tint(207,227,232),flight));
    }
}
inline void mesa(Scene &s,float x,float z,float radius,float height,Tint color) {
    // Shared irregular contours keep strata aligned while avoiding perfect,
    // repeated cones. The shape seed is independent of scrolling distance.
    const float heights[]={0,.38f,.45f,.76f,.81f,1};
    const float radii[]={1,.91f,.93f,.72f,.74f,.60f};
    std::vector<Vec3> previous;
    for(int row=0;row<6;++row) {
        std::vector<Vec3> current;
        for(int i=0;i<12;++i) {
            const float a=i*6.2831853f/12,seed=x*.17f+height*.1f;
            const float outline=1+.09f*std::sin(a*3+seed)+.045f*std::cos(a*7-seed);
            const float h=heights[row],r=radii[row]*radius*outline;
            current.push_back(Vec3(x+r*std::cos(a)+height*h*.025f*std::sin(seed),
                h*height+height*h*.018f*std::sin(a*3+seed),z+r*std::sin(a)));
        }
        if(row) for(int i=0;i<12;++i) {
            const int k=(i+1)%12;
            const Tint band=color.shade((row==2||row==4?.80f:1+row*.025f));
            s.polygon({previous[i],current[i],current[k],previous[k]},band);
        }
        if(row==5) {
            const Vec3 center(x+height*.025f*std::sin(x*.17f+height*.1f),height,z);
            for(int i=0;i<12;++i) s.polygon({current[i],center,current[(i+1)%12]},color.shade(1.1f));
        }
        previous=current;
    }
}
inline void cactus(Scene &s,float x,float z,float scale,Tint color) {
    s.link(Vec3(x,.10f*scale,z),Vec3(x,.84f*scale,z),.14f*scale,.14f*scale,color,8);
    s.ellipsoid(Vec3(x,.84f*scale,z),Vec3(.14f,.16f,.14f)*scale,color,8,4);
    for(int side=-1;side<=1;side+=2) {
        const float y=(side<0?.40f:.57f)*scale;
        ModelMesh::sweep(s,{{Vec3(x,y,z),.09f*scale},
            {Vec3(x+side*.17f*scale,y+.015f*scale,z),.09f*scale},
            {Vec3(x+side*.27f*scale,y+.08f*scale,z),.085f*scale},
            {Vec3(x+side*.29f*scale,y+.24f*scale,z),.07f*scale}},color,10);
        s.ellipsoid(Vec3(x+side*.29f*scale,y+.24f*scale,z),Vec3(.07f,.08f,.07f)*scale,color,6,3);
    }
}
// Cross-section mesh: a continuous silhouette with a squared muzzle and jaw.
struct Section {
    Vec3 c; float w,h;
    Section(float z,float y,float width,float height,float x=0):c(x,y,z),w(width),h(height) {}
};
inline Vec3 headPoint(Vec3 p) {
    // A slight glance toward the camera exposes the profile while running ahead.
    const float angle=.28f,z=p.z-.65f;
    return Vec3(p.x*std::cos(angle)+z*std::sin(angle),p.y,
                .65f+z*std::cos(angle)-p.x*std::sin(angle));
}
inline float curveValue(float a,float b,float c,float d,float t) {
    return .5f*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t);
}
inline void sculpt(Scene &s,const std::vector<Section> &source,Tint color,bool head=false) {
    if(source.size()<2) return;
    const int samples=s.meshDetail>1?3:2;
    std::vector<Section> sections;
    for(size_t i=0;i+1<source.size();++i) {
        const Section &a=source[i?i-1:i],&b=source[i],&c=source[i+1],&d=source[std::min(i+2,source.size()-1)];
        for(int step=0;step<samples;++step) {
            const float t=step/static_cast<float>(samples);
            Section q(0,0,0,0);
            q.c=Vec3(curveValue(a.c.x,b.c.x,c.c.x,d.c.x,t),curveValue(a.c.y,b.c.y,c.c.y,d.c.y,t),curveValue(a.c.z,b.c.z,c.c.z,d.c.z,t));
            q.w=std::max(.003f,curveValue(a.w,b.w,c.w,d.w,t)); q.h=std::max(.003f,curveValue(a.h,b.h,c.h,d.h,t));
            sections.push_back(q);
        }
    }
    sections.push_back(source.back());
    const int n=s.meshDetail>1?32:16;
    std::vector<Vec3> points,vertexNormals;
    for(size_t j=0;j<sections.size();++j) {
        std::vector<Vec3> ring,normals;
        const Section &before=sections[j?j-1:j],&after=sections[std::min(j+1,sections.size()-1)];
        for(int i=0;i<n;++i) {
            const float angle=(i+.5f)*6.2831853f/n,cs=std::cos(angle),sn=std::sin(angle);
            Vec3 p=sections[j].c+Vec3(cs*sections[j].w,sn*sections[j].h,0);
            const Vec3 along=after.c-before.c+Vec3(cs*(after.w-before.w),sn*(after.h-before.h),0);
            Vec3 normal=unit(cross(Vec3(-sn*sections[j].w,cs*sections[j].h,0),along));
            if(head) normal=Vec3(normal.x*std::cos(.28f)+normal.z*std::sin(.28f),normal.y,normal.z*std::cos(.28f)-normal.x*std::sin(.28f));
            ring.push_back(head?headPoint(p):p); normals.push_back(normal);
        }
        if(j==0) { std::vector<Vec3> cap=ring; std::reverse(cap.begin(),cap.end()); s.polygon(cap,color); }
        if(j+1==sections.size()) s.polygon(ring,color);
        points.insert(points.end(),ring.begin(),ring.end()); vertexNormals.insert(vertexNormals.end(),normals.begin(),normals.end());
    }
    ModelMesh::ringMesh(s,points,vertexNormals,n,color);
}
inline Vec3 wingPoint(Vec3 a,Vec3 b,Vec3 c,float u,float v,float scallop) {
    const Vec3 e1=b-a,e2=c-a,base=unit(cross(e1,e2));
    const float t=u+v,q=t>.0001f?v/t:0;
    const Vec3 edge=b*(1-q)+c*q;
    return a+e1*u+e2*v+base*(2.7f*u*v*(1-u-v))+
        unit(a-edge)*(scallop*t*t*std::sin(q*3.14159265f));
}
inline void wingWeb(Scene &s,Vec3 a,Vec3 b,Vec3 c,Tint color,float scallop=0) {
    const Vec3 e1=b-a,e2=c-a,base=unit(cross(e1,e2));
    const float sign=dot(base,s.eye-(a+b+c)*(1.f/3))<0?-1.f:1.f;
    const int divisions=s.meshDetail>1?8:4;
    for(int i=0;i<divisions;++i) for(int j=0;j<divisions-i;++j) {
        for(int half=0;half<2;++half) {
            if(half && i+j>=divisions-1) continue;
            const float d=static_cast<float>(divisions);
            const float uv[2][6]={{i/d,j/d,(i+1)/d,j/d,i/d,(j+1)/d},
                {(i+1)/d,j/d,(i+1)/d,(j+1)/d,i/d,(j+1)/d}};
            std::vector<Vec3> p,n;
            for(int k=0;k<3;++k) {
                const float u=uv[half][k*2],v=uv[half][k*2+1];
                p.push_back(wingPoint(a,b,c,u,v,scallop));
                const Vec3 du=wingPoint(a,b,c,u+.001f,v,scallop)-wingPoint(a,b,c,u-.001f,v,scallop);
                const Vec3 dv=wingPoint(a,b,c,u,v+.001f,scallop)-wingPoint(a,b,c,u,v-.001f,scallop);
                n.push_back(unit(cross(du,dv))*sign);
            }
            s.smoothPolygon(p,n,color,4,true);
        }
    }
}
inline void dinosaur(Scene &s,const Runner &run,bool guardian=false) {
    const int previousDetail=s.meshDetail; s.meshDetail=2;
    const float mega=guardian?0:run.megaProgress();
    const float divine=guardian?0:run.divineProgress();
    const Tint skin=guardian?Tint(162,89,114):mix(mix(run.dragon?Tint(115,143,141):Tint(91,133,104),Tint(52,97,127),mega),Tint(236,231,202),divine);
    const Tint upper=mix(mix(run.dragon?Tint(66,101,117):Tint(67,105,84),Tint(29,55,85),mega),Tint(174,123,43),divine);
    const Tint shadow(60,93,76),belly=run.dragon?Tint(219,183,105):Tint(160,163,116);
    const bool airborne=run.height>.01f;
    const float phase=run.distance*2.7f;
    const float y=run.height+(airborne?0:.025f*std::cos(phase*2));
    const float stride=airborne?.40f:std::sin(phase)*.72f;
    const float sway=.04f*std::sin(run.flyingStage()?run.time*3:phase*.5f);
    // Low, forward-leaning torso and muscular S-neck; no upright mascot belly.
    sculpt(s,{Section(-.75f,y+.93f,.20f,.18f),Section(-.43f,y+1.01f,.36f,.32f),
        Section(.05f,y+1.06f,.39f,.36f),Section(.45f,y+1.15f,.28f,.29f),
        Section(.72f,y+1.39f,.22f,.25f),Section(.89f,y+1.52f,.23f,.21f)},skin);
    sculpt(s,{Section(-.35f,y+.79f,.24f,.11f),Section(.15f,y+.82f,.27f,.12f),
        Section(.55f,y+1.03f,.16f,.09f)},belly);
    // Brow-to-nose planes define a theropod skull, not a round toy head.
    sculpt(s,{Section(.45f,y+1.56f,.035f,.07f),Section(.52f,y+1.59f,.16f,.18f),
        Section(.62f,y+1.62f,.22f,.22f),Section(.87f,y+1.65f,.34f,.28f),
        Section(1.11f,y+1.62f,.31f,.22f),Section(1.57f,y+1.56f,.25f,.17f),
        Section(1.70f,y+1.54f,.22f,.14f),Section(1.755f,y+1.54f,.17f,.115f),
        Section(1.78f,y+1.54f,.08f,.08f),Section(1.797f,y+1.54f,.008f,.015f)},skin,true);
    sculpt(s,{Section(.83f,y+1.37f,.23f,.08f),Section(1.12f,y+1.34f,.27f,.075f),
        Section(1.62f,y+1.36f,.205f,.065f)},belly,true);
    for(int side=-1;side<=1;side+=2) {
        // Small amber eyes sit below a heavy sloping brow. No oversized whites.
        s.ellipsoid(headPoint(Vec3(side*.326f,y+1.71f,.93f)),Vec3(.042f,.060f,.09f),upper,10,6);
        s.ellipsoid(headPoint(Vec3(side*.359f,y+1.71f,.93f)),Vec3(.026f,.042f,.065f),Tint(213,166,69),10,6);
        s.ellipsoid(headPoint(Vec3(side*.383f,y+1.71f,.945f)),Vec3(.011f,.028f,.018f),Tint(20,29,23),8,5);
        s.link(headPoint(Vec3(side*.27f,y+1.79f,.77f)),headPoint(Vec3(side*.33f,y+1.76f,1.06f)),
            .065f,.040f,upper,7);
        s.link(headPoint(Vec3(side*.28f,y+1.48f,.93f)),headPoint(Vec3(side*.22f,y+1.44f,1.62f)),
            .017f,.012f,shadow,5);
        s.ellipsoid(headPoint(Vec3(side*.23f,y+1.61f,1.55f)),Vec3(.018f,.023f,.040f),shadow,7,4);
        for(int tooth=0;tooth<3;++tooth) {
            const float z=1.16f+tooth*.14f;
            s.link(headPoint(Vec3(side*(.27f-tooth*.02f),y+1.43f,z)),
                headPoint(Vec3(side*(.26f-tooth*.02f),y+1.38f,z+.015f)),.022f,.002f,Tint(211,203,159),5);
        }
        const float leg=stride*side;
        const Vec3 hip(side*.30f,y+.91f,-.27f);
        // Fold the knee forward under the belly, heel back against the flank,
        // and curl the toes backward. A delayed wing beat flexes both joints.
        const float tuck=run.flyingStage()?1.f:(run.stage==Runner::Ascending?
            std::min(1.f,run.ascentProgress()*4):0.f);
        const float flex=std::sin(run.time*7-.7f+side*.25f)*.045f*tuck;
        const Vec3 knee=Vec3(side*.35f,y+.48f,.12f+leg*.32f)*(1-tuck)+
            Vec3(side*.39f,y+.80f+flex,-.04f)*tuck;
        const Vec3 hock=Vec3(side*.33f,y+.23f,-.18f+leg*.48f)*(1-tuck)+
            Vec3(side*.37f,y+.68f+flex,-.70f)*tuck;
        const Vec3 ankle=Vec3(side*.32f,run.height+.08f+std::max(0.f,-leg)*.21f,leg*.53f)*(1-tuck)+
            Vec3(side*.34f,y+.60f+flex*1.4f,-.54f)*tuck;
        s.ellipsoid(hip,Vec3(.22f,.34f,.29f),skin,10,6);
        ModelMesh::sweep(s,{{hip,.16f,1.08f},{hip*.65f+knee*.35f,.195f,1.1f},
            {hip*.25f+knee*.75f,.15f},{knee,.115f}},skin,12);
        s.ellipsoid(knee,Vec3(.11f,.12f,.12f),skin,10,6);
        ModelMesh::sweep(s,{{knee,.092f},{knee*.65f+hock*.35f,.102f},{hock,.061f},
            {hock*.45f+ankle*.55f,.052f},{ankle,.040f}},skin,10);
        for(int toe=-1;toe<=1;++toe) {
            const Vec3 end=ankle+Vec3(toe*(.095f-.025f*tuck),-.015f+.055f*tuck,
                (.26f-std::abs(toe)*.045f)*(1-1.9f*tuck));
            s.link(ankle+Vec3(toe*.04f,0,.02f),end,.044f,.024f,skin,6);
            const Vec3 claw=end+Vec3(toe*.015f,-.005f+.07f*tuck,.095f*(1-1.8f*tuck));
            ModelMesh::sweep(s,{{end,.025f},{(end+claw)*.5f+Vec3(0,.018f,0),.016f},{claw,.001f}},Tint(65,60,44),8);
        }
        const Vec3 shoulder(side*.26f,y+1.16f,.45f),elbow(side*.37f,y+.99f,.57f);
        const Vec3 hand(side*.31f,y+1.03f,.74f);
        ModelMesh::sweep(s,{{shoulder,.065f},{shoulder*.55f+elbow*.45f,.068f},{elbow,.042f},
            {(elbow+hand)*.5f,.039f},{hand,.028f}},skin,10);
        for(int finger=0;finger<2;++finger)
            s.link(hand+Vec3(side*finger*.045f,0,0),hand+Vec3(side*finger*.045f,-.06f,.07f),.018f,.004f,shadow,5);
    }
    // One unbroken tapered tail counterbalances the forward body.
    sculpt(s,{Section(-2.30f,y+1.09f,.008f,.009f,sway*5),
        Section(-1.98f,y+1.08f,.045f,.05f,sway*4),Section(-1.61f,y+1.06f,.09f,.09f,sway*3),
        Section(-1.23f,y+1.03f,.14f,.13f,sway*2),Section(-.85f,y+1.00f,.21f,.18f,sway),
        Section(-.45f,y+.98f,.29f,.25f)},skin);
    // Restrained dorsal ridge; remove the unrelated stegosaur-like plates.
    sculpt(s,{Section(-1.7f,y+1.12f,.025f,.025f,sway*3),Section(-1.0f,y+1.18f,.05f,.035f,sway),
        Section(-.35f,y+1.32f,.075f,.04f),Section(.15f,y+1.40f,.08f,.04f),
        Section(.50f,y+1.44f,.06f,.04f)},upper);
    if(run.dragon) {
        const Tint gold=guardian?Tint(233,188,225):mix(mix(Tint(220,179,102),Tint(166,245,255),mega),Tint(255,211,83),divine);
        const Tint membrane=guardian?Tint(89,57,109):mix(mix(Tint(148,86,59),Tint(36,77,121),mega),Tint(225,208,145),divine);
        const float flap=std::sin(run.time*(run.stage==Runner::Ascending?11.f:7.f))*.38f;
        const float spread=(.65f+.35f*(1-run.evolutionTime/2.4f))*(1+.22f*mega);
        for(int side=-1;side<=1;side+=2) {
            const Vec3 hornRoot=headPoint(Vec3(side*.23f,y+1.84f,.76f));
            const Vec3 hornTip=headPoint(Vec3(side*(.40f+.15f*mega),y+2.20f+.28f*mega,.48f-.22f*mega));
            demonHorn(s,hornRoot,hornRoot+Vec3(side*.12f,.12f,-.04f),hornTip+Vec3(0,-.08f,-.08f),hornTip,.09f,gold,10);
            const Vec3 root(side*.24f,y+1.24f,.15f);
            const Vec3 elbow(side*.85f*spread,y+1.85f+flap,.01f);
            const Vec3 tip(side*2.05f*spread,y+2.10f+flap,-.80f);
            const Vec3 lower(side*1.35f*spread,y+1.28f+flap,-1.00f);
            const Vec3 inner(side*.66f*spread,y+1.08f,-.73f);
            const Vec3 back(side*.25f,y+1.06f,-.38f);
            // Two-sided membranes remain visible throughout the wing beat.
            wingWeb(s,root,elbow,tip,membrane);
            wingWeb(s,root,tip,lower,membrane.shade(.95f),.18f);
            wingWeb(s,root,lower,inner,membrane.shade(.9f),.12f);
            wingWeb(s,root,inner,back,membrane.shade(.85f),.06f);
            ModelMesh::sweep(s,{{root,.085f},{root*.5f+elbow*.5f,.075f},{elbow,.057f},
                {elbow*.65f+tip*.35f,.043f},{tip,.009f}},upper,10);
            s.link(root,lower,.025f,.008f,gold,5);
            s.link(root,inner,.025f,.008f,gold,5);
            if(divine>0) {
                // Branching antlers and flowing whiskers distinguish the divine form.
                for(int branch=0;branch<3;++branch) {
                    const Vec3 a=demonBezier(hornRoot,hornRoot+Vec3(side*.12f,.12f,-.04f),hornTip+Vec3(0,-.08f,-.08f),hornTip,.40f+branch*.18f);
                    const Vec3 end=a+Vec3(side*(.28f+branch*.07f)*divine,.26f*divine,-.20f*divine);
                    demonHorn(s,a,a+Vec3(side*.12f,.03f,-.04f)*divine,end-Vec3(0,.08f,0),end,.04f*divine,gold,6);
                }
                Vec3 last=headPoint(Vec3(side*.22f,y+1.43f,1.55f));
                std::vector<ModelMesh::SweepPoint> whisker={{last,.022f*divine}};
                for(int i=1;i<=8;++i) {
                    Vec3 next=last+Vec3(side*.09f*divine,.045f*std::sin(i*.6f+run.time*2)*divine,-.16f*divine);
                    whisker.push_back(ModelMesh::SweepPoint(next,.022f*(1-i/9.f)*divine)); last=next;
                }
                ModelMesh::sweep(s,whisker,gold,8);
                for(int feather=0;feather<5;++feather) {
                    const float t=(feather+1)/6.f;
                    const Vec3 a=wingPoint(root,tip,lower,1-t,t,.18f);
                    const Vec3 b=a+Vec3(side*.15f,-.06f,-(.40f+.1f*std::sin(t*3.14f))*divine);
                    ModelMesh::sweep(s,{{a,.055f*divine,.22f},{a*.65f+b*.35f,.065f*divine,.18f},
                        {a*.2f+b*.8f,.038f*divine,.15f},{b,.0008f,.2f}},gold,8);
                }
            }
            if(mega>0) {
                // A second swept wing tip and luminous spars change the silhouette.
                const Vec3 crest=tip+Vec3(side*.34f*mega,.18f*mega,-.42f*mega);
                wingWeb(s,tip,crest,lower,membrane,.10f);
                s.link(tip,crest,.04f*mega,.003f,gold,5);
                for(int vein=1;vein<=3;++vein) {
                    const float q=vein/4.f;
                    Vec3 normal=unit(cross(tip-root,lower-root));
                    if(dot(s.rotateNormal(normal),s.eye-s.modelPoint(root))<0) normal=normal*-1;
                    std::vector<ModelMesh::SweepPoint> spar;
                    for(int j=0;j<=5;++j) {
                        const float t=.12f+j*.16f;
                        spar.push_back(ModelMesh::SweepPoint(wingPoint(root,tip,lower,t*(1-q),t*q,.18f)+normal*.028f,
                            (.014f-.007f*t)*mega));
                    }
                    ModelMesh::sweep(s,spar,gold,6);
                }
                s.ellipsoid(Vec3(side*.30f,y+1.23f,.25f),Vec3(.17f,.18f,.26f)*mega,upper,7,4);
                s.link(Vec3(side*.30f,y+1.30f,.20f),Vec3(side*.57f,y+1.53f,-.13f),.10f*mega,.003f,gold,5);
                s.link(headPoint(Vec3(side*.25f,y+1.68f,.70f)),
                    headPoint(Vec3(side*.54f,y+1.85f,.23f)),.07f*mega,.002f,gold,5);
            }
        }
        for(int i=0;i<5;++i) {
            const float z=.1f-i*.32f;
            s.link(Vec3(0,y+1.42f-i*.055f,z),Vec3(0,y+1.67f-i*.08f,z-.07f),.07f,.003f,gold,5);
        }
        if(mega>0) s.ellipsoid(Vec3(0,y+1.40f,.30f),Vec3(.11f,.16f,.12f)*mega,Tint(213,255,250),8,5);
    }
    s.meshDetail=previousDetail;
}
inline void pearl(Scene &s,const Runner &run) {
    if(!run.pearlActive) return;
    const Vec3 c(0,Runner::pearlHeight()+.07f*std::sin(run.time*4),run.pearlZ);
    s.shadow(0,run.pearlZ,.27f,.27f,Tint(195,165,105));
    s.ellipsoid(c,Vec3(.26f,.26f,.26f),Tint(255,219,110),12,8);
    s.ellipsoid(c+Vec3(.12f,.11f,-.17f),Vec3(.075f,.075f,.04f),Tint(255,249,213),8,5);
    for(int i=0;i<20;++i) {
        const float a=i*6.2831853f/20+run.time,b=a+6.2831853f/20;
        s.link(c+Vec3(.37f*std::cos(a),.12f*std::sin(a),.37f*std::sin(a)),
            c+Vec3(.37f*std::cos(b),.12f*std::sin(b),.37f*std::sin(b)),.012f,.012f,Tint(255,235,164),4);
    }
}
inline void magicRing(Scene &s,Vec3 center,Vec3 u,Vec3 v,float radius,float rotation,Tint color) {
    for(int i=0;i<48;++i) {
        const float a=rotation+i*6.2831853f/48,b=a+6.2831853f/48;
        const Vec3 da=u*std::cos(a)+v*std::sin(a),db=u*std::cos(b)+v*std::sin(b);
        s.polygon({center+da*radius,center+db*radius,center+db*(radius-.025f),center+da*(radius-.025f)},color,4,false);
    }
    for(int i=0;i<8;++i) {
        const float a=rotation+i*6.2831853f/8;
        const Vec3 d=u*std::cos(a)+v*std::sin(a),t=u*(-std::sin(a))+v*std::cos(a);
        const Vec3 c=center+d*radius*.84f;
        s.polygon({c+d*.10f,c+t*.055f,c-d*.10f,c-t*.055f},color,4,false);
    }
}
inline void solarLaser(Scene &s,const Runner &run) {
    if(run.laserTime<=0) return;
    const float age=Runner::laserDuration()-run.laserTime;
    const float charge=std::min(1.f,age/Runner::laserCharge());
    const float fade=std::min(1.f,run.laserTime/.18f);
    const Vec3 seal(0,run.laserHeight,2.8f);
    for(int ring=0;ring<4;++ring)
        magicRing(s,seal+Vec3(0,0,ring*.07f),Vec3(1,0,0),Vec3(0,1,0),
            (.95f+ring*.40f)*(.3f+.7f*charge)*fade,run.time*(ring%2?1.f:-1.f),
            ring%2?Tint(255,247,194):Tint(255,195,51));
    // Twelve radial gold sigils surround the much larger solar seal.
    for(int i=0;i<12;++i) {
        const float a=i*6.2831853f/12+run.time*.3f;
        const Vec3 d(std::cos(a),std::sin(a),0),t(-std::sin(a),std::cos(a),0);
        const Vec3 c=seal+d*(1.85f*charge*fade);
        s.polygon({c+d*.21f,c+t*.075f,c-d*.21f,c-t*.075f},Tint(255,232,135),4,false);
    }
    if(age<Runner::laserCharge()) {
        s.ellipsoid(seal,Vec3(.32f,.32f,.32f)*(charge+.1f),Tint(255,249,213),12,6);
        return;
    }
    const Vec3 target(0,4.9f,18),axis=unit(target-seal);
    const Vec3 u=unit(cross(axis,Vec3(0,1,0))),v=cross(axis,u);
    for(int shell=0;shell<3;++shell) {
        const float radius=(.72f-shell*.23f)*fade;
        for(int i=0;i<16;++i) {
            const float a=i*6.2831853f/16,b=a+6.2831853f/16;
            const Vec3 da=(u*std::cos(a)+v*std::sin(a))*radius,db=(u*std::cos(b)+v*std::sin(b))*radius;
            s.polygon({seal+da,seal+db,target+db*.6f,target+da*.6f},
                shell==2?Tint(255,255,239):(shell?Tint(255,234,126):Tint(255,175,31)),4,false);
        }
    }
    for(int i=0;i<4;++i) {
        const float t=std::fmod(run.time*1.8f+i*.25f,1.f);
        magicRing(s,seal+(target-seal)*t,u,v,(.8f+.22f*t)*fade,run.time,Tint(255,245,181));
    }
}
inline void magicEffects(Scene &s,const Runner &run) {
    solarLaser(s,run);
    if(run.stage==Runner::Ascending && run.ascentTarget==Runner::Citadel) {
        const float p=run.ascentProgress();
        const Vec3 c(0,run.height+1,0);
        for(int ring=0;ring<3;++ring) {
            const float a=run.time+ring*1.1f;
            magicRing(s,c,Vec3(std::cos(a),0,std::sin(a)),Vec3(0,1,0),
                (1.2f+ring*.42f)*std::sin(p*3.14159265f)+.2f,-run.time,Tint(255,220+ring*10,115+ring*35));
        }
    }
    if(run.stage==Runner::Ascending && run.ascentTarget==Runner::Storm) {
        const float p=run.ascentProgress(), envelope=std::sin(p*3.14159265f);
        const Vec3 c(0,run.height+1,0);
        for(int ring=0;ring<3;++ring) {
            const float a=run.time*(ring%2?2.f:-2.f)+ring*1.2f;
            magicRing(s,c,Vec3(std::cos(a),0,std::sin(a)),Vec3(0,1,0),
                (1.3f+ring*.22f)*envelope+.25f,run.time,Tint(103+ring*45,228,255));
        }
        for(int i=0;i<16;++i) {
            const float a=i*2.39996f+run.time*3,r=(1-p)*2.4f+.45f;
            const Vec3 point=c+Vec3(std::cos(a)*r,std::sin(a*1.7f)*r,std::sin(a)*r);
            s.ellipsoid(point,Vec3(.035f,.09f,.035f)*envelope,Tint(192,250,255),5,3);
        }
    }
    if(run.magicTime<=0) return;
    const float age=Runner::magicDuration()-run.magicTime;
    const float bloom=std::min(1.f,age/Runner::magicCharge());
    const Vec3 seal(0,run.magicHeight,2.25f);
    // The seal is perpendicular to the direction of flight, in front of the muzzle.
    if(age<.85f) {
        magicRing(s,seal,Vec3(1,0,0),Vec3(0,1,0),.92f*bloom+.12f,run.time*1.6f,Tint(88,216,255));
        magicRing(s,seal+Vec3(0,0,.035f),Vec3(1,0,0),Vec3(0,1,0),.67f*bloom+.10f,-run.time*2,Tint(217,252,255));
        for(int triangle=0;triangle<2;++triangle) for(int edge=0;edge<3;++edge) {
            const float a=run.time+triangle*3.14159265f+edge*6.2831853f/3,b=a+6.2831853f/3;
            const Vec3 v1(std::cos(a),std::sin(a),0),v2(std::cos(b),std::sin(b),0);
            s.polygon({seal+v1*.58f*bloom,seal+v2*.58f*bloom,seal+v2*.54f*bloom,seal+v1*.54f*bloom},Tint(144,236,255),4,false);
        }
        s.ellipsoid(seal,Vec3(.16f,.16f,.16f)*(1+bloom),Tint(228,255,255),10,6);
    }
    if(age<Runner::magicCharge()) return;
    const float head=run.magicZ,tail=std::max(2.25f,head-5.f);
    // A detached bolt, not an infinite laser attached to the character.
    for(int shell=0;shell<2;++shell) {
        const float radius=shell?.13f:.29f;
        for(int i=0;i<12;++i) {
            const float a=i*6.2831853f/12,b=a+6.2831853f/12;
            const float y0=run.magicHeight+(4.9f-run.magicHeight)*(tail-2.25f)/13.75f;
            const float y1=run.magicHeight+(4.9f-run.magicHeight)*(head-2.25f)/13.75f;
            const Vec3 start(0,y0,tail),end(0,y1,head);
            const Vec3 da(std::cos(a)*radius,std::sin(a)*radius,0),db(std::cos(b)*radius,std::sin(b)*radius,0);
            s.polygon({start+da*.35f,start+db*.35f,end+db,end+da},
                shell?Tint(235,255,255):Tint(64,175+i%3*23,255),4,false);
        }
    }
    const float headY=run.magicHeight+(4.9f-run.magicHeight)*(head-2.25f)/13.75f;
    magicRing(s,Vec3(0,headY,head),Vec3(1,0,0),Vec3(0,1,0),.43f,run.time*5,Tint(170,248,255));
}
inline void dragonEffects(Scene &s,const Runner &run) {
    magicEffects(s,run);
    const float floor=run.flyingStage()?3.f:0.f;
    if(run.fireTime>0) {
        const Vec3 start=headPoint(Vec3(0,run.height+1.48f,1.68f));
        const bool stoppedByAttack=run.bossStage() && run.burstTime>0;
        const float reach=run.bossStage()?(stoppedByAttack?std::max(1.8f,run.burstZ):16.f):Runner::fireRange();
        const float targetY=run.bossStage() && !stoppedByAttack?4.9f:floor+.55f;
        // Continuous glowing tongues of flame, tapered at their tips.
        // Unlit emissive faces avoid the dark, solid-ball appearance of rocks.
        const Vec3 axis=unit(Vec3(0,targetY,reach)-start);
        const Vec3 u=unit(cross(axis,Vec3(0,1,0))),v=cross(axis,u);
        for(int tongue=0;tongue<3;++tongue) {
            Vec3 previous[9];
            for(int ring=0;ring<=15;++ring) {
                const float t=ring/15.f;
                const float length=reach-1.7f-tongue*.65f;
                const float envelope=std::sin(t*3.14159265f)*(.15f+.45f*t)+.07f*(1-t);
                const float radius=envelope*(tongue? .52f:1.f);
                const Vec3 c=start+axis*(length*t)+u*(std::sin(t*14-run.time*24+tongue)*.12f*t)
                    +v*((tongue-1)*.20f*t+std::sin(t*10-run.time*17)*.08f*t);
                Vec3 current[9];
                for(int side=0;side<9;++side) {
                    const float angle=side*6.2831853f/9;
                    current[side]=c+(u*std::cos(angle)+v*std::sin(angle))*radius;
                }
                if(ring>0) for(int side=0;side<9;++side) {
                    const Tint color=side<4?Tint(255,211+ring%3*10,86):Tint(255,103+ring%4*15,22);
                    s.polygon({previous[side],previous[(side+1)%9],current[(side+1)%9],current[side]},color,4,false);
                }
                for(int side=0;side<9;++side) previous[side]=current[side];
            }
        }
    }
    if(run.burstTime>0) for(int i=0;i<12;++i) {
        const float age=.65f-run.burstTime,a=i*2.39996f;
        const Vec3 c(std::cos(a)*age*2,floor+.3f+std::sin(i*1.7f)*age+age*2-age*age*3,
                     run.burstZ+std::sin(a)*age*2);
        s.ellipsoid(c,Vec3(.09f,.09f,.12f),i%3?Tint(245,156,42):Tint(84,63,43),5,3);
    }
    if(run.evolutionTime>0 || run.pickupTime>0) for(int i=0;i<12;++i) {
        const float angle=i*6.2831853f/12+run.time*2;
        const float radius=run.dragon?1.1f:.65f;
        s.ellipsoid(Vec3(std::cos(angle)*radius,run.height+.7f+(i%4)*.37f,std::sin(angle)*radius),
            Vec3(.035f,.08f,.035f),Tint(255,225,121),5,3);
    }
}
inline void cloud(Scene &s,Vec3 c,float scale,Tint color=Tint(244,250,250)) {
    const float oldGloss=s.gloss; s.gloss=0;
    for(int i=0;i<5;++i)
        s.ellipsoid(c+Vec3((i-2)*.8f*scale,(i%2)*.23f*scale,std::sin(i*2.f)*.3f*scale),
            Vec3(1.15f,.57f+(i%2)*.25f,.86f)*scale,color,9,5);
    s.gloss=oldGloss;
}
inline float climbPitch(float progress) {
    return -.62f*std::sin(progress*3.14159265f);
}
inline void overlayFace(Scene &s,std::vector<Vec3> points,Tint color,int layer) {
    const size_t before=s.faces.size();
    skyFace(s,points,color);
    if(s.faces.size()>before) s.faces.back().layer=layer;
}
inline Vec3 climbPose(Vec3 p,float height,float progress) {
    const Vec3 origin(0,height+1,0),d=p-origin;
    const float angle=climbPitch(progress);
    return origin+Vec3(d.x,d.y*std::cos(angle)-d.z*std::sin(angle),d.y*std::sin(angle)+d.z*std::cos(angle));
}
inline void fireRibbon(Scene &s,Vec3 a,Vec3 b,float widthA,float widthB,Tint tint) {
    const Vec3 side=unit(cross(b-a,s.eye-(a+b)*.5f));
    s.polygon({a-side*widthA,a+side*widthA,b+side*widthB,b-side*widthB},tint,4,false);
}
inline void ascentEffects(Scene &s,const Runner &run) {
    const float p=run.ascentProgress(),y=run.height,t=run.ascentTime;
    // Three shock fronts peel off the launch point, then fade into the sand.
    for(int wave=0;wave<3;++wave) {
        const float age=t-wave*.19f;
        if(age<0 || age>1.55f) continue;
        const float r=.35f+age*5.7f,width=.22f*(1-age/1.7f);
        const Tint color=mix(wave==1?Tint(255,250,201):Tint(255,177,53),Tint(222,182,126),age/1.55f);
        for(int i=0;i<64;++i) {
            const float a=i*6.2831853f/64,b=(i+1)*6.2831853f/64,h=run.ascentStartHeight-.35f;
            s.polygon({Vec3(std::cos(a)*r,h,std::sin(a)*r),Vec3(std::cos(b)*r,h,std::sin(b)*r),
                Vec3(std::cos(b)*(r+width),h,std::sin(b)*(r+width)),Vec3(std::cos(a)*(r+width),h,std::sin(a)*(r+width))},color,4,false);
        }
    }
    // Wing-tip trails use historical poses, so they follow the actual climb.
    for(int side=-1;side<=1;side+=2) for(int strand=0;strand<3;++strand) {
        Vec3 previous;
        for(int i=0;i<=22;++i) {
            const float age=i*.028f,oldTime=std::max(0.f,t-age),oldP=oldTime/Runner::ascentDuration();
            const float destinationHeight=run.ascentTarget!=Runner::Sky?11.f:8.f;
            const float oldY=run.ascentStartHeight+(destinationHeight-run.ascentStartHeight)*oldP*oldP*(3-2*oldP);
            const float spread=.65f+.35f*std::min(1.f,oldTime/2.4f);
            const float flap=std::sin((run.time-age)*11)*.38f;
            Vec3 tip=climbPose(Vec3(side*(2.05f*spread+strand*.045f),oldY+2.10f+flap,-.80f),oldY,oldP);
            tip.y-=age*.9f; tip.z-=age*1.4f;
            if(i>0) fireRibbon(s,previous,tip,(1-(i-1)/23.f)*(.085f-strand*.02f),
                (1-i/23.f)*(.085f-strand*.02f),strand==0?Tint(255,125,25):(strand==1?Tint(255,202,66):Tint(255,247,180)));
            previous=tip;
        }
    }
    // Twisting flame wakes remain mainly below the body and leave the face clear.
    for(int ribbon=0;ribbon<3;++ribbon) for(int i=0;i<34;++i) {
        const float q=i/34.f,next=(i+1)/34.f;
        const float a=q*10-t*7+ribbon*2.0944f,b=next*10-t*7+ribbon*2.0944f;
        const float radius=.55f+q*.60f;
        const Vec3 c(std::cos(a)*radius,y-3.4f+q*3.5f,std::sin(a)*radius-.4f);
        const Vec3 d(std::cos(b)*(radius+.02f),y-3.4f+next*3.5f,std::sin(b)*(radius+.02f)-.4f);
        const float width=.025f+.10f*std::sin(q*3.14159f);
        fireRibbon(s,c,d,width,width,ribbon==0?Tint(255,107,23):(ribbon==1?Tint(255,196,54):Tint(255,237,147)));
    }
    // Bright elongated embers fall away, reinforcing upward acceleration.
    for(int i=0;i<70;++i) {
        const float angle=i*2.39996f,age=std::fmod(t*.9f+i*.137f,1.f);
        const float radius=.4f+age*(1.5f+p*1.8f);
        const Vec3 c(std::cos(angle)*radius,y+1.1f-age*(4+p*3),std::sin(angle)*radius-.5f);
        const Vec3 end=c-Vec3(0,.12f+age*.5f,0);
        fireRibbon(s,c,end,.018f+(1-age)*.025f,0,i%3?Tint(255,215,99):Tint(255,249,209));
    }
    // Clouds part around a clear central opening during the second half.
    if(p>.40f) {
        const float spread=(p-.40f)/.60f;
        for(int i=0;i<8;++i) {
            const float angle=i*6.2831853f/8;
            cloud(s,Vec3(std::cos(angle)*(3.5f+spread*4),run.ascentStartHeight+3.2f,
                std::sin(angle)*(3.5f+spread*4)),.5f+std::sin(spread*3.14159f)*.7f);
        }
    }
    // Edge streaks and gently eased letterboxing keep the silhouette readable.
    if(p>.22f && p<.9f) for(int i=0;i<18;++i) {
        const float age=std::fmod(t*1.5f+i*.173f,1.f);
        const float x=s.width*(i%2?.90f+(i%5)*.02f:.02f+(i%5)*.02f);
        const float top=s.height*(age*1.3f-.15f),length=s.height*(.04f+p*.06f);
        overlayFace(s,{Vec3(x,top),Vec3(x+1.1f,top),Vec3(x+2.1f,top+length),Vec3(x+.8f,top+length)},Tint(222,231,217),5);
    }
    const float bars=std::min(1.f,std::min(p/.12f,(1-p)/.14f))*s.height*.042f;
    overlayFace(s,{Vec3(0,0),Vec3(s.width,0),Vec3(s.width,bars),Vec3(0,bars)},Tint(14,23,29),6);
    overlayFace(s,{Vec3(0,s.height-bars),Vec3(s.width,s.height-bars),Vec3(s.width,s.height),Vec3(0,s.height)},Tint(14,23,29),6);
}
inline Scene makeSkyScene(const Runner &run,int width,int height) {
    Scene s(width,height);
    const bool transition=run.stage==Runner::Ascending;
    const float progress=transition?run.ascentProgress():0;
    const float storm=run.bossStage()?1.f:progress;
    s.setCamera(Vec3(4,5.4f,-7),Vec3(0,3,11),s.focal);
    if(transition) {
        const float follow=run.height-(run.ascentStartHeight*(1-progress)+.8f*progress);
        const float arc=std::sin(progress*3.14159265f);
        s.setCamera(Vec3(4+1.8f*arc,5.4f+follow,-7+.6f*arc),Vec3(0,3+follow,11),s.focal*(1+.06f*arc));
        s.centerX+=(s.width*.5f-s.project(s.camera(Vec3(0,run.height+4,0))).x)*arc;
    }
    s.fogColor=mix(Tint(195,221,234),Tint(100,91,124),storm); s.fogStart=20; s.fogRange=95;
    s.faces.reserve(6500); sky(s,1,storm);
    const bool citadel=run.stage==Runner::Citadel;
    const bool bestow=transition && run.ascentTarget==Runner::Citadel;
    if(citadel) {
        s.fogColor=Tint(103,53,64);
        for(size_t i=0;i<s.faces.size();++i) s.faces[i].blend(Tint(113,35,45),.45f);
        for(int side=-1;side<=1;side+=2) for(int tower=0;tower<4;++tower) {
            const float x=side*(7+tower*4),z=27+tower*5;
            ModelMesh::roundedBox(s,Vec3(x,3,z),Vec3(2.5f,12+tower*1.5f,2.7f),.13f,Tint(49,37,52));
            s.link(Vec3(x,9+tower*.75f,z),Vec3(x,12+tower*.75f,z),1.8f,.01f,Tint(93,47,64),5);
            s.box(Vec3(x,5,z-1.4f),Vec3(.4f,2.5f,.03f),Tint(245,100,49));
        }
        magicRing(s,Vec3(0,5,32),Vec3(1,0,0),Vec3(0,1,0),7,run.time*.1f,Tint(197,62,65));
    }
    // Cloud sea and floating ruins give the flight its own readable landscape.
    for(int i=0;i<18;++i) {
        const float z=std::fmod(i*9.f+162-std::fmod(run.distance,162.f),162.f)-12;
        const float sign=i%2?1.f:-1.f;
        cloud(s,Vec3(sign*(5+i%3*3),-.8f,z),1.6f+i%2*.5f,mix(Tint(244,250,250),Tint(145,137,167),storm));
        if(i%3==0) {
            const float x=sign*(8+i%4*2);
            s.link(Vec3(x,-5,z),Vec3(x,.5f,z),.1f,2.4f,mix(Tint(97,116,140),Tint(74,66,96),storm),7);
            s.link(Vec3(x,.5f,z),Vec3(x,.68f,z),2.4f,2.3f,mix(Tint(112,160,144),Tint(135,105,150),storm),7);
            for(int side=-1;side<=1;side+=2) {
                s.link(Vec3(x+side*.9f,.7f,z),Vec3(x+side*.9f,3.7f,z),.25f,.20f,Tint(207,213,195),7);
                s.link(Vec3(x+side*.9f,.69f,z),Vec3(x+side*.9f,.88f,z),.34f,.29f,Tint(185,193,179),10);
                s.link(Vec3(x+side*.9f,3.50f,z),Vec3(x+side*.9f,3.72f,z),.25f,.34f,Tint(210,216,199),10);
            }
            ModelMesh::roundedBox(s,Vec3(x,3.8f,z),Vec3(2.4f,.35f,.55f),.055f,Tint(207,213,195));
        }
    }
    if(storm>.2f) for(int bolt=0;bolt<3;++bolt) {
        const float x=(bolt%2?1.f:-1.f)*(10+bolt*4),z=32+bolt*11;
        for(int segment=0;segment<5;++segment) {
            Vec3 a(x+(segment%2)*.7f,10-segment*1.3f,z);
            Vec3 b(x+((segment+1)%2)*.7f,10-(segment+1)*1.3f,z);
            fireRibbon(s,a,b,.035f,.018f,mix(Tint(132,127,161),Tint(233,215,255),storm));
        }
    }
    if((run.stage==Runner::Storm || (bestow && progress<.66f)) && !run.won) {
        Runner boss=run; boss.stage=Runner::Storm;
        boss.height=3.5f+(bestow?run.height-progress*1.2f:0)+.12f*std::sin(run.time*2); boss.evolutionTime=0;
        const size_t first=s.faces.size();
        s.modelOrigin=Vec3(0,boss.height+1,0); s.modelTranslation=Vec3(0,0,16);
        s.modelScale=1.65f*(bestow?1-progress*.8f:1); s.modelYaw=3.14159265f;
        dinosaur(s,boss,true);
        s.modelScale=1; s.modelYaw=0; s.modelTranslation=Vec3();
        if(run.bossHitTime>0) for(size_t i=first;i<s.faces.size();++i)
            s.faces[i].blend(Tint(255,219,166),run.bossHitTime/.35f*.65f);
        if(bestow) for(size_t i=first;i<s.faces.size();++i)
            s.faces[i].blend(Tint(255,241,178),std::min(1.f,progress*2));
    }
    if(bestow && progress>.08f && progress<.88f) {
        // Light leaves the dying guardian and converges on the player's heart.
        for(int strand=0;strand<3;++strand) for(int i=0;i<28;++i) {
            const float t=std::fmod(i/28.f+run.time*.7f,1.f),a=t*12+strand*2.094f;
            const float radius=std::sin(t*3.14159f)*(.5f+strand*.25f);
            const Vec3 c(std::cos(a)*radius,run.height+4.3f+std::sin(a)*radius,16*(1-t));
            fireRibbon(s,c,c+Vec3(0,.02f,-.28f),.055f,.018f,Tint(255,218+strand*12,107+strand*30));
        }
    }
    if(citadel && !run.won) demonKing(s,run);
    // Faint moving wisps below the center show the flight direction.
    for(int i=0;i<12;++i) {
        const float z=std::fmod(i*6.f+72-std::fmod(run.distance,72.f),72.f)-8;
        for(int side=-1;side<=1;side+=2)
            s.link(Vec3(side*2.4f,2.6f,z),Vec3(side*2.4f,2.6f,z+.8f),.018f,.003f,Tint(220,244,249),4);
    }
    if(run.obstacleActive) {
        const float h=run.obstacleHeight(),z=run.obstacleZ;
        if(run.bossStage()) {
            s.ellipsoid(Vec3(0,3+h*.5f,z),Vec3(.46f,h*.5f,.55f),Tint(216,89,146),10,6);
            s.ellipsoid(Vec3(.12f,3+h*.6f,z-.35f),Vec3(.23f,.25f,.20f),Tint(255,208,204),8,5);
            for(int trail=0;trail<5;++trail)
                fireRibbon(s,Vec3(std::sin(run.time*12+trail)*.13f,3+h*.5f,z+.4f+trail*.25f),
                    Vec3(0,3+h*.5f,z+.8f+trail*.25f),.15f-trail*.02f,.02f,Tint(218,135,224));
        } else {
        s.link(Vec3(0,3,z),Vec3(0,3+h*.55f,z),.08f,.47f,Tint(143,85,154),7);
        s.link(Vec3(0,3+h*.55f,z),Vec3(0,3+h,z),.47f,.04f,Tint(222,136,169),7);
        s.link(Vec3(-.42f,3.05f,z),Vec3(-.28f,3+h*.75f,z),.14f,.006f,Tint(235,172,185),5);
        s.link(Vec3(.40f,3.05f,z),Vec3(.28f,3+h*.65f,z),.14f,.006f,Tint(235,172,185),5);
        }
    }
    Runner flying=run; flying.height+=3;
    if(transition) { flying.ascentStartHeight+=3; s.modelOrigin=Vec3(0,flying.height+1,0); s.modelPitch=climbPitch(progress); }
    dinosaur(s,flying);
    s.modelPitch=0;
    dragonEffects(s,flying);
    if(transition) ascentEffects(s,flying);
    if(run.won) for(int i=0;i<28;++i) {
        const float angle=i*2.39996f,age=std::fmod(run.victoryTime*.45f+i*.13f,1.f);
        const Vec3 c(std::cos(angle)*age*5,4+std::sin(angle)*age*4,16-age*2);
        fireRibbon(s,c,c+Vec3(0,.4f,0),.06f,0,Tint(255,225,139));
    }
    transitionVeil(s,transition?(progress-.86f)/.14f:1-run.stageTime/.5f);
    s.sort(); return s;
}
inline float sandHeight(float x,float z) {
    const float blend=std::min(1.f,std::max(0.f,(std::fabs(x)-2.6f)/7));
    return -.04f+blend*(.35f+.22f*std::sin(x*.29f+z*.065f)+.13f*std::sin(z*.18f-x*.10f));
}
inline void sandStrip(Scene &s,float x1,float x2,float z1,float z2,float distance,Tint color) {
    const float xs[]={x1,x1,x2,x2},zs[]={z1,z2,z2,z1};
    std::vector<Vec3> points,normals;
    for(int i=0;i<4;++i) {
        const float x=xs[i],z=zs[i]+distance;
        points.push_back(Vec3(x,sandHeight(x,z),zs[i]));
        normals.push_back(unit(Vec3(-(sandHeight(x+.1f,z)-sandHeight(x-.1f,z))/.2f,1,
            -(sandHeight(x,z+.1f)-sandHeight(x,z-.1f))/.2f)));
    }
    s.smoothPolygon(points,normals,color,0);
}
inline Scene makeScene(const Runner &run,int width,int height) {
    if(run.flyingStage() || (run.stage==Runner::Ascending && run.ascentTarget!=Runner::Sky)) return makeSkyScene(run,width,height);
    Scene s(width,height);
    const float climb=run.stage==Runner::Ascending?run.ascentProgress():0;
    if(run.stage==Runner::Ascending) {
        const float follow=run.height-(run.ascentStartHeight*(1-climb)+.8f*climb);
        const float arc=std::sin(climb*3.14159265f);
        s.setCamera(Vec3(4+1.8f*arc,3.2f+follow+.25f*arc,-7+.6f*arc),
            Vec3(0,.8f+follow,11),s.focal*(1+.06f*arc));
        const float subjectX=s.project(s.camera(Vec3(0,run.height+1,0))).x;
        s.centerX+=(s.width*.5f-subjectX)*arc;
    }
    s.fogColor=Tint(207,174,144); s.fogStart=24; s.fogRange=100;
    s.faces.reserve(5000);
    sky(s,climb);
    const Tint sand(181,134,92),track(223,182,126);
    for(int i=0;i<32;++i) {
        const float z=-10+i*5.f;
        const float bands[]={2.3f,3.6f,6.f,10.f,17.f,30.f,65.f,160.f};
        for(int band=0;band<7;++band) {
            sandStrip(s,bands[band],bands[band+1],z,z+5,run.distance,sand);
            sandStrip(s,-bands[band+1],-bands[band],z,z+5,run.distance,sand);
        }
        s.ground(-2.3f,2.3f,z,z+5,track,1);
        s.ground(-2.4f,-2.3f,z,z+5,Tint(237,199,145),2);
        s.ground(2.3f,2.4f,z,z+5,Tint(237,199,145),2);
        s.ground(-2.65f,-2.4f,z,z+5,Tint(155,111,78),1);
        s.ground(2.4f,2.65f,z,z+5,Tint(155,111,78),1);
    }
    for(int i=0;i<24;++i) {
        const float z=std::fmod(i*6.f+144-std::fmod(run.distance,144.f),144.f)-10;
        const float sign=i%2?1.f:-1.f;
        if(i%2==0) mesa(s,(i%4?1.f:-1.f)*(13+i%3*4),z,4+i%3,5+i%5*1.5f,Tint(187,117,84));
        if(i%3==0) mesa(s,-sign*(25+i%4*5),z+40,8,11+i%3*3,Tint(177,131,112));
        const float x=sign*(3.5f+(i%4)*.7f);
        // Irregular patches break up the flat sand without obscuring the lane.
        s.polygon({Vec3(x+sign*2,-.03f,z-1),Vec3(x+sign*6,.05f,z+1),
            Vec3(x+sign*5,.12f,z+4),Vec3(x+sign*1,.04f,z+3)},
            Tint(185+i%3*2,139+i%3*2,96+i%3*2),2,false);
        s.shadow(x,z,.55f,.55f,Tint(157,118,82));
        if(i%3==0) cactus(s,x,z,1.1f+i%2*.4f,Tint(98,130,109));
        else s.ellipsoid(Vec3(x,.19f,z),Vec3(.46f,.27f,.39f),Tint(192,146,105),7,4);
        for(int blade=0;blade<3;++blade)
            s.link(Vec3(x+.6f,0,z+1),Vec3(x+.6f+(blade-1)*.17f,.25f+(blade%2)*.12f,z+1.04f),
                .022f,.003f,Tint(156,145,94),4);
        const float scratchX=((i*17)%19-9)*.18f;
        s.ground(scratchX,scratchX+.025f,z,z+.4f,Tint(202,158,109),2);
        s.ground(-1.72f,-1.68f,z,z+.95f,Tint(212,169,116),2);
        s.ground(1.68f,1.72f,z,z+.95f,Tint(212,169,116),2);
    }
    const float oz=run.obstacleZ;
    if(run.obstacleActive) {
    s.shadow(.10f,oz+.15f,.65f,.52f,Tint(173,137,94));
    s.shadow(0,oz,.45f,.33f,Tint(147,119,83));
    if(run.passed%3==2) {
        s.ellipsoid(Vec3(0,.33f,oz),Vec3(.48f,.39f,.39f),Tint(155,91,67),9,5);
        s.ellipsoid(Vec3(-.25f,.12f,oz+.18f),Vec3(.22f,.15f,.20f),Tint(183,118,80),7,4);
    } else cactus(s,0,oz,1,Tint(56,111,89));
    }
    pearl(s,run);
    const float shadowScale=1/(1+run.height*.17f);
    for(int i=12;i>=0;--i) {
        const float t=i/12.f;
        s.shadow(.15f,.14f,(.38f+t*.45f)*shadowScale,(.65f+t*.60f)*shadowScale,
            mix(Tint(169,139,97),track,t));
    }
    if(run.height<.05f && !run.over) for(int i=0;i<5;++i) {
        const float age=std::fmod(run.distance*.55f+i*.2f,1.f);
        s.ellipsoid(Vec3((i%2?-.3f:.3f)*(1+age),.08f+age*.10f,-.4f-age*1.7f),
            Vec3(.05f+age*.12f,.04f+age*.06f,.07f+age*.13f),mix(Tint(245,217,173),track,age),8,5);
    }
    if(run.stage==Runner::Ascending) {
        s.modelOrigin=Vec3(0,run.height+1,0); s.modelPitch=climbPitch(climb);
    }
    dinosaur(s,run);
    s.modelPitch=0;
    dragonEffects(s,run);
    if(run.stage==Runner::Ascending) ascentEffects(s,run);
    if(run.stage==Runner::Ascending) transitionVeil(s,(climb-.86f)/.14f);
    s.sort();
    return s;
}
} }
#endif
