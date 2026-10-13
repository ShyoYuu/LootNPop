"""Acropolis_00: 기준 이음매를 보존한 부유 유적 그레이박스.

빈 Blender 백그라운드 프로세스에서 실행한다. 기존 에셋과 장면을 덮어쓰지 않는다.
지각·섬·작은 지하방의 소유권과 마커 배치를 manifest에 함께 보존한다.
"""

import json
import math
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
VERSION = "V015"
OUT = ROOT / "Saved/SliceOctant/Acropolis_00" / VERSION
ART = ROOT / "Art/Octants/Acropolis_00"
R = 30000.0
RADIAL = Vector((1, 1, 1)).normalized()
TANGENT = Vector((-1, 1, 0)).normalized()
BITANGENT = Vector((-1, -1, 2)).normalized()
GROUND = -3500.0
POCKET_RAMP = 5000.0
POCKET_END = 7600.0
POCKET_FLOOR = -7100.0
POCKET_CEILING = -3900.0
EXIT_ROUTES = [{"id":f"Exit_{axis}_{i+1:02d}","axis":axis,"angle_deg":angle}
               for axis,angles in enumerate(((20,43.5,70),(20,32,73),(20,51,78)))
               for i,angle in enumerate(angles)]
EXPORTS = []
SOURCES = []


def point(x, y, h):
    return (R * RADIAL + x * BITANGENT + y * TANGENT).normalized() * (R-h)


def blender(p):
    return Vector((p.x/100, -p.y/100, p.z/100))


def ue(p):
    return Vector((p.x*100, -p.y*100, p.z*100))


def ground(x, y):
    # 낮은 중앙 언덕의 약 70m 폭 정상을 비우고 모든 방향에서 걸어 오르게 한다.
    d = math.hypot(x, y)
    t = min(1.0, max(0.0, (d-3300)/3300))
    return GROUND + 1700*(1-t*t*(3-2*t))


def exit_blend(p,original):
    # 각 경계의 보존 띠에서 내부 지면까지 길이 약 90m의 완만한 복귀로를 만든다.
    weight,coverage=original,0.0
    for route in EXIT_ROUTES:
        axis=route["axis"]
        others=[i for i in range(3) if i!=axis]
        angle=math.atan2(p[others[0]],p[others[1]])
        lateral=abs(angle-math.radians(route["angle_deg"]))*R
        side=min(1,max(0,(2600-lateral)/1100))
        side=side*side*(3-2*side)
        # X 꼭짓점 쪽 두 길은 같은 높이장을 공유해 교차부의 옆면이 길을 가르지 않게 한다.
        t=(29000-p[0])/4500 if axis in (1,2) and route["angle_deg"] in (73,78) else (p[axis]-1500)/8500
        t=min(1,max(0,t))
        ramp=t*t*(3-2*t)
        weight=min(weight,original+(ramp-original)*side)
        if ramp<1:
            coverage=max(coverage,side)
    return weight,coverage


def mesh(name, vertices, faces):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def stats(bm):
    bm.verts.index_update()
    keys = [tuple(sorted(v.index for v in f.verts)) for f in bm.faces]
    return {"vertices": len(bm.verts), "triangles": len(bm.faces),
            "boundary_edges": sum(e.is_boundary for e in bm.edges),
            "non_manifold_non_boundary": sum(not e.is_manifold and not e.is_boundary for e in bm.edges),
            "inconsistent_edges": sum(e.is_manifold and not e.is_contiguous for e in bm.edges),
            "duplicate_faces": len(keys)-len(set(keys)),
            "degenerate_faces": sum(f.calc_area() < 1e-8 for f in bm.faces)}


