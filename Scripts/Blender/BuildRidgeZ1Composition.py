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
                                          spec.massif_island_v005_feature_points),
                # V005에서 고원 남북 끝 능선을 깎고, 분지 남북 테두리 사면을 조립체 앞면 뒤로 물린 지각.
                ("massif-rim",): (spec.MASSIF_RIM_VERSION, spec.massif_rim_ground, spec.massif_rim_feature_points),
                # V006에서 남 테두리를 둑 경사 여러 단으로 바꾼 지각(ED 둑 묶음 띠를 얹는 시험).
                ("massif-terrace",): (spec.MASSIF_TERRACE_VERSION, spec.massif_terrace_ground,
                                      spec.massif_terrace_feature_points),
                # V007에 북 테두리 앞(분지 쪽)으로 둑 2단을 덧댄 지각(북 테두리 B안).
                ("massif-bank",): (spec.MASSIF_BANK_VERSION, spec.massif_bank_ground, spec.massif_bank_feature_points),
                # V008에서 동굴 절개 남서 모서리 옆 V012 바위 돌출을 북쪽 단 높이로 깎은 지각.
                ("massif-spur",): (spec.MASSIF_SPUR_VERSION, spec.massif_spur_ground, spec.massif_spur_feature_points),
                # V009에서 북쪽 단·남서 모서리 평면 묶음(flat) 자리만 50cm 낮춘 렌더링 지각.
                ("massif-flat",): (spec.MASSIF_FLAT_VERSION, spec.massif_flat_ground, spec.massif_flat_feature_points)}
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
