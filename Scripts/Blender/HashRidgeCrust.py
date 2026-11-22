"""Blender가 구운 지각 .blend의 SM_Octant 메시 정점(0.1mm 반올림)·면 색인 해시를 출력한다.

재현 확인: `-- massif-flat` 등으로 지각을 다시 구운 뒤
`blender --background --python Scripts/Blender/HashRidgeCrust.py -- <.blend 경로>...`의 CRUST_HASH 줄을
`Scripts/Profiling/Graphics/EDTerrainLayout_Z1.json`의 `crust_build.render`와 비교한다.
"""
import hashlib
import json
import sys

import bpy

if __name__ == "__main__":
    result = {}
    for path in sys.argv[sys.argv.index("--") + 1:]:
        bpy.ops.wm.open_mainfile(filepath=path)
        rows = {}
        for obj in bpy.data.objects:
            if obj.type != "MESH" or not obj.name.startswith("SM_Octant"):
                continue
            mesh, digest = obj.data, hashlib.sha256()
            for vertex in mesh.vertices:
                co = obj.matrix_world @ vertex.co
                digest.update(("%d,%d,%d;" % tuple(round(c * 1e4) for c in co)).encode())
            for polygon in mesh.polygons:
                digest.update((",".join(map(str, polygon.vertices)) + ";").encode())
            rows[obj.name] = {"vertices": len(mesh.vertices), "polygons": len(mesh.polygons), "sha256": digest.hexdigest()}
        result[path] = rows
    print("CRUST_HASH " + json.dumps(result))
