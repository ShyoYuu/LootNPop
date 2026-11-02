"""Phase 5 §44: SampleDashMotion_5-2.py 결과에서 대시별 캡슐 이동과 표시 메시 추종을 요약한다(에디터 밖 일반 Python).

  python Scripts/AnimOverhaul/AnalyzeDashMotion_5-2.py <샘플.json> [...]
대시 구간은 활성 몽타주 이름에 Dash가 들어 있는 연속 표본이다. 기준 위치는 첫 Dash 표본 직전 표본이다.
거리는 시작 시점 캐릭터 Up에 수직인 접평면 성분만 센다(구면).
"""
import json
import math
import sys


def sub(a, b):
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def planar(v, up):
    d = dot(v, up)
    return [v[0] - up[0] * d, v[1] - up[1] * d, v[2] - up[2] * d]


def length(v):
    return math.sqrt(dot(v, v))


def signed_angle(a, b, up):
    """접평면에서 a→b 회전각(도, Up 기준 반시계 양수)."""
    return math.degrees(math.atan2(dot(cross(a, b), up), dot(a, b)))


def rnd(value):
    return round(value, 1) if value is not None else None


def dash_windows(samples):
    windows, start = [], None
    for i, s in enumerate(samples):
        is_dash = bool(s.get("montage")) and "Dash" in s["montage"]
        new_dash = is_dash and start is not None and (s["montage"] != samples[i - 1]["montage"]
                                                       or s["montagePos"] < samples[i - 1]["montagePos"])
        if start is not None and (not is_dash or new_dash):
            windows.append((start, i))
            start = None
        if is_dash and start is None:
            start = i
    if start is not None:
        windows.append((start, len(samples)))
    return windows


def follow_offsets(samples, index_range, up):
    """캡슐 중심 대비 표시 pelvis 접평면 거리, 원본 root 본의 컴포넌트 대비 거리(루트모션이 포즈에 남는지)."""
    pelvis, root = [], []
    for i in index_range:
        s = samples[i]
        if "visualPelvis" in s:
            pelvis.append(length(planar(sub(s["visualPelvis"], s["loc"]), up)))
        if "sourceRoot" in s:
            root.append(length(sub(s["sourceRoot"], s["sourceComp"])))
    return (max(pelvis) if pelvis else None), (max(root) if root else None)


def analyze(path):
    data = json.load(open(path, encoding="utf-8"))
    samples = data["samples"]
    print("== %s (%s) samples=%d class=%s" % (data["label"], data["reason"], len(samples), data["meta"].get("class")))
    idle = [i for i, s in enumerate(samples) if not s.get("montage") and length(s["vel"]) < 1.0]
    if idle:
        up = samples[idle[0]]["up"]
        print("  idle baseline: pelvisOffsetMax=%.1f rootOffsetMax=%.2f (n=%d)" % (*follow_offsets(samples, idle, up), len(idle)))
    rows = []
    for start, end in dash_windows(samples):
        base = samples[max(start - 1, 0)]
        up, fwd = base["up"], planar(base["fwd"], base["up"])
        t0 = base["t"]

        def disp_at(seconds):
            k = start
            while k + 1 < len(samples) and samples[k + 1]["t"] - t0 <= seconds:
                k += 1
            return planar(sub(samples[k]["loc"], base["loc"]), up)

        d02 = disp_at(0.2)
        last = samples[end - 1]
        dend = planar(sub(last["loc"], base["loc"]), up)
        peak = max(length(planar(samples[k]["vel"], up)) for k in range(start, end))
        # 구간 안(전신 몽타주 Release)과 종료 뒤 0.5초(일반 Offset Root 복귀)를 나눠 본다.
        pelvis_in, root_in = follow_offsets(samples, range(start, end), up)
        pelvis_after, root_after = follow_offsets(samples, range(end, min(end + 30, len(samples))), up)
        # 구면: 시작 위도(월드 Z가 극축), 구간 동안의 Up 회전량, 표시 메시가 캡슐 기준 Up 방향으로 뜨거나 가라앉은 폭.
        heights = [dot(sub(samples[k]["visualComp"], samples[k]["loc"]), samples[k]["up"])
                   for k in range(start, end) if "visualComp" in samples[k]]
        row = {"montage": samples[start]["montage"], "t": round(t0, 3), "startPos": samples[start]["montagePos"],
               "latDeg": round(math.degrees(math.asin(max(-1.0, min(1.0, up[2])))), 2),
               "upTurnDeg": round(math.degrees(math.acos(max(-1.0, min(1.0, dot(up, last["up"]))))), 2),
               "visualHeightRange": round(max(heights) - min(heights), 2) if heights else None,
               "entrySpeed": round(length(planar(base["vel"], up)), 1),
               "disp0.2": round(length(d02), 1), "dirDeg0.2": round(signed_angle(fwd, d02, up), 1),
               "dispEnd": round(length(dend), 1), "duration": round(last["t"] - t0, 3),
               "peakSpeed": round(peak, 1),
               "pelvisIn": rnd(pelvis_in), "rootIn": rnd(root_in), "pelvisAfter": rnd(pelvis_after), "rootAfter": rnd(root_after)}
        rows.append(row)
        print("  " + json.dumps(row))
    return rows


if __name__ == "__main__":
    for argument in sys.argv[1:]:
        analyze(argument)
