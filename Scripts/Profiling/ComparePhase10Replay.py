"""Phase 10: `Nav.RequestCostReplay` 결과의 변형별 비교표를 만든다(단위 2부터, 단위 3·5의 비교에도 쓴다).

사용: python ComparePhase10Replay.py <재생 폴더> [케이스...]
재생 폴더에는 `<케이스>_RequestCosts.csv`(tick 비용)와 `<케이스>_RequestCosts_Requests.csv`(요청 결과)가 있어야 한다.

- burst: 묶음 크기 400 이상인 요청의 doneTick(묶음 제출 뒤 끝난 tick, 1부터) P50/P95/최대.
- 보행 길이 비율: 같은 행의 base 대비 walk(시작 위치 → 합류 waypoint부터 꺾은선 → 목표 위치). 두 변형 모두 Succeeded인 행만 센다.
"""

import csv
import math
import sys
from collections import defaultdict
from pathlib import Path

BURST_SIZE = 400
SUCCEEDED = 3


def pct(values, q):
    ordered = sorted(values)
    if not ordered:
        return float("nan")
    return ordered[max(0, math.ceil(len(ordered) * q) - 1)]


def read_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as source:
        return list(csv.DictReader(source))


def compare(replay_dir, case):
    ticks = defaultdict(list)
    for row in read_csv(replay_dir / f"{case}_RequestCosts.csv"):
        ticks[row["variant"]].append(float(row["tickUs"]))
    requests = defaultdict(dict)
    order = []
    for row in read_csv(replay_dir / f"{case}_RequestCosts_Requests.csv"):
        if row["variant"] not in requests:
            order.append(row["variant"])
        requests[row["variant"]][int(row["row"])] = row
    base = requests["base"]
    lines = [f"### {case}", "",
             "| 변형 | tick 수 | tick P50/P95/최대(us) | 확장 합 | cache hit | 앞쪽 합류 | burst 완료 tick P50/P95/최대 "
             "| 보행 길이 비율 P50/P95/최대(전체) | 비율 P95(hit) |",
             "|:---|---:|:---|---:|---:|---:|:---|:---|---:|"]
    for variant in order:
        rows = requests[variant]
        burst = [int(r["doneTick"]) for r in rows.values() if int(r["batchSize"]) >= BURST_SIZE]
        ratios, hit_ratios = [], []
        for index, r in rows.items():
            b = base.get(index)
            if b is None or int(r["status"]) != SUCCEEDED or int(b["status"]) != SUCCEEDED or float(b["walk"]) <= 0:
                continue
            ratio = float(r["walk"]) / float(b["walk"])
            ratios.append(ratio)
            if r["fromCache"] == "1":
                hit_ratios.append(ratio)
        tick = ticks[variant]
        hits = sum(r["fromCache"] == "1" for r in rows.values())
        joins = sum(r["fromCache"] == "1" and int(r["firstWaypoint"]) > 0 for r in rows.values())
        burst_text = f"{pct(burst, .5)}/{pct(burst, .95)}/{max(burst)}" if burst else "—"
        lines.append(
            f"| {variant} | {len(tick)} | {pct(tick, .5):.1f}/{pct(tick, .95):.1f}/{max(tick):.0f} "
            f"| {sum(int(r['expansions']) for r in rows.values()):,} | {hits:,} | {joins:,} | {burst_text} "
            f"| {pct(ratios, .5):.3f}/{pct(ratios, .95):.3f}/{max(ratios):.3f} "
            f"| {pct(hit_ratios, .95):.3f} |")
    return "\n".join(lines)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8")
    replay_dir = Path(sys.argv[1])
    cases = sys.argv[2:] or ["Production", "N700Chase", "N2000Chase"]
    print("\n\n".join(compare(replay_dir, case) for case in cases))
    return 0


if __name__ == "__main__":
    sys.exit(main())
