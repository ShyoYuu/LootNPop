"""Z1 비교본의 구면 지각을 별도 FBX로 굽는다."""
import json
from pathlib import Path
import sys

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
import BuildRidgeOctant as builder
import RidgeZ1CompositionSpec as spec

OUT = builder.OUT.with_name(spec.VERSION)


if __name__ == "__main__":
    bpy.ops.wm.read_factory_settings(use_empty=True)
    OUT.mkdir(parents=True, exist_ok=True)
    builder.OUT, builder.VERSION = OUT, spec.VERSION
    builder.crust(spec.base.CUTS, ground=spec.ground, feature_points=spec.base.feature_points)
    (OUT / "manifest.json").write_text(json.dumps({"exports": builder.EXPORTS, "sources": builder.SOURCES}, indent=2) + "\n",
                                       encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "Ridge_Z1Composition.blend"), check_existing=False)
    print("RIDGE_Z1_COMPOSITION_MESH_READY", builder.EXPORTS[0]["triangles"])
