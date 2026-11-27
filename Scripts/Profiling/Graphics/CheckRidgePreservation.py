"""Phase 1-BR 보존 해시 확인(읽기 전용).

실행(프로젝트 루트): python Scripts/Profiling/Graphics/CheckRidgePreservation.py <라벨>
- V008: ClosedBodyTrial_V001/FrozenInputs_V008.json 696파일 + U5a_7_V001/AcceptedHandoffHashes_V008.json 25파일을
  경로/해시로 중복 제거한 목록(721)을 다시 해시한다.
- V006: IsleSkin_V001/Acceptance_V006.json의 파일을 다시 해시한다.
- V007: IsleSkin_V001/Acceptance_V007.json이 있으면 그 파일도 다시 해시한다(모노리스 A안, GX-040).
결과: Saved/Profiling/Graphics/Phase01/Monolith_V001/Preservation_<라벨>.json
"""
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def rows(path, key=None):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    data = data[key] if key else data
    return [(r["path"], r["sha256"]) for r in data]


def check(pairs):
    bad = []
    for path, expected in pairs:
        p = ROOT / path
        got = hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None
        if got != expected:
            bad.append({"path": path, "expected": expected, "got": got})
    return bad


def main(label):
    v008 = sorted(set(rows(HERE / "ClosedBodyTrial_V001/FrozenInputs_V008.json", "files")
                      + rows(ROOT / "Saved/Profiling/Graphics/Phase01/U5a_7_V001/AcceptedHandoffHashes_V008.json")))
    paths = {p for p, _ in v008}
    assert len(paths) == len(v008), "same path with different hashes in V008 lists"
    v006 = rows(HERE / "IsleSkin_V001/Acceptance_V006.json", "files")
    report = {"label": label, "v008_files": len(v008), "v008_mismatch": check(v008),
              "v006_files": len(v006), "v006_mismatch": check(v006)}
    v007 = HERE / "IsleSkin_V001/Acceptance_V007.json"
    if v007.exists():
        pairs = rows(v007, "files")
        report.update({"v007_files": len(pairs), "v007_mismatch": check(pairs)})
    report["pass"] = not report["v008_mismatch"] and not report["v006_mismatch"] and not report.get("v007_mismatch")
    out = ROOT / ("Saved/Profiling/Graphics/Phase01/Monolith_V001/Preservation_%s.json" % label)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    print(json.dumps({k: (len(v) if isinstance(v, list) else v) for k, v in report.items()}))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "check")
