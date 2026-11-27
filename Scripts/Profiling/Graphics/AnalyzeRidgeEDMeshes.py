"""읽기 전용: ED 후보 메시의 LOD0(폴백) 정점으로 닫힘(경계 변 수)·면적 가중 평균 법선·크기를 측정한다.

UE Cmd: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/AnalyzeRidgeEDMeshes.py"
결과는 IsleSkin_V001/MeshAnalysis.json이다. area_normal은 UE 삼각형 순서로 계산한 값이라 실제 바깥 법선 합의 반대 부호다.
즉 절벽 껍질의 보이는 앞면은 -area_normal 쪽, 열린 뒤·바닥은 +area_normal 쪽이다.
"""
import json
import math
import unreal

OUT = r"D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/IsleSkin_V001/MeshAnalysis.json"
CATALOG = r"D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/EDTerrainCatalog_V001.json"
cat = json.load(open(CATALOG, encoding="utf-8"))
paths = [c["mesh"] for c in cat["candidates"]]
rows = []
for path in paths:
    mesh = unreal.load_asset(path)
    if not mesh:
        rows.append({"mesh": path, "missing": True})
        continue
    verts, tris, normals_all = [], [], []
    count = mesh.get_num_sections(0)
    for s in range(count):
        v, t, n, uv, tan = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, s)
        base = len(verts)
        verts += [(p.x, p.y, p.z) for p in v]
        tris += [base + i for i in t]
    # 위치 용접(0.1cm) 뒤 경계 변 계산
    key = {}
    weld = []
    for p in verts:
        k = (round(p[0] * 10), round(p[1] * 10), round(p[2] * 10))
        weld.append(key.setdefault(k, len(key)))
    edges = {}
    area_n = [0.0, 0.0, 0.0]
    area = 0.0
    up_area = down_area = 0.0
    for i in range(0, len(tris), 3):
        a, b, c = verts[tris[i]], verts[tris[i + 1]], verts[tris[i + 2]]
        u = [b[j] - a[j] for j in range(3)]
        w = [c[j] - a[j] for j in range(3)]
        n = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]]
        ar = 0.5 * math.sqrt(sum(x * x for x in n))
        area += ar
        for j in range(3):
            area_n[j] += 0.5 * n[j]
        ia, ib, ic = weld[tris[i]], weld[tris[i + 1]], weld[tris[i + 2]]
        for e in ((ia, ib), (ib, ic), (ic, ia)):
            k = (min(e), max(e))
            edges[k] = edges.get(k, 0) + 1
    boundary = sum(1 for v in edges.values() if v == 1)
    b = mesh.get_bounding_box()
    rows.append({"mesh": path, "vertices": len(key), "triangles": len(tris) // 3, "sections": count,
                 "boundary_edges": boundary, "edges": len(edges), "area_cm2": area,
                 "area_normal": area_n, "openness": math.sqrt(sum(x * x for x in area_n)) / max(area, 1e-6),
                 "bounds_min": [b.min.x, b.min.y, b.min.z], "bounds_max": [b.max.x, b.max.y, b.max.z],
                 "nanite": bool(mesh.get_editor_property("nanite_settings").enabled)})
json.dump({"rows": rows}, open(OUT, "w", encoding="utf-8"), indent=1)
unreal.log("ANALYZE_MESHES_READY %d" % len(rows))