def export_obj(obj, role):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    obj.data.calc_loop_triangles()
    vertices = [ue(v.co) for v in obj.data.vertices]
    path = OUT / (obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
        apply_scale_options="FBX_SCALE_NONE", global_scale=1.0, axis_forward="-Z", axis_up="Y",
        mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    EXPORTS.append({"name": obj.name, "role": role, "file": str(path),
        "triangles": len(obj.data.loop_triangles),
        "bounds_cm": {"min": [min(v[i] for v in vertices) for i in range(3)],
                      "max": [max(v[i] for v in vertices) for i in range(3)]}})


def finish(name, vertices, faces, force_shell=False):
    obj = mesh(name, vertices, faces)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    for _ in range(7):
        edges = [e for e in bm.edges if e.calc_length() > 280]
        if not edges:
            break
        bmesh.ops.subdivide_edges(bm, edges=edges, cuts=1, use_grid_fill=True)
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
    for v in bm.verts:
        v.co = blender(point(*v.co))
    remaining = set(bm.verts)
    while remaining:
        group = {remaining.pop()}
        stack = list(group)
        while stack:
            for edge in stack.pop().link_edges:
                for v in edge.verts:
                    if v in remaining:
                        remaining.remove(v)
                        group.add(v)
                        stack.append(v)
        bmesh.ops.remove_doubles(bm, verts=list(group), dist=0.00001)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges[:], dist=0.00001)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    # 오목한 윤곽을 분할·병합하면서 생긴 동일 삼각형만 정리한다.
    bm.verts.index_update()
    seen, duplicates = set(), []
    for face in bm.faces:
        key=tuple(sorted(v.index for v in face.verts))
        if key in seen:
            duplicates.append(face)
        seen.add(key)
    bmesh.ops.delete(bm,geom=duplicates,context="FACES_ONLY")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.normal_update()
    report = stats(bm)
    if any(report[k] for k in ("boundary_edges", "non_manifold_non_boundary",
                              "inconsistent_edges", "duplicate_faces", "degenerate_faces")):
        raise ValueError((name, report))
    SOURCES.append({"id": name, "topology": report})
    bm.verts.ensure_lookup_table()
    all_vertices = [v.co.copy() for v in bm.verts]
    groups = {"Floor": [], "Shell": []}
    for f in bm.faces:
        up = -f.calc_center_median().normalized()
        role = "Floor" if not force_shell and f.normal.dot(up) >= 0.71 else "Shell"
        groups[role].append(tuple(v.index for v in f.verts))
    bm.free()
    bpy.data.objects.remove(obj, do_unlink=True)
    for role, triangles in groups.items():
        if not triangles:
            continue
        used = sorted({i for triangle in triangles for i in triangle})
        remap = {old: i for i, old in enumerate(used)}
        output = mesh(f"SM_Acropolis_00_{name}_{role}", [all_vertices[i] for i in used],
                      [tuple(remap[i] for i in tri) for tri in triangles])
        export_obj(output, role)


def add_box(vertices, faces, bounds, transform=None, top_end=None):
    x0, x1, y0, y1, h0, h1 = bounds
    start = len(vertices)
    coords = [(x, y, h0) for x, y in ((x0,y0),(x1,y0),(x1,y1),(x0,y1))]
    coords += [(x, y, h1 if top_end is None or x == x0 else top_end)
               for x, y in ((x0,y0),(x1,y0),(x1,y1),(x0,y1))]
    vertices.extend(transform(*p) if transform else p for p in coords)
    faces.extend(tuple(start+i for i in f) for f in
        ((0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)))


def pocket_sections(angle,bend):
    a,b=math.radians(angle),math.radians(angle+bend)
    start=Vector((6800*math.cos(a),6800*math.sin(a)))
    turn=start+2500*Vector((math.cos(a),math.sin(a)))
    end=turn+2500*Vector((math.cos(b),math.sin(b)))
    last=end+2600*Vector((math.cos(b),math.sin(b)))
    normals=[Vector((-math.sin(a),math.cos(a))),
             Vector((-math.sin((a+b)/2),math.cos((a+b)/2)))/math.cos((b-a)/2),
             Vector((-math.sin(b),math.cos(b))),Vector((-math.sin(b),math.cos(b)))]
    sections=[]
    for s,p,n in zip((0,2500,5000,7600),(start,turn,end,last),normals):
        sections.append({"s":s,"center":list(p),"offset":list(n),
            "h":GROUND+(POCKET_FLOOR-GROUND)*min(1,s/POCKET_RAMP)})
    # 첫 구간의 두 가장자리를 같은 비율로 나눠 굽이 안쪽의 급경사를 피한다.
    sections.insert(1,{"s":1500,"center":list(start.lerp(turn,0.6)),
        "offset":list(normals[0].lerp(normals[1],0.6)),"h":GROUND+(POCKET_FLOOR-GROUND)*0.3})
    return sections


