"""저스트 회피 Phase 3 단위 1: `LNP.Debug.Dodge 1` 로그에서 성공률·놓침 분포·업링크 지연·연출 지연을 집계한다.

사용: python AnalyzeDodgeLog.py <로그> [--since HH:MM:SS] [--until HH:MM:SS] [--early-ms 300] [--late-ms 300] [--bin-ms 50]

단일 프로세스 PIE라 서버·게스트 줄이 한 파일에 같은 벽시계로 찍힌다. 시각은 로그 접두사(`[YYYY.MM.DD-HH.MM.SS:mmm]`)를 쓴다.
월드마다 다른 `expiry`(월드 시각)는 쓰지 않는다. 폰은 이름으로 짝짓는다(서버·게스트 월드에서 같은 이름이라는 가정 — 짝 없는 줄 수를 함께 출력한다).

시도 단위는 서버 대시 하나다(.context/JustDodge/phases/Phase03_TuningLatency.md 단위 1-3).
- success: 그 대시 뒤에 같은 폰의 `JustDodge success`가 있다
- late miss: 성공이 아니고, 대시 직전 late-ms 안에 같은 폰의 `Hit`이 있다(가장 이른 피격 기준 간격)
- early miss: 둘 다 아니고, 그 대시 뒤에 `Hit ... sinceJust<=early-ms`가 있다(첫 피격의 sinceJust).
  로그는 1초까지 숫자를 남기지만, 그보다 먼 대시는 저스트 회피 시도로 보기 어려워 여기서 자른다
- 그 밖의 대시(근처에 공격이 없음)는 분모에서 뺀다
성공률 = success / (success + early + late)
"""

import argparse
import re
import sys
from bisect import bisect_left, bisect_right
from collections import Counter, defaultdict
from datetime import datetime

LINE = re.compile(r"^\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}):(\d{3})\]\[\s*\d+\]LogLootNPop: \[Dodge\] (.*)$")
OPEN = re.compile(r"^IFrame open (\S+) role=(\S+)")
SUCCESS = re.compile(r"^JustDodge success player=(\S+) attack=(\S+)")
HIT = re.compile(r"^Hit path=(\S+) player=(\S+) sinceJust=(\S+)")
IGNORED = re.compile(r"^IFrame ignored path=(\S+) player=(\S+)")
CUE = re.compile(r"^Success cue arrived player=(\S+) local=(\d) role=(\S+)")

# 같은 대시에 붙는 사건의 최대 거리(초). 저스트 창 0.12초 + 커맨드 지연을 넉넉히 덮는다.
SUCCESS_AFTER_DASH = 0.5
UPLINK_MAX = 1.0
CUE_MAX = 2.0
PRESS_GAP = 0.3


def parse(path, since, until):
    events = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            m = LINE.match(raw.rstrip("\n"))
            if not m:
                continue
            t = datetime.strptime(m.group(1), "%Y.%m.%d-%H.%M.%S").timestamp() + int(m.group(2)) / 1000.0
            if since and datetime.fromtimestamp(t).time() < since:
                continue
            if until and datetime.fromtimestamp(t).time() >= until:
                continue
            events.append((t, m.group(3)))
    # Worker Thread 줄이 섞여 순서가 어긋날 수 있다
    events.sort(key=lambda e: e[0])
    return events


def stats(values):
    if not values:
        return "n=0"
    v = sorted(values)
    p = lambda q: v[min(len(v) - 1, int(len(v) * q))]
    return f"n={len(v)} min={v[0]:.0f} p50={p(0.5):.0f} p90={p(0.9):.0f} max={v[-1]:.0f} (ms)"


