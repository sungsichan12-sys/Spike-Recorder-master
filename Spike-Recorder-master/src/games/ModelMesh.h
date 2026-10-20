#ifndef BYB_MODEL_MESH_H
#define BYB_MODEL_MESH_H
#include "Scene3D.h"
namespace BackyardBrains { namespace ModelMesh {
using namespace Scene3D;
inline float curve(float a,float b,float c,float d,float t) {
    return .5f*(2*b+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t);
}
inline Vec3 curve(Vec3 a,Vec3 b,Vec3 c,Vec3 d,float t) {
    return Vec3(curve(a.x,b.x,c.x,d.x,t),curve(a.y,b.y,c.y,d.y,t),curve(a.z,b.z,c.z,d.z,t));
}
inline void ringMesh(Scene &s,const std::vector<Vec3> &points,const std::vector<Vec3> &normals,int sides,Tint color,bool reverse=false,bool closed=false,const std::vector<Tint> *vertexTints=NULL) {
    if(sides<3 || points.size()<static_cast<size_t>(sides)*2 || points.size()!=normals.size()) return;
    const size_t rows=points.size()/sides;
    std::vector<std::array<size_t,4>> quads; quads.reserve(rows*sides);
    for(size_t row=0;row<(closed?rows:rows-1);++row) for(int col=0;col<sides;++col) {
        const size_t a=row*sides+col,b=row*sides+(col+1)%sides,c=((row+1)%rows)*sides+(col+1)%sides,d=((row+1)%rows)*sides+col;
        quads.push_back(reverse?std::array<size_t,4>{{a,d,c,b}}:std::array<size_t,4>{{a,b,c,d}});
    }
    s.indexedQuads(points,normals,quads,color,4,false,vertexTints);
}
struct SweepPoint {
    Vec3 center; float radius,aspect;
    SweepPoint(Vec3 c,float r,float a=1):center(c),radius(r),aspect(a) {}
};
// Transport the cross-section frame along the curve. Choosing a fresh global
// up vector at every bend introduces visible flips in horns and bent limbs.
inline void sweep(Scene &s,const std::vector<SweepPoint> &source,Tint tint,int sides=10) {
    if(source.size()<2) return;
    float pixels=0;
    for(const auto &p:source) pixels=std::max(pixels,p.radius*s.focal*std::fabs(s.modelScale)/std::max(.3f,s.camera(s.modelPoint(p.center)).z));
    const bool dense=s.meshDetail>1 && pixels>3;
    if(dense) sides=std::max(sides,pixels>12?20:16);
    std::vector<SweepPoint> path;
    const int samples=dense && source.size()<12?2:1;
    for(size_t i=0;i+1<source.size();++i) {
        const auto &a=source[i?i-1:i],&b=source[i],&c=source[i+1],&d=source[std::min(i+2,source.size()-1)];
        for(int j=0;j<samples;++j) {
            const float t=j/static_cast<float>(samples);
            const float radius=std::max(std::min(b.radius,c.radius),std::min(std::max(b.radius,c.radius),curve(a.radius,b.radius,c.radius,d.radius,t)));
            path.push_back(SweepPoint(curve(a.center,b.center,c.center,d.center,t),radius,b.aspect+(c.aspect-b.aspect)*t));
        }
    }
    path.push_back(source.back());
    std::vector<std::vector<Vec3>> rings;
    std::vector<Vec3> tangents;
    Vec3 frame;
    for(size_t row=0;row<path.size();++row) {
        const Vec3 tangent=unit(path[std::min(row+1,path.size()-1)].center-path[row?row-1:row].center);
        Vec3 u=frame-tangent*dot(frame,tangent);
        if(dot(u,u)<.000001f) u=cross(tangent,std::fabs(tangent.y)>.9f?Vec3(1,0,0):Vec3(0,1,0));
        u=unit(u); const Vec3 v=cross(tangent,u); frame=u;
        std::vector<Vec3> ring;
        for(int i=0;i<sides;++i) {
            const float a=i*6.2831853f/sides;
            ring.push_back(path[row].center+(u*std::cos(a)+v*(std::sin(a)*path[row].aspect))*std::max(.0001f,path[row].radius));
        }
        rings.push_back(ring); tangents.push_back(tangent);
    }
    std::vector<Vec3> points,normals;
    for(size_t row=0;row<path.size();++row) {
        std::vector<Vec3> normal;
        for(int i=0;i<sides;++i) {
            const Vec3 around=rings[row][(i+1)%sides]-rings[row][(i+sides-1)%sides];
            const Vec3 along=rings[std::min(row+1,path.size()-1)][i]-rings[row?row-1:row][i];
            normal.push_back(unit(cross(around,along)));
        }
        points.insert(points.end(),rings[row].begin(),rings[row].end());
        normals.insert(normals.end(),normal.begin(),normal.end());
    }
    ringMesh(s,points,normals,sides,tint);
    std::vector<Vec3> first=rings.front(); std::reverse(first.begin(),first.end());
    s.smoothPolygon(first,std::vector<Vec3>(sides,tangents.front()*-1),tint);
    s.smoothPolygon(rings.back(),std::vector<Vec3>(sides,tangents.back()),tint);
}
struct ProfilePoint { float radius,height; ProfilePoint(float r,float h):radius(r),height(h) {} };
// Profile order follows the upper surface from the center toward the edge,
// then returns underneath. This also supports a hollow, rounded metal hoop.
inline void revolve(Scene &s,Vec3 center,Vec3 axis,const std::vector<ProfilePoint> &source,Tint color,int sides=32,bool closed=false) {
    if(source.size()<2) return;
    const int samples=s.meshDetail>1?2:1;
    if(s.meshDetail>1) sides=std::max(sides,64);
    std::vector<ProfilePoint> profile;
    const int count=static_cast<int>(source.size());
    for(int i=0;i<(closed?count:count-1);++i) {
        const auto &a=source[closed?(i+count-1)%count:std::max(0,i-1)],&b=source[i],&c=source[(i+1)%count],&d=source[closed?(i+2)%count:std::min(count-1,i+2)];
        for(int j=0;j<samples;++j) {
            const float t=j/static_cast<float>(samples);
            // Preserve the bearing edge and skin clearance at tight corners.
            profile.push_back(ProfilePoint(std::max(std::min(b.radius,c.radius),std::min(std::max(b.radius,c.radius),curve(a.radius,b.radius,c.radius,d.radius,t))),
                std::max(std::min(b.height,c.height),std::min(std::max(b.height,c.height),curve(a.height,b.height,c.height,d.height,t)))));
        }
    }
    if(!closed) profile.push_back(source.back());
    axis=unit(axis);
    const Vec3 u=unit(cross(axis,std::fabs(axis.y)>.9f?Vec3(1,0,0):Vec3(0,1,0))),v=cross(axis,u);
    const int rows=static_cast<int>(profile.size());
    std::vector<Vec3> points,normals;
    for(int row=0;row<rows;++row) for(int i=0;i<sides;++i) {
        const int before=closed?(row+rows-1)%rows:std::max(0,row-1),after=closed?(row+1)%rows:std::min(rows-1,row+1);
        const float a=i*6.2831853f/sides;
        const Vec3 radial=u*std::cos(a)+v*std::sin(a);
        const float dr=profile[after].radius-profile[before].radius,dh=profile[after].height-profile[before].height;
        points.push_back(center+radial*profile[row].radius+axis*profile[row].height);
        normals.push_back(unit(radial*(-dh)+axis*dr));
    }
    ringMesh(s,points,normals,sides,color,true,closed);
}
inline void hoop(Scene &s,Vec3 center,float radius,float thickness,Tint tint,int sides=32) {
    std::vector<ProfilePoint> profile;
    for(int i=0;i<6;++i) {
        const float a=2.6179939f-i*6.2831853f/6;
        profile.push_back(ProfilePoint(radius+thickness*std::cos(a),thickness*std::sin(a)));
    }
    revolve(s,center,Vec3(0,1,0),profile,tint,sides,true);
}
inline void roundedBox(Scene &s,Vec3 center,Vec3 size,float bevel,Tint color) {
    const Vec3 half=size*.5f;
    bevel=std::min(bevel,std::min(half.x,std::min(half.y,half.z))*.95f);
    const float h[]={half.x,half.y,half.z};
    for(int axis=0;axis<3;++axis) for(int side=-1;side<=1;side+=2) {
        const int u=(axis+1)%3,v=(axis+2)%3;
        const int steps=s.meshDetail>1?3:1;
        std::vector<float> us,vs;
        for(int i=0;i<=steps;++i) {
            us.push_back(-h[u]+bevel*i/steps); vs.push_back(-h[v]+bevel*i/steps);
        }
        for(int i=0;i<=steps;++i) {
            us.push_back(h[u]-bevel+bevel*i/steps); vs.push_back(h[v]-bevel+bevel*i/steps);
        }
        const int cells=static_cast<int>(us.size())-1;
        for(int row=0;row<cells;++row) for(int col=0;col<cells;++col) {
            const int rows[]={row,row+1,row+1,row},cols[]={col,col,col+1,col+1};
            std::vector<Vec3> points,normals;
            for(int k=0;k<4;++k) {
                float p[3]={0,0,0}; p[axis]=side*h[axis]; p[u]=us[rows[k]]; p[v]=vs[cols[k]];
                const Vec3 point(p[0],p[1],p[2]);
                const Vec3 core(std::max(-half.x+bevel,std::min(half.x-bevel,p[0])),
                    std::max(-half.y+bevel,std::min(half.y-bevel,p[1])),std::max(-half.z+bevel,std::min(half.z-bevel,p[2])));
                const Vec3 normal=unit(point-core);
                points.push_back(center+core+normal*bevel); normals.push_back(normal);
            }
            if(side<0) { std::reverse(points.begin(),points.end()); std::reverse(normals.begin(),normals.end()); }
            s.smoothPolygon(points,normals,color);
        }
    }
}
} }
#endif
