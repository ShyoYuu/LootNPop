"""C++ 원래 길/섬의 동쪽 절단면을 double 수식으로 재구성해 후보와 비교한다."""

import json
import math
from pathlib import Path

import numpy as np


def verify(folder,axis="x"):
    folder = Path(folder)
    radial = np.array((1., 1., 1.))/math.sqrt(3)
    tangent = np.array((-1., 1., 0.))/math.sqrt(2)
    bitangent = np.array((-1., -1., 2.))/math.sqrt(6)

    def point(p):
        x, y, h = p
        v = radial*30000+bitangent*x+tangent*y
        return v/np.linalg.norm(v)*(30000-h)

    corners = [(-14000,7800,0),(-7000,7800,0),(-7000,12000,0),(-14000,12000,0)]
    edge_index = 1 if axis == "x" else 2
    normal = np.cross(point(corners[edge_index]),point(corners[(edge_index+1)%4]))
    normal /= np.linalg.norm(normal)
    u = tangent-normal*np.dot(tangent,normal)
    u /= np.linalg.norm(u)
    v = np.cross(normal,u)
    basis = np.array((u,v))

    def section(triangles):
        segments = []
        for triangle in triangles:
            points = np.array([point(p) for p in triangle])
            dots = points@normal
            hits = []
            for i in range(3):
                j = (i+1)%3
                if dots[i]*dots[j] < 0:
                    hits.append(points[i]+(points[j]-points[i])*dots[i]/(dots[i]-dots[j]))
                elif abs(dots[i]) < 1e-8:
                    hits.append(points[i])
            hits = list({tuple(np.round(p,7)):p for p in hits}.values())
            if len(hits) == 2 and np.linalg.norm(hits[1]-hits[0]) > 1e-6:
                segments.append(tuple(basis@p for p in hits))
        adjacency, positions = {}, {}
        for a,b in segments:
            ka,kb = tuple(np.round(a,5)),tuple(np.round(b,5))
            if ka == kb:
                continue
            positions[ka],positions[kb] = a,b
            adjacency.setdefault(ka,set()).add(kb)
            adjacency.setdefault(kb,set()).add(ka)
        if not adjacency:
            return []
        if any(len(neighbors) != 2 for neighbors in adjacency.values()):
            raise ValueError("C++ section is not a closed contour")
        start = next(iter(adjacency))
        keys,previous,current = [],None,start
        while current not in keys:
            keys.append(current)
            previous,current = current,next(p for p in adjacency[current] if p != previous)
        if current != start or len(keys) != len(adjacency):
            raise ValueError("C++ section contains multiple contours")
        return [positions[k] for k in keys]

    def quad(triangles,a,b,c,d):
        triangles.extend(((a,b,c),(a,c,d)))

    def bridge(a,b,width):
        a,b = np.array(a,dtype=float),np.array(b,dtype=float)
        side = np.array((-(b-a)[1],(b-a)[0]))/np.linalg.norm((b-a)[:2])
        steps,bands = max(2,math.ceil(np.linalg.norm((b-a)[:2])/200)),max(2,math.ceil(width/200))
        def at(t,l,bottom=False):
            p = a+(b-a)*t
            xy = p[:2]+side*l*width*(.9 if bottom else .5)*30000/(30000-p[2])
            return (*xy,-2000 if bottom else p[2])
        triangles = []
        for i in range(steps):
            t0,t1 = i/steps,(i+1)/steps
            for j in range(bands):
                l,r = -1+2*j/bands,-1+2*(j+1)/bands
                quad(triangles,at(t0,l),at(t1,l),at(t1,r),at(t0,r))
            for j in range(bands):
                l,r = -1+2*j/bands,-1+2*(j+1)/bands
                quad(triangles,at(t0,l,True),at(t1,l,True),at(t1,r,True),at(t0,r,True))
            for l in (-1,1):
                quad(triangles,at(t0,l,True),at(t1,l,True),at(t1,l),at(t0,l))
        for t in (0,1):
            for j in range(bands):
                l,r = -1+2*j/bands,-1+2*(j+1)/bands
                quad(triangles,at(t,l,True),at(t,r,True),at(t,r),at(t,l))
        return section(triangles)

    triangles = []
    radius,sides,rings = 1600*30000/27000,24,4
    def at(r,s,h):
        angle = math.tau*s/sides
        return (-8500+r*math.cos(angle),9800+r*math.sin(angle),h)
    for h in (2400,3000):
        for s in range(sides):
            triangles.append((at(0,0,h),at(radius/rings,s,h),at(radius/rings,s+1,h)))
            for r in range(1,rings):
                quad(triangles,at(radius*r/rings,s,h),at(radius*r/rings,s+1,h),
                     at(radius*(r+1)/rings,s+1,h),at(radius*(r+1)/rings,s,h))
    for s in range(sides):
        quad(triangles,at(radius,s,2400),at(radius,s+1,2400),at(radius,s+1,3000),at(radius,s,3000))
    polygons = [section(triangles),
                bridge((-6960,10610,3000),(-7700,10210,3000),1600),
                bridge((-4300,12000,1600),(-6960,10610,3000),1600),
                bridge((-7300,9500,3000),(-6500,9500,3000),800)]
    polygons = [p for p in polygons if p]

    def inside(p,polygon):
        result = False
        for a,b in zip(polygon,polygon[1:]+polygon[:1]):
            if (a[1]>p[1]) != (b[1]>p[1]) and p[0] < (b[0]-a[0])*(p[1]-a[1])/(b[1]-a[1])+a[0]:
                result = not result
        return result

    def cross(a,b):
        return a[0]*b[1]-a[1]*b[0]

    segments = []
    for index,polygon in enumerate(polygons):
        for a,b in zip(polygon,polygon[1:]+polygon[:1]):
            direction = b-a
            cuts = [0.,1.]
            for other in polygons[:index]+polygons[index+1:]:
                for c,d in zip(other,other[1:]+other[:1]):
                    denominator = cross(direction,d-c)
                    if abs(denominator) < 1e-10:
                        continue
                    t,s = cross(c-a,d-c)/denominator,cross(c-a,direction)/denominator
                    if 0<t<1 and -1e-8<=s<=1+1e-8:
                        cuts.append(t)
            cuts = sorted(set(cuts))
            for t0,t1 in zip(cuts,cuts[1:]):
                if not any(inside(a+direction*(t0+t1)/2,other) for other in polygons[:index]+polygons[index+1:]):
                    segments.append((a+direction*t0,a+direction*t1))
    # 교체 구역 밖으로 이어지는 원래 단면은 비교 대상에서 절단한다.
    for i in range(4):
        if i == edge_index:
            continue
        plane = np.cross(point(corners[i]),point(corners[(i+1)%4]))
        if np.dot(plane,point((-10500,9900,0))) < 0:
            plane = -plane
        plane /= np.linalg.norm(plane)
        clipped = []
        for a,b in segments:
            da,db = np.dot(a@basis,plane),np.dot(b@basis,plane)
            if da < -1e-7 and db < -1e-7:
                continue
            if da*db < 0:
                intersection = a+(b-a)*da/(da-db)
                a,b = (intersection,b) if da < 0 else (a,intersection)
            if np.linalg.norm(b-a)>1e-6:
                clipped.append((a,b))
        segments = clipped
    manifest = json.loads((folder/"manifest.json").read_text(encoding="utf-8"))
    authored = []
    for edge in manifest["interface_edges_blender_m"]:
        points = np.array([(p[0]*100,-p[1]*100,p[2]*100) for p in edge])
        if max(abs(points@normal)) > .05:
            continue
        authored.append(tuple(basis@p for p in points))

    def distance(p,edge):
        a,b = edge
        direction = b-a
        t = np.clip(np.dot(p-a,direction)/np.dot(direction,direction),0,1)
        return np.linalg.norm(p-(a+direction*t))

    def compare(source,target,minimum_h=-math.inf):
        worst = (0,None)
        for a,b in source:
            for t in np.linspace(0,1,21):
                p = a+(b-a)*t
                if 30000-np.linalg.norm(p@basis) < minimum_h:
                    continue
                delta = min(distance(p,e) for e in target)
                if delta > worst[0]:
                    world = p@basis
                    local = [30000*np.dot(world,bitangent)/np.dot(world,radial),
                             30000*np.dot(world,tangent)/np.dot(world,radial),30000-np.linalg.norm(world)]
                    worst = (float(delta),local)
        return {"max_cm":worst[0],"local_xyh_cm":worst[1]}
    result = {"axis":axis,"cppExteriorSegments":len(segments),"authoredSegments":len(authored),
              "authoredToCpp":compare(authored,segments),"cppToAuthored":compare(segments,authored),
              "aboveHMinus500AuthoredToCpp":compare(authored,segments,-500),
              "aboveHMinus500CppToAuthored":compare(segments,authored,-500),
              "scope":"Independent double reconstruction of exterior with subdivided Rim bridge bases; excludes cylinder inner plate and UE capsule"}
    (folder/f"seam-target-{axis}.json").write_text(json.dumps({"normal_ue":normal.tolist(),
                  "segments_ue_cm":[[(p@basis).tolist() for p in edge] for edge in segments]},indent=2),encoding="utf-8")
    return result


if __name__ == "__main__":
    import sys
    result = [verify(sys.argv[1],axis) for axis in ("x","y")]
    (Path(sys.argv[1])/"seam-verification.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    print(json.dumps(result,indent=2))
