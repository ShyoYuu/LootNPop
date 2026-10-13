"""Phase 10 단위 1: 목표 전환 큐 대기의 원인을 캡처(요청·timing·tick·사유·깨움)와 재생 요청 결과로 정량화한다.

사용: python AnalyzePhase10QueueWait.py <캡처 폴더> <재생 폴더> <출력 JSON> [케이스...]
캡처 폴더에는 RunPhase13Gate.ps1이 보존한 `<케이스>_Repeat1_Nav*.csv`가, 재생 폴더에는 같은 캡처를 재생한
`<케이스>_RequestCosts_Requests.csv`가 있어야 한다.
"""

import csv
import json
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path

BUDGET = 4000
RESOLVE = 16
CELL_CM = 200.0
WAKE_WINDOW_SECONDS = 2.0

# ELNPNavRequestReason 비트. 앞의 셋은 직전 요청이 없거나 버려진 상태라 나머지 비트가 비교 대상을 잃으므로 먼저 본다.
REASONS = [
    (1 << 0, "NoPrior"),
    (1 << 7, "AfterDirect"),
    (1 << 8, "AfterCancel"),
    (1 << 1, "TargetChanged"),
    (1 << 2, "GoalTileChanged"),
    (1 << 3, "GoalDrift"),
    (1 << 4, "PathInvalid"),
    (1 << 5, "NoPathOverlayChanged"),
    (1 << 6, "StaleResult"),
]
GOAL_MOVED = (1 << 2) | (1 << 3)


def pct(values, q):
    ordered = sorted(values)
    if not ordered:
        return float("nan")
    return ordered[max(0, math.ceil(len(ordered) * q) - 1)]


def ranks(values):
    order = sorted(range(len(values)), key=lambda i: values[i])
    result = [0.0] * len(values)
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and values[order[j + 1]] == values[order[i]]:
            j += 1
        for k in range(i, j + 1):
            result[order[k]] = (i + j) / 2.0
        i = j + 1
    return result


def spearman(a, b):
    if len(a) < 3:
        return float("nan")
    ra, rb = ranks(a), ranks(b)
    ma, mb = sum(ra) / len(ra), sum(rb) / len(rb)
    cov = sum((x - ma) * (y - mb) for x, y in zip(ra, rb))
    va = math.sqrt(sum((x - ma) ** 2 for x in ra))
    vb = math.sqrt(sum((y - mb) ** 2 for y in rb))
    return cov / (va * vb) if va > 0 and vb > 0 else float("nan")


def read_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as source:
        return list(csv.DictReader(source))


def primary_reason(mask):
    if mask is None:
        return "Untagged"
    for bit, name in REASONS:
        if mask & bit:
            return name
    return "None"


def serial_budget_completion(costs):
    """한 묶음을 단일 예산 스트림으로 순서대로 처리할 때 요청별 완료 tick(1부터)."""
    done, total = [], 0
    for cost in costs:
        total += cost
        done.append(max(1, math.ceil(total / BUDGET)))
    return done