def pocket_edge(section,side,width=1500):
    return [section["center"][i]+side*width*section["offset"][i] for i in range(2)]


def add_strip(vertices,faces,sections,widths,bottom,top):
    start=len(vertices)
    for section in sections:
        for h in (bottom,section["h"] if top is None else top):
            for side,width in widths:
                vertices.append((*pocket_edge(section,side,width),h))
    faces.extend(tuple(start+i for i in f) for f in ((0,2,3,1),
        tuple(4*(len(sections)-1)+i for i in (0,1,3,2))))
    for j in range(len(sections)-1):
        a,b=start+4*j,start+4*(j+1)
        faces.extend(((a,a+1,b+1,b),(a+2,b+2,b+3,a+3),
                      (a,b,b+2,a+2),(a+1,a+3,b+3,b+1)))


def inside_outline(x,y,outline):
    inside=False
    for a,b in zip(outline,outline[1:]+outline[:1]):
        if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
            inside=not inside
    return inside


def build_crust(pockets):
    bpy.ops.import_scene.fbx(filepath=str(OUT.parent / "V001/MeadowSeam.fbx"))
    obj = next(o for o in bpy.context.selected_objects if o.type == "MESH")
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    seam = []
    original_seam = {}
    for v in bm.verts:
        p = ue(obj.matrix_world @ v.co)
        if min(p) <= 1500.01:
            seam.append(p.copy())
            original_seam[v] = p.copy()
        depth = p.dot(RADIAL)
        v.co = Vector((R*p.dot(BITANGENT)/depth, R*p.dot(TANGENT)/depth, R-p.length))
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.001)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.calc_area() < 0.0001], context="FACES")
    # Meadow 전용 동굴 입구는 신규 지형으로 이어받지 않는다. 경계 세 변은 그대로 둔다.
    candidates = {e for e in bm.edges if e.is_boundary}
    sealed = 0
    while candidates:
        group = {candidates.pop()}
        stack = list(group)
        while stack:
            edge = stack.pop()
            for v in edge.verts:
                for other in v.link_edges:
                    if other in candidates:
                        candidates.remove(other)
                        group.add(other)
                        stack.append(other)
        if min(min(point(*v.co)) for e in group for v in e.verts) > 1500:
            new = bmesh.ops.holes_fill(bm, edges=list(group), sides=0)["faces"]
            bmesh.ops.triangulate(bm, faces=new)
            sealed += 1
    for v in bm.verts:
        p = point(*v.co)
        weight = min(1.0, max(0.0, (min(p)-1500)/4000))
        weight = weight*weight*(3-2*weight)
        v.co.z += weight*(ground(v.co.x,v.co.y)-v.co.z)
        ramp,coverage=exit_blend(point(v.co.x,v.co.y,0),weight)
        preserve=min(1,max(0,(min(p)-1500)/2500))
        preserve=preserve*preserve*(3-2*preserve)
        v.co.z+=coverage*preserve*(ground(v.co.x,v.co.y)*ramp-v.co.z)
    # 방 전체의 소유 영역을 정확히 절단한다. 주변 지각과 동일한 구면 투영을 쓴다.
    for pocket in pockets:
        outline=pocket["outline_xy"]
        x0,x1=min(p[0] for p in outline)-500,max(p[0] for p in outline)+500
        y0,y1=min(p[1] for p in outline)-500,max(p[1] for p in outline)+500
        for a,b in zip(outline,outline[1:]+outline[:1]):
            normal=Vector((a[1]-b[1],b[0]-a[0],0)).normalized()
            region = [f for f in bm.faces
                      if x0 < f.calc_center_median().x < x1 and y0 < f.calc_center_median().y < y1]
            edges = {e for f in region for e in f.edges}
            vertices = {v for f in region for v in f.verts}
            bmesh.ops.bisect_plane(bm, geom=list(vertices)+list(edges)+region,
                dist=0.001, plane_co=Vector((*a,0)), plane_no=normal,
                clear_inner=False, clear_outer=False)
        inside = []
        for f in bm.faces:
            p = f.calc_center_median()
            if inside_outline(p.x,p.y,outline):
                inside.append(f)
        bmesh.ops.delete(bm, geom=inside, context="FACES")
    # Pocket_05 끝의 지각 전이 높이와 일정한 지붕 높이 사이를 닫는다.
    # 윗변은 실제 절단 교선을 공유하고, 아랫변은 기존 100cm 외벽 안에 매입한다.
    pocket=next(p for p in pockets if p["id"]=="Pocket_05")
    outline=pocket["outline_xy"]
    wall_edges=[]
    for edge in bm.edges:
        if not edge.is_boundary:
            continue
        midpoint=sum((v.co for v in edge.verts),Vector())/2
        for a,b in zip(outline,outline[1:]+outline[:1]):
            # 마지막 변은 열린 진입구이므로 매입면을 만들지 않는다.
            if a==outline[-1] and b==outline[0]:
                continue
            aa,bb=Vector(a),Vector(b)
            delta=bb-aa
            t=max(0,min(1,(Vector(midpoint[:2])-aa).dot(delta)/delta.length_squared))
            if (Vector(midpoint[:2])-aa-t*delta).length<0.05:
                wall_edges.append(edge)
                break
    middle,lower={},{}
    for edge in wall_edges:
        for v in edge.verts:
            if v not in lower:
                nearest=[]
                xy=Vector(v.co[:2])
                for a,b in zip(outline,outline[1:]+outline[:1]):
                    aa,bb=Vector(a),Vector(b);delta=bb-aa
                    t=max(0,min(1,(xy-aa).dot(delta)/delta.length_squared))
                    if (xy-aa-t*delta).length<0.05:
                        normal=Vector((-delta.y,delta.x)).normalized()
                        test=xy+normal*10
                        nearest.append(normal if inside_outline(*test,outline) else -normal)
                normal=sum(nearest,Vector((0,0))).normalized()
                width=50/max(normal.dot(nearest[0]),0.5)
                middle[v]=bm.verts.new((*xy,GROUND-5))
                lower[v]=bm.verts.new((* (xy+width*normal),GROUND-80))
        a,b=edge.verts
        bm.faces.new((a,b,middle[b],middle[a]))
        bm.faces.new((middle[a],middle[b],lower[b],lower[a]))
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    for v in bm.verts:
        v.co = blender(original_seam[v]) if v in original_seam else blender(point(*v.co))
    inner_vertices = [v for v in bm.verts if min(ue(v.co)) > 1500]
    bmesh.ops.remove_doubles(bm, verts=inner_vertices, dist=0.0001)
    inner_edges = [e for e in bm.edges if all(min(ue(v.co)) > 1500 for v in e.verts)]
    bmesh.ops.dissolve_degenerate(bm, edges=inner_edges, dist=0.0001)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.normal_update()
    if sum(f.normal.dot(-f.calc_center_median().normalized()) for f in bm.faces) < 0:
        for f in bm.faces:
            f.normal_flip()
    report = stats(bm)
    if any(report[k] for k in ("non_manifold_non_boundary", "inconsistent_edges",
                              "duplicate_faces", "degenerate_faces")):
        raise ValueError(("Crust", report, [(f.calc_area(), [list(ue(v.co)) for v in f.verts],
                           [e.calc_length() for e in f.edges]) for f in bm.faces if f.calc_area()<1e-8]))
    report["sealed_meadow_openings"] = sealed
    report["pocket_05_embedded_cut_edges"] = len(wall_edges)
    SOURCES.append({"id": "Crust", "topology": report})
    bm.to_mesh(obj.data)
    bm.free()
    obj.matrix_world.identity()
    obj.name = "SM_Octant_Acropolis_00_R30000"
    export_obj(obj, "Crust")
    # 반입 뒤 native 원본 좌표와 별도로 비교할 경계 표본이다.
    (OUT / "seam-source.json").write_text(json.dumps([list(p) for p in seam]), encoding="utf-8")
    return report