def histogram(values, bin_ms):
    if not values:
        return "    (none)"
    bins = Counter(int(v // bin_ms) for v in values)
    return "\n".join(
        f"    {b * bin_ms:>5.0f}-{(b + 1) * bin_ms:<5.0f} {'#' * n} {n}" for b, n in sorted(bins.items()))


def last_before(times, t):
    i = bisect_right(times, t) - 1
    return i if i >= 0 else None


def pair_deltas(lead, follow, max_dt):
    """lead 사건마다 max_dt 안에 뒤따르는 첫 follow 사건까지의 간격(ms)과 짝 없는 lead 수."""
    deltas, unmatched = [], 0
    for t in lead:
        i = bisect_left(follow, t)
        if i < len(follow) and follow[i] - t <= max_dt:
            deltas.append((follow[i] - t) * 1000.0)
        else:
            unmatched += 1
    return deltas, unmatched


def map_names(src, dst, max_dt, follower=False):
    """src 이름마다 시각상 가장 많이 짝지어지는 dst 이름. 기본은 src가 앞서고, follower면 src가 뒤따른다."""
    mapping = {}
    for name, times in src.items():
        best, best_n = None, 0
        for other, other_times in dst.items():
            lead, follow = (other_times, times) if follower else (times, other_times)
            n = len(pair_deltas(lead, follow, max_dt)[0])
            if n > best_n:
                best, best_n = other, n
        if best:
            mapping[name] = best
    return mapping


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--since", help="HH:MM:SS (log timestamp, usually UTC)")
    ap.add_argument("--until", help="HH:MM:SS, exclusive. One PIE session can hold several lag settings")
    ap.add_argument("--early-ms", type=float, default=300.0)
    ap.add_argument("--late-ms", type=float, default=300.0)
    ap.add_argument("--bin-ms", type=float, default=50.0)
    args = ap.parse_args()
    since = datetime.strptime(args.since, "%H:%M:%S").time() if args.since else None
    until = datetime.strptime(args.until, "%H:%M:%S").time() if args.until else None

    server_open = defaultdict(list)   # pawn -> [t]
    auto_open = defaultdict(list)
    success = defaultdict(list)       # pawn -> [(t, attack)]
    hits = defaultdict(list)          # pawn -> [(t, path, sinceJust|None)]
    ignored = Counter()
    cue_local = defaultdict(list)     # pawn -> [(t, role)]

    for t, msg in parse(args.log, since, until):
        if m := OPEN.match(msg):
            (server_open if m.group(2) == "ROLE_Authority" else auto_open)[m.group(1)].append(t)
        elif m := SUCCESS.match(msg):
            success[m.group(1)].append((t, m.group(2)))
        elif m := HIT.match(msg):
            hits[m.group(2)].append((t, m.group(1), None if m.group(3) == "-" else float(m.group(3))))
        elif m := IGNORED.match(msg):
            ignored[m.group(2)] += 1
        elif (m := CUE.match(msg)) and m.group(2) == "1":
            # 게스트 월드의 자기 폰이 서버의 호스트 폰과 같은 이름(_C_0)일 수 있다 — 역할로 월드를 가른다
            cue_local[f"{m.group(1)}@{m.group(3)}"].append((t, m.group(3)))

    pawns = sorted(set(server_open) | set(success) | set(hits))
    if not pawns:
        print("No [Dodge] lines found. Was LNP.Debug.Dodge 1 set?")
        return 1

    late_s = args.late_ms / 1000.0
    early_all, late_all = [], []
    kinds_by_pawn = {}

    print("== Attempts per pawn (server dashes) ==")
    for pawn in pawns:
        opens = server_open[pawn]
        hit_times = [h[0] for h in hits[pawn]]
        result = {}  # dash index -> ("success"|"late"|"early", value)
        used_hits = set()  # 늦은 놓침에 쓴 피격은 앞 대시의 이른 놓침으로 다시 세지 않는다

        for t, _ in success[pawn]:
            i = last_before(opens, t)
            if i is not None and t - opens[i] <= SUCCESS_AFTER_DASH:
                result.setdefault(i, ("success", None))

        for i, t in enumerate(opens):
            if i in result:
                continue
            lo = bisect_left(hit_times, t - late_s)
            hi = bisect_left(hit_times, t)
            if lo < hi:
                result[i] = ("late", (t - hit_times[lo]) * 1000.0)
                used_hits.update(range(lo, hi))

        for h, (t, _, since_just) in enumerate(hits[pawn]):
            if since_just is None or since_just > args.early_ms or h in used_hits:
                continue
            i = last_before(opens, t)
            if i is not None and i not in result:
                result[i] = ("early", since_just)

        kinds = Counter(k for k, _ in result.values())
        kinds_by_pawn[pawn] = kinds
        early =[v for k, v in result.values() if k == "early"]
        late = [v for k, v in result.values() if k == "late"]
        early_all += early
        late_all += late
        attempts = kinds["success"] + kinds["early"] + kinds["late"]
        rate = f"{100.0 * kinds['success'] / attempts:.0f}%" if attempts else "-"
        by_attack = Counter(a for _, a in success[pawn])
        by_path = Counter(p for _, p, _ in hits[pawn])
        print(f"{pawn}: dashes={len(opens)} success={kinds['success']} "
              f"(lines {dict(by_attack)}) early={kinds['early']} late={kinds['late']} rate={rate} "
              f"| hits={dict(by_path)} ignored={ignored[pawn]}")

    print(f"\n== Early miss: hit within {args.early_ms:.0f}ms after just window closed (sinceJust) ==\n  {stats(early_all)}")
    print(histogram(early_all, args.bin_ms))
    print(f"\n== Late miss: hit -> server dash within {args.late_ms:.0f}ms ==\n  {stats(late_all)}")
    print(histogram(late_all, args.bin_ms))

    # 리스폰한 폰은 서버·게스트 월드에서 이름이 달라진다(예: 서버 _C_2, 게스트 _C_10) — 이름 대신 시각으로 짝짓는다
    auto_to_server = map_names(auto_open, server_open, UPLINK_MAX)
    print("\n== Uplink: autonomous dash press -> server dash ==")
    for pawn in sorted(auto_open):
        server = auto_to_server.get(pawn)
        opens = server_open.get(server, [])
        # 롤백 뒤 같은 대시가 다시 실행되면 Autonomous 줄이 여러 번 찍힌다 — PRESS_GAP 안의 줄은 한 번 누른 것으로 묶는다
        presses = []
        for t in auto_open[pawn]:
            if presses and t - presses[-1][-1] <= PRESS_GAP:
                presses[-1].append(t)
            else:
                presses.append([t])
        firsts = [p[0] for p in presses]
        deltas, dropped = pair_deltas(firsts, opens, UPLINK_MAX)
        # 서버가 끝내 실행하지 않은 누름 — 그 직전·직후 서버 피격이 있으면 피격(넉백)이 먼저 처리돼 대시가 거부된 것으로 본다
        hit_times = [h[0] for h in hits.get(server, [])]
        near_hit = sum(1 for t in firsts
                       if not any(0 <= o - t <= UPLINK_MAX for o in opens)
                       and any(abs(h - t) <= late_s for h in hit_times))
        reexec = sum(1 for p in presses if len(p) > 1)
        print(f"  {pawn} -> {server}: presses={len(presses)} (re-executed {reexec}) {stats(deltas)}")
        print(f"    dropped by server={dropped} (with server hit within {args.late_ms:.0f}ms: {near_hit})")
        # 거부된 누름은 서버 대시가 없어 위 표의 분모에서 빠진다 — 늦은 놓침으로 더해 다시 낸 성공률
        k = kinds_by_pawn.get(server)
        if k is not None:
            attempts = k["success"] + k["early"] + k["late"] + near_hit
            rate = f"{100.0 * k['success'] / attempts:.0f}%" if attempts else "-"
            print(f"    {server} rate incl. dropped presses={rate} ({k['success']}/{attempts})")

    success_times = {p: [t for t, _ in v] for p, v in success.items()}
    cue_times = {p: [t for t, _ in v] for p, v in cue_local.items()}
    cue_to_server = map_names(cue_times, success_times, CUE_MAX, follower=True)
    print("\n== Presentation: server success -> owner cue / own dash -> owner cue ==")
    for pawn in pawns:
        if not success[pawn]:
            continue
        cues = sorted(c for name, v in cue_local.items() if cue_to_server.get(name) == pawn for c in v)
        own_auto = sorted(t for name, v in auto_open.items() if auto_to_server.get(name) == pawn for t in v)
        ct_list = [c[0] for c in cues]
        server_to_cue, dash_to_cue, unmatched = [], [], 0
        for t in success_times[pawn]:
            j = bisect_left(ct_list, t)
            if j >= len(ct_list) or ct_list[j] - t > CUE_MAX:
                unmatched += 1
                continue
            ct, role = cues[j]
            server_to_cue.append((ct - t) * 1000.0)
            own = own_auto if role == "ROLE_AutonomousProxy" else server_open[pawn]
            k = last_before(own, ct)
            if k is not None:
                dash_to_cue.append((ct - own[k]) * 1000.0)
        print(f"  {pawn}: server->cue {stats(server_to_cue)}")
        print(f"  {pawn}: dash->cue   {stats(dash_to_cue)} unmatched={unmatched}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
