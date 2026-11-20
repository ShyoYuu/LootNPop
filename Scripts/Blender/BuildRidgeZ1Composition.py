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
                ("massif-cover",): (spec.MASSIF_COVER_VERSION, spec.massif_cover_ground),
                # 고원에 CloseRange 하상 띠를 얕게 판 렌더링용 지각. 좁은 골을 살리려고 띠 둘레 특징점을 더한다.
                ("massif-channel",): (spec.MASSIF_CHANNEL_VERSION, spec.massif_channel_ground,
                                      spec.massif_channel_feature_points),
                # ED 섬 지지면(둑 고리 +3m 단)을 따라 올린 렌더링용 지각. 하상 띠는 계속 판다.
                ("massif-island",): (spec.MASSIF_ISLAND_VERSION, spec.massif_island_ground,
                                     spec.massif_island_feature_points),
                # 테두리 밖을 지지면 +10cm로 넓혀 둥근 타일 가장자리(버섯 갓)를 묻은 지각.
                ("massif-island-v005",): (spec.MASSIF_ISLAND_V005_VERSION, spec.massif_island_v005_ground,
                                          spec.massif_island_v005_feature_points)}
    assert tuple(args) in variants, args
    version, ground, *features = variants[tuple(args)]
    feature_points = features[0] if features else spec.base.feature_points
    out = builder.OUT.with_name(version)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    out.mkdir(parents=True, exist_ok=True)
    builder.OUT, builder.VERSION = out, version
    builder.crust(spec.base.CUTS, ground=ground, feature_points=feature_points)
    (out / "manifest.json").write_text(json.dumps({"exports": builder.EXPORTS, "sources": builder.SOURCES}, indent=2) + "\n",
                                       encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(out / "Ridge_Z1Composition.blend"), check_existing=False)
    print("RIDGE_Z1_COMPOSITION_MESH_READY", builder.EXPORTS[0]["triangles"])
