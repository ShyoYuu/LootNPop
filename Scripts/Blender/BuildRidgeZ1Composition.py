"""Z1 비교본의 구면 지각을 별도 FBX로 굽는다."""
import json
from pathlib import Path
import sys

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
import BuildRidgeOctant as builder
import RidgeZ1CompositionSpec as spec

if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    variants = {(): (spec.VERSION, spec.ground), ("west-cover",): (spec.WEST_COVER_VERSION, spec.west_cover_ground),
                ("massif-cover",): (spec.MASSIF_COVER_VERSION, spec.massif_cover_ground)}
    assert tuple(args) in variants, args
    version, ground = variants[tuple(args)]
    out = builder.OUT.with_name(version)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    out.mkdir(parents=True, exist_ok=True)
    builder.OUT, builder.VERSION = out, version
    builder.crust(spec.base.CUTS, ground=ground, feature_points=spec.base.feature_points)
    (out / "manifest.json").write_text(json.dumps({"exports": builder.EXPORTS, "sources": builder.SOURCES}, indent=2) + "\n",
                                       encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(out / "Ridge_Z1Composition.blend"), check_existing=False)
    print("RIDGE_Z1_COMPOSITION_MESH_READY", builder.EXPORTS[0]["triangles"])
