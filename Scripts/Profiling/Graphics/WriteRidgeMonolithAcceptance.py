"""Phase 1-BR IsleSkin V007(모노리스 A안 시각 외피) 사용자 채택 기록을 쓴다(GX-040).

실행(프로젝트 루트): python Scripts/Profiling/Graphics/WriteRidgeMonolithAcceptance.py
결과: Scripts/Profiling/Graphics/IsleSkin_V001/Acceptance_V007.json(맵·배치·형상·Verify·도구 해시와 결정 내용).
다음 세션의 보존 확인은 V008 721항목 + V006 Acceptance + 이 파일의 files를 다시 해시한다.
"""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SAVED = "Saved/Profiling/Graphics/Phase01/"
BODIES = "Content/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_V001/Bodies_V007/"
FILES = [
    "Content/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_V007/LVI_Ridge_EDTerrain_IsleSkin_V007.umap",
    BODIES + "SM_Ridge_MonolithShroud_V002.uasset", BODIES + "SM_Ridge_MonolithCap_V002.uasset",
    BODIES + "SM_Ridge_MonolithSlabs_V002.uasset", BODIES + "SM_Ridge_MonolithChips_V002.uasset",
    "Scripts/Profiling/Graphics/IsleSkin_V001/IsleSkin_V007.json",
    SAVED + "IsleSkin_V001/Monolith_V002/Build.json",
    SAVED + "Monolith_V001/Dump_M1_V002.json",
    SAVED + "IsleSkin_V001/Verify_V007_20261127_131612.json",
    "Scripts/Blender/BuildRidgeMonolithSkin.py",
    "Scripts/Profiling/Graphics/GenerateRidgeMonolithSkin.py",
    "Scripts/Profiling/Graphics/InspectRidgeMonolithTargets.py",
    "Scripts/Profiling/Graphics/CheckRidgePreservation.py",
    "Scripts/Profiling/Graphics/BuildRidgeIsleSkin.py",
]


def main():
    build = json.loads((ROOT / SAVED / "IsleSkin_V001/Monolith_V002/Build.json").read_text(encoding="utf-8"))
    record = {
        "unit": "IsleSkin", "version": "V007", "date": "2026-11-27", "status": "current_baseline",
        "user_decision": "Option A (visual skin only, no crust/collision cut) for M1, the east tower above the "
                         "MassifCavern landing: split spire (two shards) + floating angular slabs + face slabs + chips "
                         "kept as is (GX-040). M2 (MassifCavern block west face) left untouched.",
        "scope": "visual only (NoCollision); collision derivation, Lit/Lumen review, PIE and performance not done",
        "level": "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_V007/LVI_Ridge_EDTerrain_IsleSkin_V007",
        "source_level": "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_V006/LVI_Ridge_EDTerrain_IsleSkin_V006",
        "monolith_version": "V002", "monolith_rejected": ["V001 (weak change, rounded cap read as an egg)"],
        "protected": {"spaces": build["protected_spaces"], "samples": build["samples"], "new_hits": build["new_hits"],
                      "inside_existing": build["inside_existing"],
                      "keepout": build["rules"]["keepout_spaces"], "keepout_margin_cm": build["rules"]["keepout_margin_cm"]},
        "landmark_sight_min_cm": min(v["min_distance_cm"] for r in build["landmark_sight"].values() for v in r.values()),
        "max_top_h_cm": build["shroud"]["max_top_h_cm"],
        "files": [{"path": p, "sha256": hashlib.sha256((ROOT / p).read_bytes()).hexdigest()} for p in FILES],
        "previous_trials_kept": ["IsleSkin_V001", "IsleSkin_V002", "IsleSkin_V003", "IsleSkin_V004", "IsleSkin_V005",
                                 "IsleSkin_V006 (baseline before V007, Acceptance_V006 unchanged)"],
        "unused_assets_kept": ["Bodies_V007/SM_Ridge_Monolith*_V001 (V001 trial import)"],
    }
    out = ROOT / "Scripts/Profiling/Graphics/IsleSkin_V001/Acceptance_V007.json"
    out.write_text(json.dumps(record, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(json.dumps({"files": len(record["files"]), "new_hits": record["protected"]["new_hits"],
                      "landmark_min_cm": record["landmark_sight_min_cm"]}))


if __name__ == "__main__":
    main()
