"""Phase 1-BR 모노리스 재해석(A안 시각 외피) 배치 JSON을 만든다(GX-038).

실행(프로젝트 루트): python Scripts/Profiling/Graphics/GenerateRidgeMonolithSkin.py <IsleSkin 판> <모노리스 판>
예: python Scripts/Profiling/Graphics/GenerateRidgeMonolithSkin.py V007 V001
- 기준 IsleSkin_V006.json(석교 B안, GX-039)을 그대로 이어받고 Blender BuildRidgeMonolithSkin.py 몸체 3개를 더한다.
- 원본 맵은 IsleSkin_V006 맵이다. BuildRidgeIsleSkin.py `build <판>`이 V006 맵을 새 맵으로 복제한 뒤 같은 배치를 다시 놓는다.
- 모든 몸체 에셋은 새 판 폴더(Bodies_<판>)로 반입한다. V006 몸체 에셋·맵·생성 스크립트는 바꾸지 않는다
  (GenerateRidgeIsleSkin.py는 V006 Acceptance의 보존 대상이라 고치지 않고 이 스크립트를 따로 둔다).
- Blender 보호 검사에서 새 점유가 있거나 위상 결함이 있으면 만들지 않는다.
"""
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
AUTHOR = Path(__file__).resolve().parent / "IsleSkin_V001"
BASE = AUTHOR / "IsleSkin_V006.json"
MAP = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_{0}/LVI_Ridge_EDTerrain_IsleSkin_{0}"
BODIES = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_V001/Bodies_"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main(version, monolith):
    base = json.loads(BASE.read_text(encoding="utf-8"))
    build_rel = "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Monolith_%s/Build.json" % monolith
    build = json.loads((ROOT / build_rel).read_text(encoding="utf-8"))
    assert build["topology_pass"], "monolith topology failed"
    assert build["new_hits"] == 0, "monolith protected-space hits: %d" % build["new_hits"]
    layout = json.loads(json.dumps(base))
    layout["version"] = version
    layout["source_level"] = MAP.format("V006")
    layout["work_level"] = MAP.format(version)
    layout["rules"] = dict(base["rules"], monolith=build["rules"], monolith_version=monolith,
                           base_layout="IsleSkin_V006")
    layout["inputs"] = dict(base["inputs"], **{
        BASE.relative_to(ROOT).as_posix(): sha(BASE), build_rel: sha(ROOT / build_rel),
        Path(__file__).resolve().relative_to(ROOT).as_posix(): sha(__file__)})
    if layout.get("review_hits"):
        layout["review_hits"]["note"] = "SkyBridgeDeck side band occupied by the bridge's own skin (accepted, GX-039)"
    folder = BODIES + version
    for row in layout["bodies"]:
        row["asset"] = folder + "/" + row["asset"].rsplit("/", 1)[1]
    material = base["bodies"][0]["material"]
    layout["bodies"] += [{"label": o["name"].replace("SM_Ridge_", "BODY_"), "group": "Monolith/Body", "file": o["file"],
                          "fbx_sha256": o["sha256"], "asset": folder + "/" + o["name"], "triangles": o["triangles"],
                          "bounds_cm": o["bounds_cm"], "role": o["role"], "material": material}
                         for o in build["outputs"]]
    out = AUTHOR / ("IsleSkin_%s.json" % version)
    out.write_text(json.dumps(layout, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(json.dumps({"out": out.relative_to(ROOT).as_posix(), "placements": len(layout["placements"]),
                      "bodies": [b["label"] for b in layout["bodies"]], "monolith_new_hits": build["new_hits"]}))


if __name__ == "__main__":
    main(*sys.argv[1:3])