def build():
    if (OUT / "manifest.json").exists():
        raise ValueError("Version already exists; preserve it and choose a new version")
    OUT.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0
    pockets = [{"id": f"Pocket_{i+1:02d}", "angle": 18+i*72,"bend_deg":bend}
               for i,bend in enumerate((0,40,-40,40,-40))]
    for pocket in pockets:
        sections=pocket_sections(pocket["angle"],pocket["bend_deg"])
        pocket["sections"]=sections
        pocket["outline_xy"]=[pocket_edge(s,1) for s in sections]+[pocket_edge(s,-1) for s in reversed(sections)]
    crust = build_crust(pockets)
    islands = []
    for ring, count, radius, heights, offset in (
        ("Outer",12,14500,[1000,3000,5500,3000],0),
        ("Middle",8,10100,[6500,8500,7500,9500],15),
        ("Upper",4,9600,[11500,12500,11500,13000],35)):
        for i in range(count):
            angle = math.radians(offset+360*i/count)
            h = heights[i % len(heights)]
            actual_radius = 1800 if ring == "Outer" else 1900 if ring == "Middle" else 2100
            radius_xy = actual_radius*R/(R-h)
            islands.append({"id": f"{ring}_{i+1:02d}", "x": radius*math.cos(angle),
                "y": radius*math.sin(angle), "h": h, "radius_xy": radius_xy,
                "nominal_radius_cm": actual_radius})
    shapes = ("Small", "Broad", "Long", "Elbow", "Terrace", "Broad")
    for index, island in enumerate(islands):
        vertices, faces = [], []
        x,y,h,r = (island[k] for k in ("x","y","h","radius_xy"))
        shape = shapes[index % len(shapes)]
        island["shape"] = shape
        # 긴 축은 띠의 접선 쪽으로 향하게 하고 기존 중심·훅 아래 착지면은 보존한다.
        angle = math.atan2(y,x)+math.pi/2
        def local(u,v,height):
            return (x+r*(u*math.cos(angle)-v*math.sin(angle)),
                    y+r*(u*math.sin(angle)+v*math.cos(angle)),height)
        if shape == "Terrace":
            # 별도 겹친 상자 대신 경사로와 두 평탄부를 하나의 닫힌 외피로 만든다.
            sections = [(-1.45,0), (0.25,0), (1.05,700), (1.65,700)]
            for u,rise in sections:
                vertices.extend(local(u,v,height) for height in (h-650,h+rise) for v in (-1.1,1.1))
            faces.extend(((0,2,3,1),(12,13,15,14)))
            for j in range(3):
                a,b=4*j,4*(j+1)
                faces.extend(((a,a+1,b+1,b),(a+2,b+2,b+3,a+3),
                              (a,b,b+2,a+2),(a+1,a+3,b+3,b+1)))
            outline=[(-1.45,-1.1),(1.65,-1.1),(1.65,1.1),(-1.45,1.1)]
        else:
            if shape == "Elbow":
                outline=[(-1,-1),(1.9,-1),(1.9,0.75),(0.75,0.75),(0.75,1.8),(-1,1.8)]
            else:
                sx,sy = {"Small":(1,1),"Broad":(1.5,1.35),"Long":(1.95,1.0)}[shape]
                outline=[(sx*math.cos(i*math.tau/8),sy*math.sin(i*math.tau/8)) for i in range(8)]
            count=len(outline)
            for height, scale in ((h-1000,0.82),(h,1)):
                vertices.extend(local(u*scale,v*scale,height) for u,v in outline)
            faces += [tuple(reversed(range(count))),tuple(range(count,2*count))]
            faces += [(i,(i+1)%count,(i+1)%count+count,i+count) for i in range(count)]
        island["outline_xy"] = [list(local(u,v,h)[:2]) for u,v in outline]
        island["cover_exclusion_radius_xy"] = r*max(math.hypot(u,v) for u,v in outline)
        island["upper_rise_cm"] = 700 if shape == "Terrace" else 0
        finish(island["id"],vertices,faces)
    for pocket in pockets:
        vertices, faces = [], []
        # 방 폭·길이는 유지하고 경사로를 늘려 36m 깊이와 32m 천고를 확보한다.
        bottom=POCKET_FLOOR-300
        sections=pocket["sections"]
        add_strip(vertices,faces,sections,((-1,1500),(1,1500)),bottom,None)
        add_strip(vertices,faces,sections,((-1,1500),(-1,1400)),bottom,GROUND)
        add_strip(vertices,faces,sections,((1,1400),(1,1500)),bottom,GROUND)
        add_strip(vertices,faces,sections[1:],((-1,1400),(1,1400)),POCKET_CEILING,GROUND)
        back=dict(sections[-1])
        heading=Vector(sections[-1]["center"])-Vector(sections[-2]["center"])
        back["center"]=list(Vector(back["center"])-heading.normalized()*100)
        add_strip(vertices,faces,[back,sections[-1]],((-1,1400),(1,1400)),bottom,GROUND)
        maximum=max(max(abs(v) for v in point(*p)) for p in vertices)
        assert maximum<32767,(pocket["id"],"Coordinate cap",maximum)
        finish(pocket["id"],vertices,faces)
        p=Vector(sections[-2]["center"]).lerp(Vector(sections[-1]["center"]),1250/2600)
        pocket["pod"] = [*p,POCKET_FLOOR]
        pocket.update(ramp_length_cm=POCKET_RAMP,end_cm=POCKET_END,floor_h=POCKET_FLOOR,ceiling_h=POCKET_CEILING)
    vertices, faces = [], []
    # 중앙의 약70m 전투장은 비우고, 언덕 바깥쪽에만 단순 기둥과 깨진 보를 둔다.
    for i in range(12):
        a = i*math.tau/12
        x,y = 4100*math.cos(a),4100*math.sin(a)
        base = ground(x,y)-150
        height = (900,1600,500,1300)[i%4]
        add_box(vertices,faces,(x-160,x+160,y-160,y+160,base,base+height))
    finish("Ruins",vertices,faces,force_shell=True)
    vertices, faces = [], []
    for h in (-1875,-1795):
        vertices.extend((3200*math.cos(i*math.tau/12),3200*math.sin(i*math.tau/12),h) for i in range(12))
    faces=[tuple(reversed(range(12))),tuple(range(12,24))]
    faces.extend((i,(i+1)%12,(i+1)%12+12,i+12) for i in range(12))
    # 지면과 5cm 단차인 유적 기단으로 중앙 보스 공간을 식별한다.
    finish("BossDais",vertices,faces)
    vertices, faces = [], []
    rocks = []
    for i in range(144):
        a = (i*137.5077+8)*math.pi/180
        radius = 6800+(i%5)*2300
        x,y = radius*math.cos(a),radius*math.sin(a)
        h = 1500+(i%7)*1450
        # Pod 머리 위와 착지면을 비운다. 암석은 가파른 팔면체로만 만든다.
        def near_outline(island):
            outline=island["outline_xy"]
            inside=False
            nearest=float("inf")
            for a,b in zip(outline,outline[1:]+outline[:1]):
                if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
                    inside=not inside
                dx,dy=b[0]-a[0],b[1]-a[1]
                t=min(1,max(0,((x-a[0])*dx+(y-a[1])*dy)/(dx*dx+dy*dy)))
                nearest=min(nearest,math.hypot(x-a[0]-t*dx,y-a[1]-t*dy))
            return inside or nearest<950
        if any(abs(h-s["h"]) < 3500+s["upper_rise_cm"] and near_outline(s) for s in islands):
            continue
        w = 350+(i%3)*130
        start = len(vertices)
        vertices.extend(((x+w,y,h),(x,y+w,h),(x-w,y,h),(x,y-w,h),
                         (x+150,y,h+1400),(x-100,y,h-1400)))
        faces.extend(tuple(start+j for j in f) for f in
            ((0,1,4),(1,2,4),(2,3,4),(3,0,4),(1,0,5),(2,1,5),(3,2,5),(0,3,5)))
        rocks.append({"x":x,"y":y,"h":h})
    finish("AirCover",vertices,faces,force_shell=True)
    manifest = {"name":"Acropolis_00", "version":VERSION, "radius_cm": R,
        "ground_h": GROUND,"boss_h": -1800,"boss_radius_cm":3300,
        "seam_margin_cm":1500,"seam_transition_cm":4000,
        "islands":islands,"pockets":pockets,"air_cover":rocks,
        "exit_routes":EXIT_ROUTES,
        "exports":EXPORTS,"sources":SOURCES,"crust":crust,
        "limits":["No engine capsule sweeps", "No bake or PIE validation",
                  "Closed pocket pieces intentionally overlap inside solid rock"]}
    (OUT / "manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    ART.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(ART / f"Acropolis_00_{VERSION}.blend"),check_existing=False)
    print("ACROPOLIS_GEOMETRY_SUCCESS",json.dumps({"meshes":len(EXPORTS),
        "islands":len(islands),"pockets":len(pockets),"air_cover":len(rocks),
        "triangles":sum(e["triangles"] for e in EXPORTS)}))


if __name__ == "__main__":
    build()
