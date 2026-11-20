"""ED CloseRange 섬의 지지면 높이장(CR 원점 기준 격자)을 만들어 CloseRangeSupport_<버전>.json에 쓴다.

일반 Python으로 `python BuildCloseRangeSupport.py <버전>`을 실행한다. 입력은 로컬 조사 자료(Saved, Git 제외)다.
- CloseRangeHeights.json: LootNPop 에디터에서 덤프한 CR 그룹별 인스턴스 transform·메시 상자.
- CloseRangeSupportTrace.json: ED 에디터에서 산괴를 빼고 위에서 쏜 1m 격자 트레이스(충돌 있는 메시만 맞음).

ED CloseRange는 떠 있는 섬이고 테두리의 ForestGround 타일(원점 +3m)에는 충돌이 없다. 그래서 납작한 지면 타일은
인스턴스 상자 윗면을 직접 래스터화하고, 둑·하상 암석은 트레이스 높이를 쓴다(식생 제외). 지지면이 있는 칸은
'지지면 - 여유'로 지각을 그 아래에 두고, 지지면이 없는 칸은 둘레 지지면에서 45° 경사로 내려와 고원에 닿게 한다.
"""
import json
import math
from pathlib import Path
import sys

SAVED = Path(__file__).resolve().parents[3] / "Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119"
FLAT_RATIO = 0.35
# 트레이스에서 지지면으로 치지 않는 식생·쓰러진 나무.
VEGETATION = ("Elderberry", "Hornbeam", "MoneyPlant", "Fern", "CastorOil", "EnglishIvy", "DeadLeaves", "Cordyline",
              "FallenTree", "Palm", "Amaryllis", "Arrowhead", "Ginger", "Dracaena")
# step: 격자(cm). margin: 지지면 칸의 지각을 보이는 지지면보다 낮추는 값(하상 골 V003과 같은 50cm).
# slope·reach: 지지면 없는 칸으로 내려오는 경사(cm/cm, 1 = 45°)와 최대 거리(cm).
# inset: 타일 발자국을 상자 중심 기준으로 줄이는 비율. lip·ring: 섬 바깥(테두리 밖) 빈 칸의 지각을 지지면보다
# lip만큼 높게 ring 폭으로 넓힌 뒤 경사로 내린다. lip이 None이면 바깥·안쪽 빈 칸을 구분하지 않는다.
VERSIONS = {
    "V001": {"step": 100, "margin": 50, "slope": 1.0, "reach": 600, "inset": 1.0, "lip": None, "ring": 0},
    # V001 결과: 바깥 테두리 둥근 타일 가장자리가 바로 밖에서 내려가는 지각 위로 처마처럼 튀어나와 버섯 갓 그림자가 졌다.
    # V002는 50cm 격자로 타일 발자국을 가장자리 20%만큼 줄이고, 섬 바깥 빈 칸을 지지면 +10cm로 1m 넓혀 갓을 묻는다.
    "V002": {"step": 50, "margin": 50, "slope": 1.0, "reach": 800, "inset": 0.8, "lip": 10, "ring": 100},
}


def axes(pitch, yaw, roll):
    # UE FRotationMatrix의 X·Y·Z 축.
    sp, cp = math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    sy, cy = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    sr, cr = math.sin(math.radians(roll)), math.cos(math.radians(roll))
    return ((cp * cy, cp * sy, sp), (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp),
            (-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp))


def exterior(support):
    # 격자 테두리에서 빈 칸끼리 4방향으로 이어진 칸(섬 바깥)을 찾는다.
    ny, nx = len(support), len(support[0])
    outside = [[False] * nx for _ in range(ny)]
    stack = [(j, i) for j in range(ny) for i in (0, nx - 1)] + [(j, i) for i in range(nx) for j in (0, ny - 1)]
    while stack:
        j, i = stack.pop()
        if outside[j][i] or support[j][i] is not None:
            continue
        outside[j][i] = True
        for dj, di in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            if 0 <= j + dj < ny and 0 <= i + di < nx and not outside[j + dj][i + di]:
                stack.append((j + dj, i + di))
    return outside


