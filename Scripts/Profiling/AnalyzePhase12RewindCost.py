"""수동 사격 trace의 서버 투사체 실행을 같은 길이 구간에서 비교한다."""

import bisect
import csv
import hashlib
import json
import math
import re
import sys
from collections import defaultdict
from pathlib import Path


def stats(values):
    ordered = sorted(values)
    if not ordered:
        raise ValueError("Empty timing population")
    return {
        "Count": len(ordered),
        "MeanMs": sum(ordered) / len(ordered),
        "P50Ms": ordered[math.ceil(len(ordered) * 0.5) - 1],
        "P95Ms": ordered[math.ceil(len(ordered) * 0.95) - 1],
    }


def read_run(directory, mode):
    frames = []
    parents = []
    current = {}
    unmatched = 0
    with (directory / f"{mode}_Events.csv").open(encoding="utf-8-sig", newline="") as source:
        for row in csv.DictReader(source):
            name = row["TimerName"]
            start, end, duration = (float(row[key]) for key in ("StartTime", "EndTime", "Duration"))
            if row["ThreadName"] == "Game Frames":
                frames.append((start, end, duration))
                continue
            thread = row["ThreadId"]
            if re.fullmatch(r"LNPProjectileHitDetectionProcessor(?:_\d+)?", name):
                parent = {"Start": start, "End": end, "Ms": duration * 1000, "Server": False, "Children": defaultdict(float)}
                parents.append(parent)
                current[thread] = parent
            elif name.startswith("LNPProjectile_Server"):
                parent = current.get(thread)
                if parent is None or start < parent["Start"] - 2e-6 or end > parent["End"] + 2e-6:
                    unmatched += 1
                    continue
                parent["Server"] = True
                parent["Children"][name] += duration * 1000
    if unmatched:
        raise ValueError(f"{mode}: {unmatched} server events lack enclosing processor")
    servers = sorted((parent for parent in parents if parent["Server"]), key=lambda parent: parent["Start"])
    if not servers or not frames:
        raise ValueError(f"{mode}: missing server or game frame events")
    # 분석 전에 고정한 양끝 제외 폭이다. 모드 전환과 PIE 종료를 비교에 섞지 않는다.
    start, end = servers[0]["Start"] + 10, servers[-1]["End"] - 5
    return {"Mode": mode, "Servers": servers, "Frames": frames, "Start": start, "End": end}


def summarize(run, seconds):
    start, end = run["Start"], run["Start"] + seconds
    frames = [(a, b, d) for a, b, d in run["Frames"] if a >= start and b <= end]
    starts = [frame[0] for frame in frames]
    parents = [parent for parent in run["Servers"] if parent["Start"] >= start and parent["End"] <= end]
    per_frame = [0.0] * len(frames)
    child_sums = defaultdict(float)
    outside = 0
    for parent in parents:
        index = bisect.bisect_right(starts, parent["Start"]) - 1
        if index >= 0 and parent["Start"] <= frames[index][1] + 2e-6:
            per_frame[index] += parent["Ms"]
        else:
            outside += 1
        for name, milliseconds in parent["Children"].items():
            child_sums[name] += milliseconds
    if outside > 2:
        raise ValueError(f"{run['Mode']}: {outside} processor invocations outside selected frames")
    blocks = []
    for offset in range(0, int(seconds), 30):
        block = [parent["Ms"] for parent in parents if start + offset <= parent["Start"] < start + offset + 30]
        blocks.append({"OffsetSeconds": offset, **stats(block)})
    return {
        "Mode": run["Mode"], "IntervalStart": start, "IntervalEnd": end,
        "Seconds": seconds, "ServerInvocations": stats([parent["Ms"] for parent in parents]),
        "ServerInclusiveMsPerGameFrame": stats(per_frame),
        "EditorGameFrameMs": stats([duration * 1000 for _, _, duration in frames]),
        "BoundaryInvocationsOutsideFrames": outside,
        "ChildrenRecordedMs": dict(child_sums), "ThirtySecondBlocks": blocks,
        "ActiveServerIntervalStart": run["Servers"][0]["Start"],
        "ActiveServerIntervalEnd": run["Servers"][-1]["End"],
    }


directory = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("Saved/Profiling/Phase12/Rewind/Cost")
runs = [read_run(directory, mode) for mode in ("Off1", "On1")]
# 수치를 보기 전에 정한 상한 180초와 30초 블록으로 서로 다른 기록 길이를 맞춘다.
seconds = min(180, math.floor(min(run["End"] - run["Start"] for run in runs) / 30) * 30)
if seconds < 60:
    raise ValueError("Less than 60 seconds of matched active capture")
summary = {
    "Runs": [summarize(run, seconds) for run in runs],
    "Sources": {mode: hashlib.file_digest((directory / f"{mode}_Events.csv").open("rb"), "sha256").hexdigest() for mode in ("Off1", "On1")},
    "ManualWorkloadsAreNotIdentical": True,
    "ShotCountsAndRemoteOnlyCpuNotMeasured": True,
    "EditorFramesAreNotPackagedFrameGate": True,
    "NestedChildTimesMustNotBeSummedWithParents": True,
}
(directory / "Summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
print(json.dumps(summary, indent=2))
