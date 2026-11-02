"""Phase 5 §45: SampleNetPawns_5-2.py 결과를 사본(넷 역할)별로 요약한다.

  python Scripts/AnimOverhaul/AnalyzeNetPawns_5-2.py <샘플.json> [<샘플.json> ...]
같은 폴더에 <라벨>_Summary.txt를 쓴다. 판정은 하지 않고 비교용 수치만 낸다.
"""
import json
import math
import sys
from collections import defaultdict
from pathlib import Path


def sub(a, b):
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def length(a):
    return math.sqrt(dot(a, a))


def planar(a, up):
    d = dot(a, up)
    return [a[0] - up[0] * d, a[1] - up[1] * d, a[2] - up[2] * d]


def pct(values, p):
    if not values:
        return float("nan")
    values = sorted(values)
    return values[min(len(values) - 1, int(p * (len(values) - 1) + 0.5))]


def segments(rows):
    """연속 프레임에서 같은 몽타주가 활성인 구간 [(이름, 시작 프레임, 끝 프레임, 시작 위치)]."""
    out, current = [], None
    for frame, row in rows:
        name = row.get("montage")
        if current and (name != current[0] or frame != current[2] + 1):
            out.append(tuple(current))
            current = None
        if name and not current:
            current = [name, frame, frame, row.get("montagePos")]
        elif name:
            current[2] = frame
    if current:
        out.append(tuple(current))
    return out


def analyze(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    copies = defaultdict(list)  # (net, id, role) -> [(frame, row)]
    by_frame = defaultdict(dict)  # (id, frame) -> {net: row}
    for entry in data["frames"]:
        for row in entry["pawns"]:
            copies[(row["net"], row["id"], row["role"])].append((entry["frame"], row))
            by_frame[(row["id"], entry["frame"])][row["net"]] = row

    lines = ["label=%s frames=%d reason=%s" % (data["label"], len(data["frames"]), data["reason"]), ""]
    lines.append("== 사본별 원본 root 이탈 / 표시 pelvis 접평면 거리 / 표시 메시 Up 높이 (cm, p50/p95/max) ==")
    for key in sorted(copies):
        rows = copies[key]
        root, pelvis, height = [], [], []
        root_by_montage = defaultdict(list)
        for _, row in rows:
            up = row["up"]
            if "sourceRoot" in row:
                value = length(sub(row["sourceRoot"], row["sourceComp"]))
                root.append(value)
                root_by_montage[(row.get("montage") or "-").split("_")[2] if row.get("montage") else "-"].append(value)
            if "visualPelvis" in row:
                offset = sub(row["visualPelvis"], row["loc"])
                pelvis.append(length(planar(offset, up)))
                height.append(dot(sub(row["visualComp"], row["loc"]), up))
        lines.append("%s id=%d %s n=%d root=%.1f/%.1f/%.1f pelvis=%.1f/%.1f/%.1f meshUp=%.1f..%.1f" % (
            key[0], key[1], key[2], len(rows), pct(root, .5), pct(root, .95), max(root or [0]),
            pct(pelvis, .5), pct(pelvis, .95), max(pelvis or [0]), min(height or [0]), max(height or [0])))
        for group, values in sorted(root_by_montage.items()):
            lines.append("    root by montage group %-8s n=%5d p95=%.1f max=%.1f" % (group, len(values), pct(values, .95), max(values)))

    lines.append("")
    lines.append("== 몽타주 구간: 서버 사본 vs 클라이언트 사본 (프레임 단위 시작 시차 = 클라 - 서버) ==")
    ids = sorted({key[1] for key in copies})
    for pid in ids:
        server = next((copies[k] for k in copies if k[0] == "Server" and k[1] == pid), [])
        client_key = next((k for k in copies if k[0] == "Client" and k[1] == pid), None)
        client = copies[client_key] if client_key else []
        s_segments, c_segments = segments(server), segments(client)
        lines.append("id=%d client role=%s server segments=%d client segments=%d" % (
            pid, client_key[2] if client_key else "-", len(s_segments), len(c_segments)))
        used = set()
        for name, start, end, pos in s_segments:
            match = None
            for index, other in enumerate(c_segments):
                if index not in used and other[0] == name and abs(other[1] - start) <= 30:
                    match = index
                    break
            if match is None:
                lines.append("   %-34s server %d..%d (%d f) client MISSING" % (name, start, end, end - start + 1))
                continue
            used.add(match)
            other = c_segments[match]
            lines.append("   %-34s server %d..%d (%d f) client %+d f len %d f startPos s=%.3f c=%.3f" % (
                name, start, end, end - start + 1, other[1] - start, other[2] - other[1] + 1, pos or 0, other[3] or 0))
        for index, other in enumerate(c_segments):
            if index not in used:
                lines.append("   %-34s client-only %d..%d" % (other[0], other[1], other[2]))

    lines.append("")
    lines.append("== 같은 프레임 사본 차이: 위치 / Up 각도 / 루트 모드 불일치 ==")
    for pid in ids:
        dist, up_angle, mode_mismatch, both = [], [], defaultdict(int), 0
        for (row_id, frame), rows in by_frame.items():
            if row_id != pid or "Server" not in rows or "Client" not in rows:
                continue
            s, c = rows["Server"], rows["Client"]
            both += 1
            dist.append(length(sub(s["loc"], c["loc"])))
            up_angle.append(math.degrees(math.acos(max(-1.0, min(1.0, dot(s["up"], c["up"]))))))
            if s.get("montage") == c.get("montage") and (s.get("rootT"), s.get("rootR")) != (c.get("rootT"), c.get("rootR")):
                mode_mismatch["%s/%s vs %s/%s @%s" % (s.get("rootT"), s.get("rootR"), c.get("rootT"), c.get("rootR"),
                                                      s.get("montage") or "-")] += 1
        lines.append("id=%d paired=%d posDiff p50/p95/max=%.1f/%.1f/%.1f upDiff max=%.2fdeg" % (
            pid, both, pct(dist, .5), pct(dist, .95), max(dist or [0]), max(up_angle or [0])))
        for text, count in sorted(mode_mismatch.items(), key=lambda item: -item[1])[:8]:
            lines.append("    mode mismatch (same montage) %4d  %s" % (count, text))

    lines.append("")
    lines.append("== 사본별 몽타주 활성 중 루트 모드 분포 ==")
    for key in sorted(copies):
        counts = defaultdict(int)
        for _, row in copies[key]:
            if row.get("montage"):
                group = row["montage"].split("_")[2]
                counts["%s %s/%s" % (group, row.get("rootT"), row.get("rootR"))] += 1
        lines.append("%s id=%d %s: %s" % (key[0], key[1], key[2], ", ".join("%s=%d" % kv for kv in sorted(counts.items()))))

    output = Path(path).with_name(data["label"] + "_Summary.txt")
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))


for argument in sys.argv[1:]:
    analyze(argument)