def analyze(case, capture_dir, replay_dir):
    prefix = capture_dir / f"{case}_Repeat1_Nav"
    requests = read_csv(Path(f"{prefix}Requests.csv"))
    timings = read_csv(Path(f"{prefix}Timings.csv"))
    ticks = read_csv(Path(f"{prefix}Ticks.csv"))
    reasons = {int(r["requestSerial"]): r for r in read_csv(Path(f"{prefix}Reasons.csv"))}
    wakes = read_csv(Path(f"{prefix}Wakes.csv"))
    replay = defaultdict(dict)
    for row in read_csv(replay_dir / f"{case}_RequestCosts_Requests.csv"):
        if row["variant"] in ("base", "cache256"):
            replay[row["variant"]][int(row["row"])] = row

    # 원 제출 timing: 같은 serial의 Replan 재큐(RequeueInvalidatedPaths)는 따로 센다.
    first_timing, replans = {}, []
    for row in timings:
        serial = int(row["requestSerial"])
        if int(row["priority"]) == 1:
            replans.append(row)
        elif serial not in first_timing or float(row["submittedSeconds"]) < float(first_timing[serial]["submittedSeconds"]):
            first_timing[serial] = row
    end_seconds = max(float(t["seconds"]) for t in ticks) if ticks else 0.0

    wakes_by_owner = defaultdict(list)
    for row in wakes:
        wakes_by_owner[(row["owner"], row["ownerSerial"])].append(float(row["time"]))

    items = []
    for index, row in enumerate(requests, start=1):
        serial = int(row["requestSerial"])
        reason = reasons.get(serial)
        mask = int(reason["reasons"]) if reason else None
        timing = first_timing.get(serial)
        submitted = float(timing["submittedSeconds"]) if timing else float("nan")
        started = float(timing["startedSeconds"]) if timing else -1.0
        finished = float(timing["finishedSeconds"]) if timing else -1.0
        wait_end = started if started >= 0 else finished if finished >= 0 else end_seconds
        time = float(row["time"])
        woken = any(time - WAKE_WINDOW_SECONDS <= w <= time for w in wakes_by_owner[(row["owner"], row["ownerSerial"])])
        base, cached = replay["base"].get(index), replay["cache256"].get(index)
        start = tuple(float(row[k]) for k in ("startX", "startY", "startZ"))
        goal = tuple(float(row[k]) for k in ("goalX", "goalY", "goalZ"))
        items.append({
            "row": index, "time": time, "serial": serial, "priority": int(row["priority"]), "mask": mask,
            "cause": primary_reason(mask), "woken": woken,
            "goalMoved": mask is not None and bool(mask & GOAL_MOVED),
            "submitted": submitted, "started": started, "finished": finished,
            "status": int(timing["status"]) if timing else -1,
            "waitMs": (wait_end - submitted) * 1000.0 if timing else float("nan"),
            "doneMs": ((finished if finished >= 0 else end_seconds) - submitted) * 1000.0 if timing else float("nan"),
            "distance": math.dist(start, goal),
            "expBase": int(base["expansions"]) if base else 0, "expCache": int(cached["expansions"]) if cached else 0,
            "fromCache": cached is not None and cached["fromCache"] == "1",
            "cost": float(base["cost"]) if base else 0.0,
            "doneTickCache": int(cached["doneTick"]) if cached else 0,
            "runTicksCache": int(cached["runningTicks"]) if cached else 0,
            "statusCache": int(cached["status"]) if cached else -1,
            "startTile": int(reason["startTile"]) if reason else -1,
            "goalNode": int(reason["goalNode"]) if reason else -1,
            "goalTile": int(reason["goalTile"]) if reason else -1,
        })

    batches = defaultdict(list)
    for item in items:
        batches[item["time"]].append(item)
    burst_threshold = 5 if case == "Production" else 400

    def cause_label(item):
        if item["cause"] == "NoPrior" and item["woken"]:
            return "NoPrior(woken)"
        return item["cause"]

    # 원인별 큐 대기 기여.
    valid = [i for i in items if not math.isnan(i["waitMs"])]
    total_wait = sum(i["waitMs"] for i in valid) or 1.0
    by_cause = defaultdict(list)
    for item in valid:
        by_cause[cause_label(item)].append(item)
    cause_rows = []
    for label, group in sorted(by_cause.items(), key=lambda kv: -sum(i["waitMs"] for i in kv[1])):
        dominant = [b for b in batches.values() if Counter(cause_label(i) for i in b).most_common(1)[0][0] == label]
        cause_rows.append({
            "cause": label, "requests": len(group), "requestShare": len(group) / len(valid),
            "waitShare": sum(i["waitMs"] for i in group) / total_wait,
            "waitP50Ms": pct([i["waitMs"] for i in group], 0.5), "waitP95Ms": pct([i["waitMs"] for i in group], 0.95),
            "goalMovedShare": sum(i["goalMoved"] for i in group) / len(group),
            "dominantBatches": len(dominant), "dominantBatchesMultiple": sum(len(b) >= 2 for b in dominant),
            "maxBatch": max((len(b) for b in dominant), default=0),
            "meanBatch": sum(len(b) for b in dominant) / len(dominant) if dominant else 0.0,
            "burstBatches": sum(len(b) >= burst_threshold for b in dominant),
        })

    # tick 처리량과 정지 사유.
    tick_rows = [{k: float(v) for k, v in t.items()} for t in ticks]
    for t in tick_rows:
        t["charge"] = t["expansions"] + t["resolveCharge"]

    def tick_summary(selected):
        pending = [t for t in selected if t["queuedAfter"] > 0]
        stops = Counter(int(t["stopReason"]) for t in pending)
        scratch_budget = [t["budgetAtFirstScratchBlock"] for t in pending if int(t["stopReason"]) == 2]
        return {
            "ticks": len(selected), "pendingTicks": len(pending),
            "budgetStops": stops.get(1, 0), "scratchStops": stops.get(2, 0),
            "scratchStopShare": stops.get(2, 0) / len(pending) if pending else 0.0,
            "ticksWithScratchBlock": sum(t["scratchBlocks"] > 0 for t in pending),
            "scratchBlockBudgetP50": pct(scratch_budget, 0.5), "scratchBlockBudgetP95": pct(scratch_budget, 0.95),
            "pendingChargeP50": pct([t["charge"] for t in pending], 0.5),
            "pendingUtilization": sum(t["charge"] for t in pending) / (BUDGET * len(pending)) if pending else 0.0,
            "maxRunningP50": pct([t["maxRunning"] for t in pending], 0.5),
            "startedImmediateShare": sum(t["startedImmediate"] for t in selected) / max(1.0, sum(t["started"] for t in selected)),
            "cacheHitShare": sum(t["cacheHits"] for t in selected) / max(1.0, sum(t["started"] for t in selected)),
        }

    # 묶음별 H1·H3·H4·H5.
    bursts = []
    for time, batch in sorted(batches.items()):
        if len(batch) < burst_threshold:
            continue
        timed = [i for i in batch if not math.isnan(i["submitted"])]
        submit = min(i["submitted"] for i in timed)
        last = max((i["finished"] if i["finished"] >= 0 else end_seconds) for i in timed)
        # tick 기록 시각은 tick 끝이므로 마지막 요청을 끝낸 tick은 그 종료 시각 뒤의 첫 tick이다.
        closing = next((t["seconds"] for t in tick_rows if t["seconds"] >= last), end_seconds)
        window = [t for t in tick_rows if submit <= t["seconds"] <= closing]
        charge_cache = sum(i["expCache"] + RESOLVE for i in batch)
        charge_base = sum(i["expBase"] + RESOLVE for i in batch)
        # H3: 같은 확장 총량을 FIFO(제출 순서), 직선 거리 오름차순, 확장 오름차순(사후 최적)으로 처리할 때의 완료 tick.
        costs_fifo = [i["expCache"] + RESOLVE for i in batch]
        by_distance = [i["expCache"] + RESOLVE for i in sorted(batch, key=lambda i: i["distance"])]
        by_work = sorted(costs_fifo)
        sim = {name: serial_budget_completion(order) for name, order in
               (("fifo", costs_fifo), ("distance", by_distance), ("shortest", by_work))}
        waits = [i["waitMs"] for i in timed]
        # H4: (StartTile, GoalTile) 고유 쌍이 상한을 정한다. 묶음 안 첫 요청은 반드시 miss다.
        keys = Counter((i["startTile"], i["goalTile"]) for i in batch)
        # 같은 key가 묶음 앞에서 이미 나왔는데 miss인 요청을, 앞선 같은 key 탐색이 아직 끝나지 않은 동시 miss와
        # 끝났는데도 못 쓴 miss(직선 보행 검증 거부·접근점 경로·실패)로 나눈다. 재생 cache256의 tick 기준이다.
        first_done, concurrent_miss, late_miss, late_miss_exp = {}, 0, 0, 0
        for item in batch:
            key = (item["startTile"], item["goalTile"])
            if key in first_done and not item["fromCache"]:
                start_tick = item["doneTickCache"] - item["runTicksCache"]
                if first_done[key] > start_tick:
                    concurrent_miss += 1
                else:
                    late_miss += 1
                    late_miss_exp += item["expCache"]
            if not item["fromCache"] and item["statusCache"] == 3:
                first_done[key] = min(first_done.get(key, 1 << 30), item["doneTickCache"])
        # H5: 같은 goal node 요청들. 고유 탐색 추정은 (a) 그룹 최대 단일 확장(하한), (b) 최대 경로 cost 반경 원판 node 수.
        groups = defaultdict(list)
        for item in batch:
            groups[item["goalNode"]].append(item)
        group_max = sum(max(i["expBase"] for i in g) for g in groups.values())
        group_disk = sum(min(sum(i["expBase"] for i in g), math.pi * (max(i["cost"] for i in g) / CELL_CM) ** 2)
                         for g in groups.values())
        causes = Counter(cause_label(i) for i in batch)
        bursts.append({
            "time": time, "size": len(batch), "causes": dict(causes.most_common(4)),
            "goalMovedShare": sum(i["goalMoved"] for i in batch) / len(batch),
            "chargeBase": charge_base, "chargeCache": charge_cache,
            "predictedTicksCache": charge_cache / BUDGET, "predictedTicksBase": charge_base / BUDGET,
            "actualTicks": len(window),
            "actualMs": (last - submit) * 1000.0,
            "windowUtilization": sum(t["charge"] for t in window) / (BUDGET * len(window)) if window else 0.0,
            "windowCacheHits": sum(t["cacheHits"] for t in window),
            "windowScratchStopShare": tick_summary(window)["scratchStopShare"],
            "waitP50Ms": pct(waits, 0.5), "waitP95Ms": pct(waits, 0.95),
            "rhoWaitDistance": spearman([i["distance"] for i in timed], waits),
            "rhoWaitWork": spearman([i["expCache"] for i in timed], waits),
            "rhoWaitOrder": spearman(list(range(len(timed))), waits),
            "simDoneP50": {k: pct(v, 0.5) for k, v in sim.items()},
            "simDoneP95": {k: pct(v, 0.95) for k, v in sim.items()},
            "replayCacheHitRate": sum(i["fromCache"] for i in batch) / len(batch),
            "maxCacheHitRate": 1.0 - len(keys) / len(batch),
            "uniqueKeys": len(keys), "concurrentMiss": concurrent_miss, "lateMiss": late_miss,
            "lateMissExp": late_miss_exp,
            "hitFirstQuarter": sum(i["fromCache"] for i in batch[:len(batch) // 4]) / max(1, len(batch) // 4),
            "hitLastQuarter": sum(i["fromCache"] for i in batch[-(len(batch) // 4):]) / max(1, len(batch) // 4), "uniqueGoalTiles": len({i["goalTile"] for i in batch}),
            "goalGroups": len(groups),
            "expBase": sum(i["expBase"] for i in batch), "expCache": sum(i["expCache"] for i in batch),
            "groupMaxExp": group_max, "groupDiskExp": group_disk,
        })

    non_burst = [i for b in batches.values() if len(b) < burst_threshold for i in b]
    burst_items = [i for b in batches.values() if len(b) >= burst_threshold for i in b]
    wake_times = Counter(round(float(w["time"]), 6) for w in wakes)
    woken_batches = [b for b in batches.values() if sum(i["woken"] for i in b) * 2 >= len(b)]
    return {
        "case": case, "requests": len(items), "taggedRequests": sum(i["mask"] is not None for i in items),
        "replanRequeues": len(replans), "wakes": len(wakes), "wakeFrames": len(wake_times),
        "maxWakeFrame": max(wake_times.values(), default=0),
        "wakeBatches": len(woken_batches), "wakeBatchMax": max((len(b) for b in woken_batches), default=0),
        "batches": len(batches), "burstThreshold": burst_threshold, "bursts": bursts,
        "waitP50Ms": pct([i["waitMs"] for i in valid], 0.5), "waitP95Ms": pct([i["waitMs"] for i in valid], 0.95),
        "burstWaitShare": sum(i["waitMs"] for i in burst_items if not math.isnan(i["waitMs"])) / total_wait,
        "causes": cause_rows,
        "ticksAll": tick_summary(tick_rows),
        "replayCacheHitNonBurst": sum(i["fromCache"] for i in non_burst) / len(non_burst) if non_burst else 0.0,
        "replayCacheHitBurst": sum(i["fromCache"] for i in burst_items) / len(burst_items) if burst_items else 0.0,
    }


def main():
    capture_dir, replay_dir, output = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    cases = sys.argv[4:] or ["Production", "N700Chase", "N2000Chase"]
    results = [analyze(case, capture_dir, replay_dir) for case in cases]
    output.write_text(json.dumps(results, indent=1, ensure_ascii=False), encoding="utf-8")
    for result in results:
        print(f"== {result['case']}: requests={result['requests']} tagged={result['taggedRequests']} "
              f"wait P50/P95={result['waitP50Ms']:.1f}/{result['waitP95Ms']:.1f}ms burstWaitShare={result['burstWaitShare']:.2f} "
              f"replans={result['replanRequeues']} wakes={result['wakes']}")
        for row in result["causes"]:
            print("  cause {cause}: n={requests} req={requestShare:.2f} wait={waitShare:.2f} P95={waitP95Ms:.1f} "
                  "goalMoved={goalMovedShare:.2f} batches={dominantBatches}(multi {dominantBatchesMultiple}, burst {burstBatches}) "
                  "max={maxBatch} mean={meanBatch:.1f}".format(**row))
        print("  ticks", json.dumps(result["ticksAll"]))
        print(f"  replay cache hit nonBurst={result['replayCacheHitNonBurst']:.2f} burst={result['replayCacheHitBurst']:.2f}")
        for burst in result["bursts"][:12]:
            print("  burst t={time:.2f} n={size} pred={predictedTicksCache:.1f}/{predictedTicksBase:.1f} actual={actualTicks} "
                  "util={windowUtilization:.2f} scratchStop={windowScratchStopShare:.2f} rho(d,w,o)={rhoWaitDistance:.2f}/"
                  "{rhoWaitWork:.2f}/{rhoWaitOrder:.2f} hit={replayCacheHitRate:.2f}/{maxCacheHitRate:.2f} "
                  "miss(conc/late)={concurrentMiss}/{lateMiss} hitQ1/Q4={hitFirstQuarter:.2f}/{hitLastQuarter:.2f} "
                  "groups={goalGroups} exp={expBase}/{expCache} groupMax={groupMaxExp} disk={groupDiskExp:.0f}".format(**burst),
                  burst["causes"], burst["simDoneP50"], burst["simDoneP95"])


if __name__ == "__main__":
    main()
