#ifndef BYB_SCENE_3D_H
#define BYB_SCENE_3D_H
#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>
#include <array>
namespace BackyardBrains { namespace Scene3D {

struct Vec3 {
    float x,y,z;
    Vec3(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
    Vec3 operator+(const Vec3 &v) const { return Vec3(x+v.x,y+v.y,z+v.z); }
    Vec3 operator-(const Vec3 &v) const { return Vec3(x-v.x,y-v.y,z-v.z); }
    Vec3 operator*(float s) const { return Vec3(x*s,y*s,z*s); }
};
inline float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return Vec3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x); }
inline Vec3 unit(Vec3 v) { const float n=std::sqrt(dot(v,v)); return n>0?v*(1/n):Vec3(); }
struct Tint {
    float r,g,b;
    Tint(float red=0,float green=0,float blue=0):r(red),g(green),b(blue){}
    Tint shade(float s) const { return Tint(r*s,g*s,b*s); }
};
struct Face {
    std::vector<Vec3> points; // x/y are screen pixels; z is reciprocal camera depth.
    std::vector<Tint> colors; // Optional per-vertex lighting, including clipped vertices.
    Tint tint;
    float depth;
    int layer;
    Tint color(size_t i) const { return colors.empty()?tint:colors[i]; }
    void blend(Tint target,float t) {
        tint=Tint(tint.r+(target.r-tint.r)*t,tint.g+(target.g-tint.g)*t,tint.b+(target.b-tint.b)*t);
        for(auto &c:colors) c=Tint(c.r+(target.r-c.r)*t,c.g+(target.g-c.g)*t,c.b+(target.b-c.b)*t);
    }
};

// A small CPU perspective renderer. It reuses the recorder's 2D GL context,
// without changing its viewport or matrices. Reciprocal depth supports optional
// per-pixel occlusion; sorting remains a fallback for hosts without a depth buffer.
class Scene {
public:
    int width,height;
    Vec3 eye,forward,right,up;
    float focal;
    float centerX, centerY;
    Tint fogColor;
    float fogStart, fogRange;
    float gloss = .10f;
    int meshDetail = 1; // Scoped by foreground models; distant scenery stays inexpensive.
    // Optional local pose, scoped by the caller; defaults preserve every scene.
    float modelPitch = 0;
    float modelYaw = 0, modelScale = 1;
    Vec3 modelOrigin;
    Vec3 modelTranslation;
    std::vector<Face> faces;
    Scene(int w,int h) : width(std::max(1,w)),height(std::max(1,h)),
        eye(4.0f,3.2f,-7.0f), centerX(width*.5f), centerY(height*.43f),
        fogColor(55,62,70), fogStart(18), fogRange(85) {
        forward=unit(Vec3(0,.8f,11)-eye);
        right=unit(cross(Vec3(0,1,0),forward));
        up=cross(forward,right);
        focal=std::min(width*.93f,height*1.18f);
        faces.reserve(1000);
    }
    Vec3 camera(Vec3 p) const {
        p=p-eye; return Vec3(dot(p,right),dot(p,up),dot(p,forward));
    }
    void setCamera(Vec3 position,Vec3 target,float lens) {
        eye=position; forward=unit(target-eye);
        right=unit(cross(Vec3(0,1,0),forward)); up=cross(forward,right);
        focal=lens;
    }
    void link(Vec3 a,Vec3 b,float ra,float rb,Tint tint,int sides=8) {
        if(ra<=0 && rb<=0) return;
        const float pixels=std::max(ra,rb)*focal/std::max(1.f,camera((a+b)*.5f).z);
        if(pixels>3) sides=std::max(sides,meshDetail>1?20:12);
        const Vec3 axis=unit(b-a);
        const float slope=(ra-rb)/std::max(.001f,std::sqrt(dot(b-a,b-a)));
        const Vec3 u=unit(cross(axis,std::fabs(axis.y)>.9f?Vec3(1,0,0):Vec3(0,1,0)));
        const Vec3 v=cross(axis,u);
        std::vector<Vec3> capA,capB;
        for(int i=0;i<sides;++i) {
            const float t=i*6.2831853f/sides, next=(i+1)*6.2831853f/sides;
            const Vec3 p=u*std::cos(t)+v*std::sin(t), q=u*std::cos(next)+v*std::sin(next);
            smoothPolygon({a+p*ra,a+q*ra,b+q*rb,b+p*rb},
                {unit(p+axis*slope),unit(q+axis*slope),unit(q+axis*slope),unit(p+axis*slope)},tint);
            capA.insert(capA.begin(),a+p*ra); capB.push_back(b+p*rb);
        }
        polygon(capA,tint); polygon(capB,tint);
    }
    void ellipsoid(Vec3 c,Vec3 radius,Tint tint,int segments=10,int rings=6) {
        if(radius.x<=.00001f || radius.y<=.00001f || radius.z<=.00001f) return;
        const float pixels=std::max(radius.x,std::max(radius.y,radius.z))*focal/std::max(1.f,camera(c).z);
        if(pixels>7) { segments=std::max(segments,meshDetail>1?24:12); rings=std::max(rings,meshDetail>1?12:6); }
        std::vector<Vec3> points,normals;
        std::vector<std::array<size_t,4>> quads;
        for(int row=0;row<=rings;++row) for(int col=0;col<segments;++col) {
            const float lat=-1.5707963f+row*3.14159265f/rings,lon=col*6.2831853f/segments;
            points.push_back(c+Vec3(radius.x*std::cos(lat)*std::cos(lon),radius.y*std::sin(lat),radius.z*std::cos(lat)*std::sin(lon)));
            normals.push_back(unit(Vec3(std::cos(lat)*std::cos(lon)/radius.x,std::sin(lat)/radius.y,std::cos(lat)*std::sin(lon)/radius.z)));
        }
        for(int row=0;row<rings;++row) for(int col=0;col<segments;++col) {
            const size_t a=row*segments+col,b=row*segments+(col+1)%segments;
            quads.push_back({{a+segments,b+segments,b,a}});
        }
        indexedQuads(points,normals,quads,tint);
    }
    Vec3 rotateNormal(Vec3 n) const {
        if(modelPitch==0 && modelYaw==0) return n;
        const Vec3 p(n.x,n.y*std::cos(modelPitch)-n.z*std::sin(modelPitch),n.y*std::sin(modelPitch)+n.z*std::cos(modelPitch));
        return Vec3(p.x*std::cos(modelYaw)+p.z*std::sin(modelYaw),p.y,-p.x*std::sin(modelYaw)+p.z*std::cos(modelYaw));
    }
    Vec3 modelPoint(Vec3 p) const {
        if(modelPitch==0 && modelYaw==0 && modelScale==1) return p+modelTranslation;
        return modelOrigin+modelTranslation+rotateNormal((p-modelOrigin)*modelScale);
    }
    Tint lightVertex(Vec3 p,Vec3 normal,Tint base) const {
        const Vec3 n=unit(normal),light=unit(Vec3(-.55f,1,-.7f)),view=unit(eye-p);
        const float diffuse=std::max(0.f,dot(n,light));
        const float sky=.5f+.5f*n.y;
        const float fill=std::max(0.f,dot(n,unit(Vec3(.7f,.2f,.5f))));
        const float spec=std::pow(std::max(0.f,dot(n,unit(light+view))),28.f)*gloss*190;
        const float rim=std::pow(1-std::max(0.f,dot(n,view)),3.f)*std::max(0.f,n.y)*.12f;
        const float value=.28f+.56f*diffuse+.15f*sky+.10f*fill+rim;
        return Tint(std::min(255.f,base.r*value+spec),std::min(255.f,base.g*(value+.025f*fill)+spec*.94f),
            std::min(255.f,base.b*(value+.07f*fill)+spec*.84f));
    }
    struct ShadedVertex { Vec3 p; Tint c; };
    ShadedVertex shadeVertex(Vec3 world,Vec3 normal,Tint tint) const {
        const Vec3 p=camera(world);
        Tint c=lightVertex(world,rotateNormal(normal),tint);
        const float fog=std::max(0.f,std::min(.80f,(p.z-fogStart)/fogRange));
        c=Tint(c.r*(1-fog)+fogColor.r*fog,c.g*(1-fog)+fogColor.g*fog,c.b*(1-fog)+fogColor.b*fog);
        return {p,c};
    }
    static std::vector<ShadedVertex> clipShaded(std::vector<ShadedVertex> in,int axis,float edge,bool greater) {
        std::vector<ShadedVertex> out;
        if(in.empty()) return out;
        size_t inside=0;
        for(const auto &v:in) {
            const float value=axis==0?v.p.x:(axis==1?v.p.y:v.p.z);
            if(greater?value>=edge:value<=edge) ++inside;
        }
        if(inside==in.size()) return in;
        if(!inside) return out;
        out.reserve(in.size()+1);
        ShadedVertex a=in.back();
        float av=(axis==0?a.p.x:(axis==1?a.p.y:a.p.z))-edge;
        for(const auto &b:in) {
            const float bv=(axis==0?b.p.x:(axis==1?b.p.y:b.p.z))-edge;
            const bool ai=greater?av>=0:av<=0,bi=greater?bv>=0:bv<=0;
            if(ai!=bi) {
                const float t=av/(av-bv);
                out.push_back({a.p+(b.p-a.p)*t,Tint(a.c.r+(b.c.r-a.c.r)*t,a.c.g+(b.c.g-a.c.g)*t,a.c.b+(b.c.b-a.c.b)*t)});
            }
            if(bi) out.push_back(b);
            a=b; av=bv;
        }
        return out;
    }
    void smoothPolygon(std::vector<Vec3> points,const std::vector<Vec3> &normals,Tint tint,int layer=4,bool twoSided=false) {
        if(points.size()<3 || points.size()!=normals.size()) return;
        for(auto &p:points) p=modelPoint(p);
        Vec3 faceNormal;
        for(size_t i=2;i<points.size();++i) {
            faceNormal=cross(points[i-1]-points[0],points[i]-points[0]);
            if(dot(faceNormal,faceNormal)>.00000001f) break;
        }
        if(!twoSided && dot(faceNormal,eye-points[0])<=0) return;
        std::vector<ShadedVertex> vertices; vertices.reserve(points.size());
        for(size_t i=0;i<points.size();++i) vertices.push_back(shadeVertex(points[i],normals[i],tint));
        emitShaded(std::move(vertices),layer);
    }
    // Project/light a shared vertex once, instead of repeating those operations
    // for every adjacent face of a dense model. Uses the same clipping path.
    void indexedQuads(const std::vector<Vec3> &points,const std::vector<Vec3> &normals,
        const std::vector<std::array<size_t,4>> &quads,Tint tint,int layer=4,bool twoSided=false,const std::vector<Tint> *vertexTints=NULL) {
        if(points.size()!=normals.size()) return;
        if(vertexTints && vertexTints->size()!=points.size()) return;
        std::vector<Vec3> world; world.reserve(points.size());
        std::vector<ShadedVertex> shaded; shaded.reserve(points.size());
        for(size_t i=0;i<points.size();++i) {
            world.push_back(modelPoint(points[i]));
            shaded.push_back(shadeVertex(world.back(),normals[i],vertexTints?(*vertexTints)[i]:tint));
        }
        for(const auto &q:quads) {
            if(q[0]>=points.size()||q[1]>=points.size()||q[2]>=points.size()||q[3]>=points.size()) continue;
            Vec3 normal=cross(world[q[1]]-world[q[0]],world[q[2]]-world[q[0]]);
            if(dot(normal,normal)<.00000001f) normal=cross(world[q[2]]-world[q[0]],world[q[3]]-world[q[0]]);
            if(!twoSided && dot(normal,eye-world[q[0]])<=0) continue;
            emitShaded({shaded[q[0]],shaded[q[1]],shaded[q[2]],shaded[q[3]]},layer);
        }
    }
    void emitShaded(std::vector<ShadedVertex> vertices,int layer) {
        float depth=0;
        for(const auto &v:vertices) depth+=v.p.z;
        depth/=vertices.size(); vertices=clipShaded(std::move(vertices),2,.3f,true);
        for(auto &v:vertices) v.p=project(v.p);
        vertices=clipShaded(std::move(vertices),0,0,true); vertices=clipShaded(std::move(vertices),0,width,false);
        vertices=clipShaded(std::move(vertices),1,0,true); vertices=clipShaded(std::move(vertices),1,height,false);
        if(vertices.size()<3) return;
        Face face; face.tint=Tint(); face.layer=layer; face.depth=depth;
        face.points.reserve(vertices.size()); face.colors.reserve(vertices.size());
        for(const auto &v:vertices) {
            face.points.push_back(v.p); face.colors.push_back(v.c);
            face.tint.r+=v.c.r/vertices.size(); face.tint.g+=v.c.g/vertices.size(); face.tint.b+=v.c.b/vertices.size();
        }
        faces.push_back(std::move(face));
    }
    Vec3 project(Vec3 p) const {
        return Vec3(centerX+focal*p.x/p.z,centerY-focal*p.y/p.z,1.f/p.z);
    }
    // Clip convex polygons both at the near plane and at the playfield edge.
    // Thus scenery never spills into other recorder widgets, even when resized.
    static std::vector<Vec3> clip(const std::vector<Vec3> &in,int axis,float edge,bool greater) {
        std::vector<Vec3> out;
        if(in.empty()) return out;
        Vec3 a=in.back();
        float av=(axis==0?a.x:(axis==1?a.y:a.z))-edge;
        for(size_t i=0;i<in.size();++i) {
            const Vec3 b=in[i];
            const float bv=(axis==0?b.x:(axis==1?b.y:b.z))-edge;
            const bool ai=greater?av>=0:av<=0, bi=greater?bv>=0:bv<=0;
            if(ai!=bi) out.push_back(a+(b-a)*(av/(av-bv)));
            if(bi) out.push_back(b);
            a=b; av=bv;
        }
        return out;
    }
    void polygon(std::vector<Vec3> points,Tint tint,int layer=4,bool lit=true) {
        if(points.size()<3) return;
        if(modelPitch!=0 || modelYaw!=0 || modelScale!=1 || dot(modelTranslation,modelTranslation)>0) {
            const float c=std::cos(modelPitch),s=std::sin(modelPitch);
            const float cy=std::cos(modelYaw),sy=std::sin(modelYaw);
            for(size_t i=0;i<points.size();++i) {
                const Vec3 p=(points[i]-modelOrigin)*modelScale;
                const Vec3 pitched(p.x,p.y*c-p.z*s,p.y*s+p.z*c);
                points[i]=modelOrigin+modelTranslation+Vec3(pitched.x*cy+pitched.z*sy,pitched.y,-pitched.x*sy+pitched.z*cy);
            }
        }
        if(lit) {
            const Vec3 normal=unit(cross(points[1]-points[0],points[2]-points[0]));
            if(dot(normal,eye-points[0])<=0) return;
            tint=tint.shade(.54f+.46f*std::max(0.0f,dot(normal,unit(Vec3(-.6f,1,-.7f)))));
        }
        float depth=0;
        for(size_t i=0;i<points.size();++i) { points[i]=camera(points[i]); depth+=points[i].z; }
        depth/=points.size();
        points=clip(points,2,.3f,true);
        for(size_t i=0;i<points.size();++i) points[i]=project(points[i]);
        points=clip(points,0,0,true); points=clip(points,0,static_cast<float>(width),false);
        points=clip(points,1,0,true); points=clip(points,1,static_cast<float>(height),false);
        if(points.size()<3) return;
        const float fog=std::max(0.0f,std::min(.80f,(depth-fogStart)/fogRange));
        tint=Tint(tint.r*(1-fog)+fogColor.r*fog,tint.g*(1-fog)+fogColor.g*fog,tint.b*(1-fog)+fogColor.b*fog);
        Face face; face.points=points; face.tint=tint; face.depth=depth; face.layer=layer;
        faces.push_back(std::move(face));
    }
    void box(Vec3 c,Vec3 size,Tint tint,float pitch=0) {
        Vec3 v[8];
        for(int i=0;i<8;++i) {
            const float x=(i&1?.5f:-.5f)*size.x;
            const float y=(i&2?.5f:-.5f)*size.y;
            const float z=(i&4?.5f:-.5f)*size.z;
            v[i]=c+Vec3(x,y*std::cos(pitch)-z*std::sin(pitch),y*std::sin(pitch)+z*std::cos(pitch));
        }
        const int sides[6][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}};
        for(int i=0;i<6;++i) polygon({v[sides[i][0]],v[sides[i][1]],v[sides[i][2]],v[sides[i][3]]},tint);
    }
    void mountain(float x,float z,float radius,float peak,Tint tint) {
        const Vec3 top(x,peak,z);
        const Vec3 base[4]={Vec3(x-radius,-.06f,z-radius),Vec3(x-radius,-.06f,z+radius),
                            Vec3(x+radius,-.06f,z+radius),Vec3(x+radius,-.06f,z-radius)};
        for(int i=0;i<4;++i) polygon({base[i],base[(i+1)%4],top},tint);
    }
    void ground(float left,float rightEdge,float nearZ,float farZ,Tint tint,int layer=0) {
        polygon({Vec3(left,-.04f,nearZ),Vec3(left,-.04f,farZ),
                 Vec3(rightEdge,-.04f,farZ),Vec3(rightEdge,-.04f,nearZ)},tint,layer,false);
    }
    void shadow(float x,float z,float rx,float rz,Tint tint) {
        std::vector<Vec3> v;
        for(int i=0;i<40;++i) {
            const float a=i*6.2831853f/40;
            v.push_back(Vec3(x+std::cos(a)*rx,0,z+std::sin(a)*rz));
        }
        polygon(v,tint,3,false);
    }
    void sort() {
        std::stable_sort(faces.begin(),faces.end(),[](const Face &a,const Face &b) {
            return a.layer==b.layer?a.depth>b.depth:a.layer<b.layer;
        });
    }
};


} }
#endif