def main(version):
    p = VERSIONS[version]
    step = p["step"]
    trace = json.loads((SAVED / "CloseRangeSupportTrace.json").read_text(encoding="utf-8"))
    heights = json.loads((SAVED / "CloseRangeHeights.json").read_text(encoding="utf-8"))
    x0, y0, tstep = trace["x0"], trace["y0"], trace["step"]
    tny, tnx = len(trace["rows"]), len(trace["rows"][0])
    ny, nx = (tny - 1) * tstep // step + 1, (tnx - 1) * tstep // step + 1
    support = [[None] * nx for _ in range(ny)]
    traced = 0
    for j in range(ny):
        for i in range(nx):
            # 트레이스 격자보다 촘촘하면 가장 가까운 트레이스 칸을 쓴다.
            tj, ti = min(tny - 1, round(j * step / tstep)), min(tnx - 1, round(i * step / tstep))
            cell = trace["rows"][tj][ti]
            if cell and not any(v in trace["meshes"][cell[1]] for v in VEGETATION):
                support[j][i] = cell[0]
                traced += 1

    lo_t, hi_t = (1 - p["inset"]) / 2, (1 + p["inset"]) / 2
    tiles = 0
    for group in ("Ditch", "Ground"):
        for mesh, data in heights[group].items():
            lo, hi = data["box_min"], data["box_max"]
            for x, y, z, pitch, yaw, roll, sx, sy, sz in data["i"]:
                ex, ey, ez = (hi[0] - lo[0]) * abs(sx), (hi[1] - lo[1]) * abs(sy), (hi[2] - lo[2]) * abs(sz)
                if ez >= FLAT_RATIO * max(ex, ey) or z < -2000:
                    continue
                ax = axes(pitch, yaw, roll)
                corners = []
                for bx in (lo[0], hi[0]):
                    for by in (lo[1], hi[1]):
                        local = (bx * sx, by * sy, hi[2] * sz)
                        corners.append([(x, y, z)[c] + sum(ax[k][c] * local[k] for k in range(3)) for c in range(3)])
                top = max(c[2] for c in corners)
                # 사각형 발자국(inset 적용) 안의 격자 칸(중심 기준)에 윗면 높이를 쓴다.
                (ax0, ay0), (ux, uy), (vx, vy) = corners[0][:2], \
                    (corners[2][0] - corners[0][0], corners[2][1] - corners[0][1]), \
                    (corners[1][0] - corners[0][0], corners[1][1] - corners[0][1])
                uu, vv = ux * ux + uy * uy, vx * vx + vy * vy
                if not uu or not vv:
                    continue
                xs, ys = [c[0] for c in corners], [c[1] for c in corners]
                for j in range(max(0, math.ceil((min(ys) - y0) / step)), min(ny - 1, math.floor((max(ys) - y0) / step)) + 1):
                    for i in range(max(0, math.ceil((min(xs) - x0) / step)), min(nx - 1, math.floor((max(xs) - x0) / step)) + 1):
                        px, py = x0 + i * step - ax0, y0 + j * step - ay0
                        a, b = (px * ux + py * uy) / uu, (px * vx + py * vy) / vv
                        if lo_t <= a <= hi_t and lo_t <= b <= hi_t and (support[j][i] is None or top > support[j][i]):
                            support[j][i] = round(top)
                tiles += 1

    # 지지면 칸은 지지면 - 여유. 빈 칸은 둘레 지지면에서 경사로 내려온 높이(0 아래로는 내리지 않음).
    # lip이 있으면 섬 바깥 빈 칸은 지지면 + lip을 ring 폭만큼 유지한 뒤 내려온다.
    outside = exterior(support) if p["lip"] is not None else None
    reach = int(p["reach"] // step)
    offset = [[0.0] * nx for _ in range(ny)]
    for j in range(ny):
        for i in range(nx):
            s = support[j][i]
            if s is None:
                continue
            offset[j][i] = s - p["margin"]
            for dj in range(-reach, reach + 1):
                for di in range(-reach, reach + 1):
                    qj, qi = j + dj, i + di
                    if not (0 <= qj < ny and 0 <= qi < nx) or support[qj][qi] is not None:
                        continue
                    d = step * math.hypot(di, dj)
                    if outside and outside[qj][qi]:
                        v = s + p["lip"] - p["slope"] * max(0.0, d - p["ring"])
                    else:
                        v = s - p["margin"] - p["slope"] * d
                    if v > offset[qj][qi]:
                        offset[qj][qi] = v
    rows = [[round(v) for v in r] for r in offset]
    filled = sum(1 for r in support for v in r if v is not None)
    out = Path(__file__).resolve().with_name("CloseRangeSupport_%s.json" % version)
    out.write_text(json.dumps({"source": "ED ElectricDreams_PCGCloseRange, origin (90250, -4360, 1660)",
                               "x0": x0, "y0": y0, "step": step, "params": p, "rows": rows},
                              separators=(",", ":")) + "\n", encoding="utf-8")
    print("CR_SUPPORT_READY %s cells=%d supported=%d traced=%d tiles=%d" % (version, nx * ny, filled, traced, tiles))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "V001")
