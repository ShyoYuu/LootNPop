"""V012 경계 고리·기반면과 V011 보존 구조물을 Blender에서 굽는다.

실행은 Blender의 백그라운드 모드에서 --python Scripts/Blender/BuildRidgeEDBase.py를 사용한다.
형상과 FBX는 Saved/SliceOctant/Ridge_00/V012에 저장한다.
"""
import json
from pathlib import Path
import sys

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
import BuildRidgeOctant as builder
import RidgeEDBaseSpec as base

OUT = builder.OUT.with_name(base.VERSION)


def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    OUT.mkdir(parents=True, exist_ok=True)
    builder.OUT, builder.VERSION = OUT, base.VERSION

    bodies = [builder.rock_body(rock) for rock in base.ROCKS]
    bpy.ops.object.select_all(action="DESELECT")
    for body in bodies:
        body.select_set(True)
    body = bodies[0]
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    body.name = "PreservedRocksSource"
    builder.finish(body, "PreservedRocks")
    body.hide_render = True
    body.hide_set(True)

    builder.crust(base.CUTS, ground=base.base_ground, feature_points=base.feature_points)
    manifest = {"name": "Ridge_00", "version": base.VERSION, "source": base.ridge.VERSION,
                "radius_cm": base.ridge.RADIUS, "seam_margin_cm": base.ridge.SEAM_MARGIN,
                "seam_transition_cm": base.SEAM_BLEND_CM, "preserved_rocks": [r["id"] for r in base.ROCKS],
                "exports": builder.EXPORTS, "sources": builder.SOURCES}
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "Ridge_00_V012.blend"), check_existing=False)
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("RIDGE_ED_BASE_GEOMETRY_READY", json.dumps({"exports": len(builder.EXPORTS),
                                                     "triangles": sum(e["triangles"] for e in builder.EXPORTS)}))


if __name__ == "__main__":
    build()
