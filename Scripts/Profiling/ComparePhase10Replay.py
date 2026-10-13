"""Phase 10: `Nav.RequestCostReplay` 결과의 변형별 비교표를 만든다(단위 2부터, 단위 3·5의 비교에도 쓴다).

사용: python ComparePhase10Replay.py <재생 폴더> [케이스...]
재생 폴더에는 `<케이스>_RequestCosts.csv`(tick 비용)와 `<케이스>_RequestCosts_Requests.csv`(요청 결과)가 있어야 한다.

- burst: 묶음 크기 400 이상인 요청의 doneTick(묶음 제출 뒤 끝난 tick, 1부터) P50/P95/최대.
- 보행 길이 비율: 같은 행의 base 대비 walk(시작 위치 → 합류 waypoint부터 꺾은선 → 목표 위치). 두 변형 모두 Succeeded인 행만 센다.
- tick 합: 재생 tick 벽시계 시간 합(ms). 경로 CPU 총합의 근사다.
- field·폴백(단위 3): 요청 결과 `source` 열이 1(flow field 응답)·2(A* 폴백)인 수. flow field 변형의 확장 합에는 field 확정 node 수가 들어 있다.
- `<케이스>_RequestCosts_FlowFields.csv`가 있으면 flow field 변형 요약(field 수·확정 수·최대 메모리)을 따로 낸다.
- 계층형(단위 4): `source` 3은 계층형 응답(cache hit 포함), 2는 A* 폴백이다. 열 "field·폴백"은 field 응답 또는 계층형 응답/폴백 수다.
  `<케이스>_RequestCosts_Hierarchy.csv`가 있으면 계층 요약(사전 계산·메모리·link·고수준·저수준 확장·폴백)을 따로 낸다.
- overlay 토글(단위 5, 기준 5): `*Overlay` 변형은 burst마다 합성 Pod를 켜고 끈다. doneTick은 재계획까지 끝난 tick이다.
  `<케이스>_RequestCosts_Overlay.csv`가 있으면 재계산 범위(바뀐 Tile·재빌드 field·dirty Cluster·재계획)와
  같은 실행의 토글 없는 짝(이름에서 Overlay를 뺀 변형) 대비 tick 합·확장 합·burst P95 변화를 따로 낸다.
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
             "| 변형 | tick 수 | tick P50/P95/최대(us) | tick 합(ms) | 확장 합 | cache hit | 앞쪽 합류 | field·폴백 "
             "| burst 완료 tick P50/P95/최대 | 보행 길이 비율 P50/P95/최대(전체) | 비율 P95(hit) |",
             "|:---|---:|:---|---:|---:|---:|---:|:---|:---|:---|---:|"]
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
        fields = sum(r.get("source") in ("1", "3") for r in rows.values())
        fallbacks = sum(r.get("source") == "2" for r in rows.values())
        field_text = f"{fields:,}/{fallbacks:,}" if fields or fallbacks else "—"
        burst_text = f"{pct(burst, .5)}/{pct(burst, .95)}/{max(burst)}" if burst else "—"
        lines.append(
            f"| {variant} | {len(tick)} | {pct(tick, .5):.1f}/{pct(tick, .95):.1f}/{max(tick):.0f} | {sum(tick) / 1000:,.1f} "
            f"| {sum(int(r['expansions']) for r in rows.values()):,} | {hits:,} | {joins:,} | {field_text} | {burst_text} "
            f"| {pct(ratios, .5):.3f}/{pct(ratios, .95):.3f}/{max(ratios):.3f} "
            f"| {pct(hit_ratios, .95):.3f} |")
    field_path = replay_dir / f"{case}_RequestCosts_FlowFields.csv"
    if field_path.exists():
        lines += ["", "| 변형 | field 수 | 확정 합 | field당 최대 확정 | field 응답 | 폴백(반경 밖/스냅) | 스냅 | 응답 경로 node "
                  "| field 최대 메모리(MiB) | A* scratch(MiB) |",
                  "|:---|---:|---:|---:|---:|:---|---:|---:|---:|---:|"]
        for r in read_csv(field_path):
            lines.append(
                f"| {r['variant']} | {int(r['fields']):,} | {int(r['settled']):,} | {int(r['maxFieldSettled']):,} "
                f"| {int(r['fieldHits']):,} | {int(r['fallbackOutside']):,}/{int(r['fallbackEndpoint']):,} | {int(r['resolves']):,} "
                f"| {int(r['answerNodes']):,} | {int(r['peakFieldBytes']) / 1048576:.2f} | {int(r['schedulerScratchBytes']) / 1048576:.2f} |")
    hierarchy_path = replay_dir / f"{case}_RequestCosts_Hierarchy.csv"
    if hierarchy_path.exists():
        lines += ["", "| 변형 | span·고리·정제 | 사전 계산(확정 / ms) | 계층 메모리(MiB) | 고수준 scratch(MiB) | query | cache hit | 목표 link·확정 "
                  "| 고수준 확장 | 저수준 확장 | 폴백(고수준/저수준/스냅) | segment 복구 |",
                  "|:---|:---|:---|---:|---:|---:|---:|:---|---:|---:|:---|---:|"]
        for r in read_csv(hierarchy_path):
            refine = "corridor" if r["corridor"] == "1" else "segment"
            extra = (" exact" if r["exactStart"] == "1" else "") + (" keep" if r["keep"] == "1" else "")
            lines.append(
                f"| {r['variant']} | {r['span']}·{r['ring']}·{refine}{extra} | {int(r['buildSettled']):,} / {float(r['buildMs']):.0f} "
                f"| {int(r['hierarchyBytes']) / 1048576:.2f} | {int(r['highScratchBytes']) / 1048576:.2f} | {int(r['queries']):,} "
                f"| {int(r['cacheHits']):,} | {int(r['goalLinks']):,}·{int(r['linkSettled']):,} | {int(r['highLevelExpansions']):,} "
                f"| {int(r['corridorExpansions']):,} | {r['fallbackHighLevel']}/{r['fallbackCorridor']}/{r['fallbackEndpoint']} "
                f"| {int(r['refineFallbacks']):,} |")
    overlay_path = replay_dir / f"{case}_RequestCosts_Overlay.csv"
    overlay_rows = read_csv(overlay_path) if overlay_path.exists() else []
    if overlay_rows:
        lines += ["", "| 변형 | 토글 | 바뀐 Tile | 막힌 node 최대 | A* 재계획(완료/Stale) | field·계층 응답 재계획 | field 재빌드/생존(버린 확정) "
                  "| 재빌드 확정 | dirty Cluster(abstract) / 재계산 확정 | 재계산 ms 합/최대 | 목표 link 폐기 | query 재시작(버린 단위) "
                  "| tick 합 Δ | 확장 합 Δ | burst P95 짝→토글 |",
                  "|:---|---:|---:|---:|:---|---:|:---|---:|:---|:---|---:|:---|:---|:---|:---|"]
        for r in overlay_rows:
            variant = r["variant"]
            twin = variant.removesuffix("Overlay")
            summary = {}
            for name in (twin, variant):
                rows = requests.get(name, {})
                burst = [int(x["doneTick"]) for x in rows.values() if int(x["batchSize"]) >= BURST_SIZE]
                summary[name] = (sum(ticks[name]) / 1000, sum(int(x["expansions"]) for x in rows.values()), pct(burst, .95) if burst else "—")
            (twin_ms, twin_exp, twin_p95), (ms, exp, p95) = summary[twin], summary[variant]
            lines.append(
                f"| {variant} | {r['toggles']} | {r['changedTiles']} | {r['maxBlockedNodes']} | {r['requeued']}/{r['staleRestarts']} "
                f"| {r['replans']} | {r['fieldsRebuilt']}/{r['fieldsLive']}({int(r['fieldSettledDiscarded']):,}) "
                f"| {int(r['fieldRebuildSettled']):,} | {r['dirtyClusters']}({r['dirtyAbstract']}) / {int(r['updateSettled']):,} "
                f"| {float(r['updateMs']):.2f}/{float(r['maxUpdateMs']):.2f} | {r['goalLinksDropped']} "
                f"| {r['queriesRestarted']}({int(r['restartUnits']):,}) "
                f"| {ms - twin_ms:+.1f}ms({(ms / twin_ms - 1) * 100 if twin_ms else 0:+.1f}%) "
                f"| {exp - twin_exp:+,}({(exp / twin_exp - 1) * 100 if twin_exp else 0:+.1f}%) | {twin_p95}→{p95} |")
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
