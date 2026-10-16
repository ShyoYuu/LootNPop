"""확정된 피라미드의 좌표·원본 검사·역할 메시 출력에 쓰는 공통 함수."""
import math
from pathlib import Path
import bpy
import bmesh
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2]
VERSION="V058"
OUT=ROOT/"Saved/SliceOctant/Pyramid_00"/VERSION
R=30000.0
EQUATOR=31500.0
APEX=18000.0
RADIAL=Vector((1,1,1)).normalized()
TANGENT=Vector((-1,1,0)).normalized()
BITANGENT=Vector((-1,-1,2)).normalized()
LEVELS=[i*1400 for i in range(-4,9)]
EXPORTS=[]
def point(x, y, h):
    return (R * RADIAL + x * BITANGENT + y * TANGENT).normalized() * (R - h)

def to_blender(p):
    return Vector((p.x / 100, -p.y / 100, p.z / 100))

def to_ue(p):
    return Vector((p.x * 100, -p.y * 100, p.z * 100))

def radius(h):
    return EQUATOR * (1 - h / APEX) if h >= 0 else EQUATOR * (1 + h / 9000)

def triangle(r):
    return [Vector((r, 0)), Vector((-r / 2, r * math.sqrt(3) / 2)),
            Vector((-r / 2, -r * math.sqrt(3) / 2))]

def mesh(name, vertices, faces, recalc=True):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    bm = bmesh.new()
    bm.from_mesh(data)
    if recalc:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(data)
    bm.free()
    return obj

def prism(name, outline, bottom, top):
    n = len(outline)
    vertices = [(*p, h) for h in (bottom, top) for p in outline]
    faces = [tuple(reversed(range(n))), tuple(range(n, 2 * n))]
    faces += [(i, (i + 1) % n, (i + 1) % n + n, i + n) for i in range(n)]
    return mesh(name, vertices, faces)

def extent(p):
    return max(-2*p[0],p[0]+math.sqrt(3)*p[1],p[0]-math.sqrt(3)*p[1])

def export(obj, role):
    bpy.ops.object.select_all(action="DESELECT")
    obj.hide_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    obj.data.calc_loop_triangles()
    coordinates = [to_ue(v.co) for v in obj.data.vertices]
    path = OUT / (obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
        apply_scale_options="FBX_SCALE_NONE", global_scale=1, axis_forward="-Z", axis_up="Y",
        mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    EXPORTS.append({"name": obj.name, "role": role, "file": str(path),
        "triangles": len(obj.data.loop_triangles),
        "bounds_cm": {side: [function(v[i] for v in coordinates) for i in range(3)]
                      for side, function in (("min", min), ("max", max))}})

def topology(bm):
    bm.verts.index_update()
    keys = [tuple(sorted(v.index for v in f.verts)) for f in bm.faces]
    return {"vertices": len(bm.verts), "triangles": len(bm.faces),
            "boundary_edges": sum(e.is_boundary for e in bm.edges),
            "non_manifold": sum(not e.is_manifold for e in bm.edges),
            "inconsistent_edges": sum(e.is_manifold and not e.is_contiguous for e in bm.edges),
            "duplicate_faces": len(keys)-len(set(keys)),
            "degenerate_faces": sum(f.calc_area() < 1e-8 for f in bm.faces)}
