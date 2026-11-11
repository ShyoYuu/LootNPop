"""Ridge_00 대능선 산괴 옥탄트 spec. 단위 cm, 옥탄트 로컬 X/Y/H이며 H는 구 중심 쪽(현지 Up)으로 증가한다.

V004부터 고저차 약 116m의 산괴다. 가장 낮은 플레이 공간(대분지·협곡 출구 -40m)에서 가장 높은 곳(왕관 정상 +76m)까지 걷는다.
지각 높이장 하나가 능선·대지·계단 단구·협곡·분지·경사로를 모두 소유한다. 암반은 지각을 절단하는 소수의 닫힌 몸체다.
높이장 합성 순서: 기본 굴곡 → 능선(좌우 비대칭 사면) → 대지(다각형) → 계단 단구(영역 한정 양자화)
→ 분지·평탄부 → 경사로·잔도·협곡(선형 경로) → 이음매 감쇠.
"""
import math

VERSION = "V011"
STAGE = "Full"
RADIUS = 30000.0
SEAM_MARGIN = 1500.0
SEAM_BAND = 4000.0
# 설계 높이 변화는 이음매 평면까지 성분 거리 ENVELOPE_START~END 사이에서 0→1로 키운다.
# 높은 지형은 감쇠가 1인 중앙 삼각형(내접원 반경 약 88m) 안에 두고, 감쇠 띠에는 20m 이하만 둔다.
ENVELOPE_START = 2500.0
ENVELOPE_END = 9000.0
STAGES = {"Full"}


def point(x, y, h):
    raw = (RADIUS/math.sqrt(3)-x/math.sqrt(6)-y/math.sqrt(2),
           RADIUS/math.sqrt(3)-x/math.sqrt(6)+y/math.sqrt(2),
           RADIUS/math.sqrt(3)+2*x/math.sqrt(6))
    scale = (RADIUS-h)/math.sqrt(sum(v*v for v in raw))
    return tuple(v*scale for v in raw)


def local(p):
    """월드(UE cm) 점을 옥탄트 로컬 X/Y/H로 되돌린다."""
    s3, s6, s2 = math.sqrt(3), math.sqrt(6), math.sqrt(2)
    depth = (p[0]+p[1]+p[2])/s3
    x = RADIUS*(-p[0]-p[1]+2*p[2])/s6/depth
    y = RADIUS*(-p[0]+p[1])/s2/depth
    return (x, y, RADIUS-math.sqrt(sum(v*v for v in p)))


def smooth(t):
    t = max(0.0, min(1.0, t))
    return t*t*(3-2*t)


def flank(d, w, f):
    """능선 단면: 마루 폭 w 안쪽은 1, 사면 f에서 0. 가운데는 선형이라 최대 경사가 평균의 약 1.25배다."""
    if d <= w:
        return 1.0
    c = (d-w)/f
    if c >= 1:
        return 0.0
    k = 0.2
    if c < k:
        v = c*c/(2*k*(1-k))
    elif c > 1-k:
        v = 1-(1-c)*(1-c)/(2*k*(1-k))
    else:
        v = (c-k/2)/(1-k)
    return 1-v


SMOOTH_MAX_CM = 300.0


def smax(a, b, k):
    """다항식 smooth max. 두 값 차이가 k 안이면 둘을 부드럽게 잇는다(k=0이면 max)."""
    if k <= 0:
        return max(a, b)
    t = max(k-abs(a-b), 0.0)/k
    return max(a, b)+t*t*k/4


def segment(px, py, ax, ay, bx, by):
    dx, dy = bx-ax, by-ay
    length2 = dx*dx+dy*dy
    t = max(0.0, min(1.0, ((px-ax)*dx+(py-ay)*dy)/length2)) if length2 else 0.0
    return math.hypot(px-ax-t*dx, py-ay-t*dy), t


def inside(x, y, polygon):
    result = False
    for a, b in zip(polygon, polygon[1:]+polygon[:1]):
        if (a[1] > y) != (b[1] > y) and x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
            result = not result
    return result


def signed_distance(x, y, polygon):
    """다각형 안쪽 양수, 바깥 음수인 가장자리까지 거리."""
    d = min(segment(x, y, a[0], a[1], b[0], b[1])[0] for a, b in zip(polygon, polygon[1:]+polygon[:1]))
    return d if inside(x, y, polygon) else -d


# 능선: 마루 중심선과 정점별 마루 높이(cm). 진행 방향 왼쪽/오른쪽 사면 폭이 다르다(S→N이면 왼쪽=서쪽).
# V004는 지층 단(TIERS)이 산괴를 소유하므로 비워 둔다(형식 보존).
RIDGES = []

# 지층 단: 다각형 안을 목표 높이로 올리고 edge 폭 안에서 아래 지면으로 떨어진다(가장자리가 절벽).
# 높이는 19m 간격으로 통일한다: 지면 0 · 1단 +19 · 2단 +38 · 3단 +57 · 4단(왕관 정상) +76, 대분지 -40.
# 지각 SurfaceData는 반지름 30000 ± 8191.75cm(int16 × 0.25cm)만 담는다. 걷는 지각은 ±81.9m 안에 둔다(V004 첫 베이크가 +84m에서 실패).
LEVEL = 1900
PLATES = [
    # 대능선 산괴: 남북으로 긴 4단 지층. 서쪽은 넓은 디딤면(계단 상승로), 동쪽은 단 가장자리가 겹쳐 대분지로 떨어지는 절벽이다.
    {"id": "MassifTier1", "stage": "Full", "h": LEVEL, "edge": 600,
     "polygon": [(-11600, -5500), (-3500, -6100), (-500, -4800), (0, -2000), (0, 3600), (-2000, 7800), (-4600, 12000), (-9800, 12200), (-11600, 10000)]},
    {"id": "MassifTier2", "stage": "Full", "h": 2*LEVEL, "edge": 600,
     "polygon": [(-9400, -5000), (-3800, -5800), (-1200, -4300), (-800, -1800), (-800, 3300), (-2900, 7000), (-4800, 10400), (-7600, 10600), (-9400, 6500)]},
    {"id": "MassifTier3", "stage": "Full", "h": 3*LEVEL, "edge": 600,
     "polygon": [(-7200, -3600), (-2200, -3500), (-1500, -1600), (-1500, 3000), (-3500, 5600), (-5600, 6200), (-7200, 3200)]},
    {"id": "MassifTier4", "stage": "Full", "h": 4*LEVEL, "edge": 600,
     "polygon": [(-5000, -3200), (-2600, -3200), (-2200, -500), (-2200, 2200), (-3400, 3000), (-5000, 2000)]},
    # 남쪽 메사: 협곡 남쪽의 2단 탁상지. 1단 턱을 거쳐 2단 정상으로 오른다.
    {"id": "MesaTier1", "stage": "Full", "h": LEVEL, "edge": 600,
     "polygon": [(-11000, -9000), (-3600, -8700), (-700, -9300), (-1000, -12000), (-4600, -14400), (-10800, -14200)]},
    {"id": "MesaTier2", "stage": "Full", "h": 2*LEVEL, "edge": 600,
     "polygon": [(-9000, -9100), (-4200, -8800), (-2400, -9400), (-2600, -11600), (-9000, -11800)]},
    # 북쪽 대지: 대분지 북쪽 벽 위의 1단 대지. 북동 진입부 쪽은 이음매 감쇠가 자연 경사로를 만든다.
    {"id": "NorthTableland", "stage": "Full", "h": LEVEL, "edge": 900,
     "polygon": [(-1500, 4200), (3000, 3800), (8800, 2800), (14000, 6000), (18000, 9000), (10000, 19000), (-7000, 19000), (-4800, 12200), (-2500, 9000)]},
]

# 계단 단구(영역 한정 양자화). V004는 지층 단으로 직접 만들므로 쓰지 않는다(형식 보존).
TERRACES = []

# 분지·평탄부: 반경 radius 안은 목표 높이, falloff에서 주변 지면으로 돌아간다.
BASINS = [
    {"id": "DeepBasin", "stage": "Full", "xy": (3000, -300), "h": -4000, "radius": 2300, "falloff": 1500},
]
PADS = []

# 선형 경로: 정점 (x, y, h, 반폭). h가 None이면 경로 적용 전 지면 높이를 쓴다. shoulder는 옆 경사 폭이다.
# mode: set = 경로 높이로 맞춤(성토·절토 모두), cut = 경로보다 높은 지면만 깎음(협곡).
PATHS = [
    # 서쪽 계단 상승로: 단마다 절벽면을 비스듬히 타고 오르는 경사로. 꺾이는 곳은 디딤면 위 평지 착지부다.
    {"id": "StairRamp1", "stage": "Full", "mode": "set", "shoulder": 350, "landing": 400,
     "points": [(-12300, -3600, None, 350), (-11400, 2000, None, 350)]},
    {"id": "StairRamp2", "stage": "Full", "mode": "set", "shoulder": 350, "landing": 400,
     "points": [(-10700, 2600, None, 350), (-9200, -3200, None, 350)]},
    {"id": "StairRamp3", "stage": "Full", "mode": "set", "shoulder": 350, "landing": 400,
     "points": [(-8500, -3600, None, 350), (-7000, 2400, None, 350)]},
    {"id": "StairRamp4", "stage": "Full", "mode": "set", "shoulder": 350, "landing": 400,
     "points": [(-6300, 2900, None, 350), (-4700, -3000, None, 350)]},
    # 북쪽 하강: 3단 북쪽 → 2단(고개로 이어지는 북쪽 디딤면). 정상↔3단은 서쪽 4번 경사로를 함께 쓴다.
    {"id": "NorthRamp2", "stage": "Full", "mode": "set", "shoulder": 350, "landing": 400,
     "points": [(-3000, 4300, None, 350), (-8300, 7600, None, 350)]},
    # 북서 저지 → 1단 → 2단.
    {"id": "NorthwestRamp1", "stage": "Full", "mode": "set", "shoulder": 350, "landing": 400,
     "points": [(-12300, 4200, None, 350), (-11400, 9800, None, 350)]},
    {"id": "NorthwestRamp2", "stage": "Full", "mode": "set", "shoulder": 350, "landing": 400,
     "points": [(-10300, 10400, None, 350), (-9100, 4600, None, 350)]},
    # 2단 고개 → 1단 북쪽 대지.
    {"id": "SaddleRamp", "stage": "Full", "mode": "set", "shoulder": 350, "landing": 400,
     "points": [(-4600, 9000, None, 350), (100, 12000, None, 350)]},
    # 대분지 → 보스 고원으로 오르는 넓은 상승 골짜기. 가운데 평탄한 층계참에서 잔도·남동 하강로와 만난다.
    {"id": "BossValley", "stage": "Full", "mode": "set", "shoulder": 1800,
     "points": [(4000, -300, -4000, 1500), (7600, -100, -2400, 1700), (8600, 0, -2400, 1800), (13800, 0, 0, 2000)]},
    # 북쪽 대지 → 골짜기: 대분지 북쪽 절벽에 깎은 잔도.
    {"id": "NorthLedge", "stage": "Full", "mode": "set", "shoulder": 300, "fade_end": 900,
     "points": [(0, 5200, None, 350), (2200, 3500, 660, 300), (5400, 2700, -860, 300), (8400, 1200, -2400, 350)]},
    # 협곡: 서남 저지에서 산괴와 메사 사이를 가르고 대분지로 내려간다. 가운데에 넓어진 방(Pod)이 있다.
    {"id": "RiftCanyon", "stage": "Full", "mode": "cut", "shoulder": 800,
     "points": [(-14500, -7800, 0, 800), (-11000, -7400, -300, 650), (-7200, -7300, -1300, 1300),
                (-4300, -7300, -2200, 650), (-1600, -5400, -3200, 750), (1200, -2600, -4000, 1100)]},
    # 남동 진입 → 골짜기.
    {"id": "SoutheastDescent", "stage": "Full", "mode": "set", "shoulder": 600,
     "points": [(8400, -14800, None, 450), (7600, -8500, -300, 450), (7900, -2000, -2400, 500)]},
    # 남서 저지 → 메사 1단(서쪽 절벽면) → 메사 2단(남쪽 절벽면).
    {"id": "MesaRamp1", "stage": "Full", "mode": "set", "shoulder": 350, "landing": 400,
     "points": [(-12100, -15200, None, 350), (-10800, -9600, None, 350)]},
    {"id": "MesaRamp2", "stage": "Full", "mode": "set", "shoulder": 350, "landing": 400,
     "points": [(-9000, -13200, None, 350), (-3300, -11000, None, 350)]},
]


def active(items):
    return [item for item in items if item["stage"] in STAGES]


def envelope(x, y):
    """이음매 보존·전이 띠 앞에서 설계 높이 변화를 줄인다."""
    return smooth((min(point(x, y, 0))-ENVELOPE_START)/(ENVELOPE_END-ENVELOPE_START))


def base(x, y):
    return 110*math.sin(x/5100+0.4)*math.cos(y/6300-0.7)+70*math.sin((x-0.6*y)/3900+1.3)


def ridge_profile(ridge, x, y):
    """가장 가까운 마루 선분까지 거리, 마루 높이, 사면 폭(좌우를 각도로 부드럽게 섞는다)."""
    best = None
    pts = ridge["points"]
    for (ax, ay, ah), (bx, by, bh) in zip(pts, pts[1:]):
        d, t = segment(x, y, ax, ay, bx, by)
        if best is None or d < best[0]:
            length = math.hypot(bx-ax, by-ay)
            cross = ((bx-ax)*(y-ay)-(by-ay)*(x-ax))/length
            best = (d, ah+(bh-ah)*t, cross)
    d, crest, cross = best
    side = cross/d if d > 1e-6 else 0.0
    wt = smooth((side+0.5)/1.0)
    return d, crest, ridge["flank_right"]+(ridge["flank_left"]-ridge["flank_right"])*wt


def terrace(h, step, cliff, rise):
    n = math.floor(h/step)
    f = h/step-n
    if f < 1-cliff:
        v = f/(1-cliff)*rise
    else:
        v = rise+(1-rise)*smooth((f-(1-cliff))/cliff)
    return (n+v)*step


def edge_jitter(x, y):
    """단 가장자리선을 흔드는 저주파 굴곡(cm). 같은 높이 간격의 단이 계단 피라미드처럼 보이지 않게 한다."""
    return 170*math.sin(x/1300+0.7)*math.cos(y/1700-0.4)+90*math.sin((x-y)/700+2.1)


# 외곽 보조 지형 1: 소소한 둔덕(+)·오목지(-). 단·경로가 덮는 곳에는 두지 않는다(V009).
OUTER_SWELLS = [
    ((-16500, -12000), 300, 1500), ((-14000, 16500), 300, 1800), ((0, -17000), -250, 1600),
    ((15800, -8200), 280, 1500), ((12200, 11500), 250, 1600), ((19300, 5000), 200, 1300),
    ((-16200, 10500), -200, 1500),
]


def swell(x, y):
    h = 0.0
    for (cx, cy), amp, radius in OUTER_SWELLS:
        d = math.hypot(x-cx, y-cy)
        if d < radius:
            h += amp*(1-smooth(d/radius))
    return h


def landform(x, y):
    """경로 적용 전 지면(감쇠 전)."""
    h = base(x, y)+swell(x, y)
    excess = 0.0
    for ridge in active(RIDGES):
        d, crest, width = ridge_profile(ridge, x, y)
        p = flank(d, ridge["crest"], width)
        if p > 0:
            e = max(0.0, (crest-h)*p)
            excess = smax(excess, e, min(SMOOTH_MAX_CM, excess, e))
    h += excess
    for plate in active(PLATES):
        sd = signed_distance(x, y, plate["polygon"])+edge_jitter(x, y)
        if sd > -plate["edge"]:
            wt = smooth(1+sd/plate["edge"])
            target = plate["h"]+0.5*base(x, y)
            h = max(h, h+(target-h)*wt)
    for t in active(TERRACES):
        sd = signed_distance(x, y, t["polygon"])
        if sd > -t["blend"]:
            wt = smooth(1+sd/t["blend"])
            h += (terrace(h, t["step"], t["cliff"], t["rise"])-h)*wt
    for basin in active(BASINS+PADS):
        bx, by = basin["xy"]
        wt = 1-smooth((math.hypot(x-bx, y-by)-basin["radius"])/basin["falloff"])
        h += (basin["h"]-h)*wt
    return h


_PATH_CACHE = {}


def path_points(path):
    """정점 높이를 확정한다. landing이 있으면 양 끝에서 그 길이만큼 평지 착지부를 넣는다(꺾이는 곳의 옆 경사 방지)."""
    pts = _PATH_CACHE.get(path["id"])
    if pts is None:
        pts = [(x, y, landform(x, y) if h is None else h, w) for x, y, h, w in path["points"]]
        landing = path.get("landing", 0)
        if landing:
            (ax, ay, ah, aw), (bx, by, bh, bw) = pts[0], pts[1]
            n = math.hypot(bx-ax, by-ay)
            first = (ax+(bx-ax)*landing/n, ay+(by-ay)*landing/n, ah, aw)
            (cx, cy, ch, cw), (dx, dy, dh, dw) = pts[-2], pts[-1]
            n = math.hypot(dx-cx, dy-cy)
            last = (dx-(dx-cx)*landing/n, dy-(dy-cy)*landing/n, dh, dw)
            pts = [pts[0], first]+pts[1:-1]+[last, pts[-1]]
        _PATH_CACHE[path["id"]] = pts
    return pts


def path_profile(path, x, y):
    """가장 가까운 선분까지 거리, 경로 높이, 반폭, 끝점까지 남은 길이."""
    best = None
    pts = path_points(path)
    lengths = [math.hypot(b[0]-a[0], b[1]-a[1]) for a, b in zip(pts, pts[1:])]
    for i, ((ax, ay, ah, aw), (bx, by, bh, bw)) in enumerate(zip(pts, pts[1:])):
        d, t = segment(x, y, ax, ay, bx, by)
        if best is None or d < best[0]:
            best = (d, ah+(bh-ah)*t, aw+(bw-aw)*t, (1-t)*lengths[i]+sum(lengths[i+1:]))
    return best


def ground(x, y):
    h = landform(x, y)
    for path in active(PATHS):
        d, ph, half, remain = path_profile(path, x, y)
        if d >= half+path["shoulder"]:
            continue
        wt = 1-smooth((d-half)/path["shoulder"])
        # fade_end: 끝 구간에서 이미 있는 지면(먼저 적용된 경로 포함)으로 녹아든다. 끝 캡의 평지 단차를 없앤다.
        if path.get("fade_end"):
            wt *= smooth(remain/path["fade_end"])
        if path["mode"] == "cut":
            h -= max(0.0, h-ph)*wt
        else:
            h += (ph-h)*wt
    return h*envelope(x, y)


def features():
    """manifest 기록용 높이장 구성."""
    return {"ridges": active(RIDGES), "plates": active(PLATES), "terraces": active(TERRACES), "basins": active(BASINS),
            "pads": active(PADS), "paths": [dict(p, resolved=path_points(p)) for p in active(PATHS)]}


# 암반: 볼록 외곽(정점마다 지면 위 높이 cm)의 닫힌 각기둥. 밑면은 지면 아래로 매입한다.
ROCKS = [
    # 보스 랜드마크: 보스 고원 뒤의 쌍둥이 첨탑(지면 위 95~125m). 두 첨탑 사이 틈은 걸어서 통과한다.
    {"id": "TwinSpireNorth", "stage": "Full", "role": "Landmark",
     "outline": [(17000, 800, 7000), (18600, 700, 12500), (20200, 1500, 9000), (20000, 3600, 6500), (18300, 4300, 5000), (16900, 3000, 5500)]},
    {"id": "TwinSpireSouth", "stage": "Full", "role": "Landmark",
     "outline": [(17000, -800, 8000), (18700, -700, 11000), (20300, -1600, 9500), (20100, -3800, 6000), (18200, -4400, 4500), (16800, -3000, 6000)]},
    # 보스 고원 좌우 엄폐 암반.
    {"id": "ArenaFlankNorth", "stage": "Full", "role": "Cover",
     "outline": [(13200, 3500, 700), (14800, 3900, 900), (14600, 4700, 800), (13000, 4300, 600)]},
    {"id": "ArenaFlankSouth", "stage": "Full", "role": "Cover",
     "outline": [(13000, -3800, 650), (14700, -4200, 950), (14900, -5000, 800), (13200, -4700, 600)]},
    # 왕관 정상의 암관(부분 엄폐·벽보행 접근면).
    {"id": "CrownTor", "stage": "Full", "role": "Cover",
     "outline": [(-4500, 600, 500), (-3800, 400, 900), (-3500, 1100, 800), (-3900, 1700, 600), (-4500, 1400, 450)]},
    # 대분지 바닥의 거석(엄폐·시야 차단).
    {"id": "BasinMonolith", "stage": "Full", "role": "Cover",
     "outline": [(3600, 900, 1200), (4600, 1100, 1600), (4500, 1700, 1400), (3500, 1500, 1000)]},
]


def rock_center(rock):
    pts = rock["outline"]
    return (sum(p[0] for p in pts)/len(pts), sum(p[1] for p in pts)/len(pts))


def rock_outline(rock, scale=1.0):
    cx, cy = rock_center(rock)
    return [(cx+(x-cx)*scale, cy+(y-cy)*scale) for x, y, _ in rock["outline"]]


LANDMARK_ROCK = "TwinSpireNorth"

# 연결표·검토점. 좌표는 지면 위 보행 지점이다.
NODES = [
    {"id": "WestEntry", "stage": "Full", "xy": (-17600, 0)},
    {"id": "StairFoot", "stage": "Full", "xy": (-12700, -3600)},
    {"id": "StairTerrace2", "stage": "Full", "xy": (-8200, 4800)},
    {"id": "CrownEastRim", "stage": "Full", "xy": (-2300, -800)},
    {"id": "CrownSummit", "stage": "Full", "xy": (-3400, -800)},
    {"id": "CrownSouthRim", "stage": "Full", "xy": (-3800, -2800)},
    {"id": "DeepBasin", "stage": "Full", "xy": (3300, -300)},
    {"id": "CanyonMouth", "stage": "Full", "xy": (-14500, -7800)},
    {"id": "CanyonChamber", "stage": "Full", "xy": (-7200, -7300)},
    {"id": "NorthSaddle", "stage": "Full", "xy": (-5400, 9000)},
    {"id": "NorthwestLowland", "stage": "Full", "xy": (-14500, 9000)},
    {"id": "NorthTableland", "stage": "Full", "xy": (2000, 6800)},
    {"id": "NorthLedgeMid", "stage": "Full", "xy": (2200, 3500)},
    {"id": "TablelandRim", "stage": "Full", "xy": (4500, 3600)},
    {"id": "SouthMesa", "stage": "Full", "xy": (-6000, -10300)},
    {"id": "ValleyRise", "stage": "Full", "xy": (8200, 0)},
    {"id": "BossPlateau", "stage": "Full", "xy": (14800, 0)},
    {"id": "SoutheastEntry", "stage": "Full", "xy": (8400, -16000)},
    {"id": "NortheastEntry", "stage": "Full", "xy": (8400, 16000)},
    {"id": "SouthwestLowland", "stage": "Full", "xy": (-14500, -11000)},
    {"id": "UnderHall", "stage": "Full", "xy": (2300, 6000), "layer": "cave"},
    {"id": "HallPitFloor", "stage": "Full", "xy": (3850, 7400), "layer": "cave"},
    {"id": "HallBasinGate", "stage": "Full", "xy": (2500, 1900)},
    {"id": "MassifCavern", "stage": "Full", "xy": (-9200, -4000), "layer": "cave"},
    {"id": "CavernWestGate", "stage": "Full", "xy": (-13800, -4600)},
    {"id": "ValleySkyIsle", "stage": "Full", "xy": (7800, -5600), "layer": "deck"},
    {"id": "WestSkyIsle", "stage": "Full", "xy": (-14600, 4800), "layer": "deck"},
]

# 보행 연결(검사용 중심선). 종류: walk / walk-exposed(노출 직행) / walk-flank(측면) / walk-outer(바깥 순환) / walk-crest(마루).
ROUTES = [
    {"id": "EntryToStairFoot", "stage": "Full", "kind": "walk", "xy": [(-19500, 0), (-17600, 0), (-12700, -3400)]},
    {"id": "WestStairAscent", "stage": "Full", "kind": "walk-exposed",
     "xy": [(-12700, -3600), (-12300, -3600), (-11400, 2000), (-10700, 2600), (-9200, -3200), (-8500, -3600), (-7000, 2400), (-6300, 2900), (-4700, -3000), (-3800, -2800), (-3400, -800)]},
    {"id": "CrownToSaddle", "stage": "Full", "kind": "walk-crest",
     "xy": [(-3400, -800), (-3800, -2800), (-4700, -3000), (-6300, 2900), (-3000, 4300), (-8300, 7600), (-7400, 8600), (-5400, 9000)]},
    {"id": "NorthwestToSaddle", "stage": "Full", "kind": "walk-flank",
     "xy": [(-14500, 9000), (-13000, 4200), (-12300, 4200), (-11400, 9800), (-10300, 10400), (-9100, 4600), (-8300, 5600), (-8450, 7600), (-8000, 8400), (-5400, 9000)]},
    {"id": "SaddleToTableland", "stage": "Full", "kind": "walk", "xy": [(-5400, 9000), (-4600, 9000), (100, 12000), (2000, 6800)]},
    {"id": "NorthLedgeDescent", "stage": "Full", "kind": "walk-flank",
     "xy": [(2000, 6800), (0, 5200), (2200, 3500), (5400, 2700), (8400, 1200), (8200, 0)]},
    {"id": "NortheastToTableland", "stage": "Full", "kind": "walk-outer", "xy": [(8400, 16000), (5000, 10500), (2000, 6800)]},
    {"id": "CanyonRun", "stage": "Full", "kind": "walk-flank",
     "xy": [(-14500, -11000), (-14500, -7800), (-11000, -7400), (-7200, -7300), (-4300, -7300), (-1600, -5400), (1200, -2600), (3300, -300)]},
    {"id": "EntryToCanyonMouth", "stage": "Full", "kind": "walk-outer", "xy": [(-17600, 0), (-16000, -5000), (-14500, -7800)]},
    {"id": "EntryToNorthwest", "stage": "Full", "kind": "walk-outer", "xy": [(-17600, 0), (-16000, 5000), (-14500, 9000)]},
    {"id": "BasinToBoss", "stage": "Full", "kind": "walk", "xy": [(3300, -300), (4000, -300), (7600, -100), (8600, 0), (13800, 0), (14800, 0)]},
    {"id": "SoutheastToValley", "stage": "Full", "kind": "walk-outer", "xy": [(8400, -16000), (8400, -14800), (7600, -8500), (7900, -2000), (8200, 0)]},
    {"id": "SkyBridgeCrossing", "stage": "Full", "kind": "walk-exposed",
     "deck": True, "xy": [(-8500, -3600), (-7700, -4600), (-4400, -5300), (-4500, -7300), (-4600, -9300), (-6000, -10300)]},
    # 지하 대전당: 대분지 남쪽 문 ↔ 골짜기 동쪽 터널.
    {"id": "HallFromBasin", "stage": "Full", "kind": "walk-flank", "layer": "cave",
     "xy": [(3300, -300), (2500, 1300), (2500, 4450), (2300, 6000)]},
    {"id": "HallFromValley", "stage": "Full", "kind": "walk-flank", "layer": "cave",
     "xy": [(8200, 0), (8250, 350), (8790, 937), (9234, 1551), (9272, 2284), (9096, 3026), (8576, 3601), (7940, 4087), (7330, 4540), (6662, 4922), (5994, 5303), (5332, 5648), (4594, 5700), (4200, 5700), (2300, 6000), (3850, 7400)]},
    # 산괴 지하 동굴: 서쪽 저지 터널 ↔ 협곡 터널.
    {"id": "CavernFromWest", "stage": "Full", "kind": "walk-flank", "layer": "cave",
     "xy": [(-13800, -4600), (-12900, -4600), (-10950, -4600), (-9200, -4000)]},
    {"id": "CavernFromCanyon", "stage": "Full", "kind": "walk-flank", "layer": "cave",
     "xy": [(-9000, -7400), (-9000, -6800), (-9000, -5450), (-9200, -4000)]},
    {"id": "MesaAscent", "stage": "Full", "kind": "walk-exposed",
     "xy": [(-14500, -11000), (-12900, -15200), (-12100, -15200), (-10800, -9600), (-10100, -10000), (-10000, -12600), (-9000, -13200), (-3300, -11000), (-3500, -10200), (-6000, -10300)]},
]

# Pod: 사수 조건이 서로 다르다.
PODS = [
    # 정상 디딤면은 폭 약 21m이고 바로 아래 단이 19m 밑이라 반경 25m로는 후보가 모자란다(4명 부족). 3단 디딤면까지 넣는다.
    {"id": "CrownSummit", "stage": "Full", "xy": (-3400, -800), "condition": "ExposedSummit", "formation_radius": 3500},
    {"id": "StairTerrace2", "stage": "Full", "xy": (-8200, 4800), "condition": "TerraceLedge"},
    # 산괴 지하 동굴 몸체가 협곡 바닥 일부를 덮어(지각 Layer 후보 감소) 반경을 넓혀 앞뒤 협곡 바닥까지 포함한다.
    {"id": "CanyonChamber", "stage": "Full", "xy": (-7200, -7300), "condition": "CanyonChamber", "formation_radius": 3200},
    {"id": "DeepBasin", "stage": "Full", "xy": (3300, -300), "condition": "DeepBasin"},
    {"id": "NorthSaddle", "stage": "Full", "xy": (-5400, 9000), "condition": "HighPass"},
    {"id": "NorthTableland", "stage": "Full", "xy": (2000, 6800), "condition": "TablelandRim"},
    {"id": "SouthMesa", "stage": "Full", "xy": (-6000, -10300), "condition": "MesaTop"},
    # 대전당 동쪽 통로 입구 몸체가 층계참 북쪽을 덮어(지각 Layer 후보 감소) 편성 반경을 넓힌다(V011).
    # 동쪽 통로 입구 몸체 외곽(y=150)에서 2.5m 이상 떨어져야 베이크 Pod 여유(250cm)를 통과한다(V011, 원래 (8200,0)).
    {"id": "ValleyRise", "stage": "Full", "xy": (8200, -600), "condition": "ValleyRise", "formation_radius": 3200},
    # 지하·부유섬 전투 공간.
    {"id": "UnderHall", "stage": "Full", "xy": (2300, 6000), "condition": "UndergroundHall", "layer": "cave", "formation_radius": 2000},
    {"id": "MassifCavern", "stage": "Full", "xy": (-9200, -4000), "condition": "LowCavern", "layer": "cave", "formation_radius": 2000,
     "formation": [("Crawler01", 12), ("Melee01", 9), ("Ranged01", 6)]},
    {"id": "ValleySkyIsle", "stage": "Full", "xy": (7800, -5600), "condition": "SkyIsland", "layer": "deck", "formation_radius": 3500},
    {"id": "WestSkyIsle", "stage": "Full", "xy": (-14600, 4800), "condition": "SkyIsland", "layer": "deck", "formation_radius": 3500},
]

# Pod 출현 편성: 전역 세트와 같은 종·수(27)를 Pod 중심 반경 25m(3D) 안 안전 후보에 둔다.
# 전역 편성의 반경 15m는 단 가장자리 절벽·저지대(후보 간격이 넓어짐)에서 22~25개 후보뿐이라 전량 배치가 안 됐다.
# 25m면 정상은 바로 아래 단, 협곡 방은 앞뒤 협곡 바닥까지 접근 구역에 포함한다.
ENEMY_FORMATION = [("Flyer01", 9), ("Crawler01", 9), ("Melee01", 6), ("Ranged01", 3)]
FORMATION_RADIUS = 2500.0


def pod_formations(pod):
    return [{"enemyConfigPath": "/Game/Enemy/DA_EnemyEntityConfig_PureEntity_%s.DA_EnemyEntityConfig_PureEntity_%s" % (name, name),
             "count": count, "offset": {"x": 0.0, "y": 0.0}, "radius": pod.get("formation_radius", FORMATION_RADIUS)}
            for name, count in pod.get("formation", ENEMY_FORMATION)]


def pod_has_flyers(pod):
    return any(name.startswith("Flyer") for name, _ in pod.get("formation", ENEMY_FORMATION))


# 훅: 앵커는 착지 지점 위 공중 점이다. from은 실제 출발 검토점.
# V010 정리: 10m~30m 간격으로 겹치던 앵커 8개를 허브 4개로 합치고, 비어 있던 흐름 2개(메사→정상, 고개→3단)는 허브 공유·신규 1개로 채웠다.
# 출발점 여러 곳이 같은 앵커를 쓰므로 앵커는 모든 출발점에서 보이도록 착지점 위 높이(above)를 크게 둔다.
ANCHORS = [
    # 대지 남쪽 가장자리 허브: 대분지 바닥·골짜기 층계참에서 대지로 복귀, 대전당 싱크홀 바닥에서 탈출(싱크홀을 통해 보인다).
    {"id": "TablelandRimHub", "stage": "Full", "role": "CliffReturn", "landing": (3850, 5800), "above": 3500,
     "from": ["DeepBasin", "ValleyRise", "HallPitFloor"], "max_distance": 11500},
    # 메사 정상 허브: 협곡 방에서 탈출, 정상 남쪽 가장자리에서 협곡 건너 하강.
    {"id": "MesaTopHub", "stage": "Full", "role": "CanyonCrossing", "landing": (-6500, -10000), "above": 1600,
     "from": ["CanyonChamber", "CrownSouthRim"], "max_distance": 11000},
    # 정상 남쪽 가장자리: 메사에서 협곡을 건너 정상으로 복귀(메사 정상 허브의 역방향).
    {"id": "CrownSouthRimAnchor", "stage": "Full", "role": "CanyonCrossing", "landing": (-3800, -2600), "above": 1500,
     "from": ["SouthMesa"], "max_distance": 11000},
    # 3단 서쪽 허브: 계단 2단에서 한 단 건너뛰기, 서쪽 부유섬에서 산괴로 이탈, 북쪽 고개에서 정상 쪽으로 복귀.
    {"id": "Terrace3Hub", "stage": "Full", "role": "TerraceClimb", "landing": (-5700, 4300), "above": 1800,
     "from": ["StairTerrace2", "WestSkyIsle", "NorthSaddle"], "max_distance": 9500},
    # 골짜기 남쪽 부유섬 허브: 골짜기 층계참·보스 고원에서 섬으로.
    {"id": "ValleyIsleHub", "stage": "Full", "role": "IslandClimb", "landing": (8700, -3800), "above": 3000,
     "from": ["ValleyRise", "BossPlateau"], "max_distance": 12000, "layer": "deck"},
    # 서쪽 부유섬: 서쪽 진입부에서 섬으로.
    {"id": "EntryToWestIsle", "stage": "Full", "role": "IslandClimb", "landing": (-15760, 2950), "above": 3000,
     "from": ["WestEntry"], "max_distance": 11000, "layer": "deck"},
]

# 사선 기대값: (출발 검토점, 목표, 보여야 하는가). 목표는 "Pod_<id>" 또는 "LandmarkTop".
SIGHT_EXPECTATIONS = [
    ("WestEntry", "Pod_DeepBasin", False), ("StairFoot", "Pod_DeepBasin", False),
    ("CrownEastRim", "Pod_DeepBasin", True), ("CanyonChamber", "Pod_CrownSummit", False), ("TablelandRim", "Pod_DeepBasin", True),
    ("WestEntry", "LandmarkTop", True), ("CrownSummit", "LandmarkTop", True), ("DeepBasin", "LandmarkTop", True),
]

# 렌더 시점: (이름, 눈 local (x, y, h 또는 None=지면+170), 목표 local, 렌즈 mm).
VIEWS = [
    ("Overview", (-17000, -19000, 15000), (-1000, -1000, 1500), 18),
    ("OverviewEast", (24000, 14000, 24000), (-2000, -1000, 2000), 20),
    ("WestEntryEye", (-17600, 0, None), (-4000, 0, 6000), 24),
    ("CrownSummitEye", (-2600, -800, None), (16000, 0, 2000), 24),
    ("DeepBasinEye", (4600, -1300, None), (-3000, 0, 7000), 20),
    ("SkyBridgeEye", (-4000, -7300, -1700), (-4500, -7400, 4500), 18),
    ("CanyonChamberEye", (-9800, -8000, None), (-4500, -7300, 500), 20),
    ("WestStairEye", (-15500, -6000, None), (-6000, 0, 4000), 22),
    ("NorthTablelandEye", (2000, 6800, None), (3000, -2000, -2000), 24),
    ("SouthMesaEye", (-6000, -10300, None), (4000, 0, 0), 24),
    ("ValleyRiseEye", (8200, 0, None), (19000, 0, 5000), 24),
    ("UnderHallEye", (900, 4700, -3830), (4200, 7400, -2200), 18),
    ("HallGateEye", (2500, 600, -3830), (2500, 4400, -3200), 22),
    ("MassifCavernEye", (-10400, -2600, -330), (-8000, -5300, 0), 18),
    ("CavernWestGateEye", (-14200, -4700, 170), (-10600, -4600, -100), 22),
    ("SkyIslesEye", (2000, -800, -3800), (7800, -5600, 5800), 20),
    ("WestSkyIsleEye", (-17600, 0, 170), (-14600, 4800, 6200), 22),
    ("CrownToIsleEye", (-2400, -900, None), (7800, -5600, 5500), 22),
    ("ValleyIsleTopEye", (6200, -4300, 5670), (16000, 0, 4000), 22),
    ("SouthwestOuterEye", (-14000, -12500, None), (-16500, -16000, 300), 24),
    ("NorthwestOuterEye", (-13500, 11500, None), (-15500, 15000, 400), 24),
    ("SoutheastOuterEye", (11500, -6000, None), (17000, -7500, 300), 24),
    ("HallEastMouthEye", (7600, -600, None), (9000, 1600, -2000), 22),
    ("HallEastTunnelEye", (6000, 5050, -3150), (4300, 5700, -3800), 22),
]


def bridge(rid, a, b, half, thickness, embed):
    """두 단을 잇는 석교 몸체. 윗면은 양 끝 지면 높이(+5cm)로 잇고, 지각은 절단하지 않는다."""
    dx, dy = b[0]-a[0], b[1]-a[1]
    n = math.hypot(dx, dy)
    ux, uy = dx/n, dy/n
    a2, b2 = (a[0]-ux*embed, a[1]-uy*embed), (b[0]+ux*embed, b[1]+uy*embed)
    corners = [(a2[0]+uy*half, a2[1]-ux*half), (b2[0]+uy*half, b2[1]-ux*half), (b2[0]-uy*half, b2[1]+ux*half), (a2[0]-uy*half, a2[1]+ux*half)]
    ha, hb = ground(*a)+5, ground(*b)+5
    tops = [ha, hb, hb, ha]
    outline = [(x, y, t) for (x, y), t in zip(corners, tops)]
    return {"id": rid, "stage": "Full", "role": "Bridge", "outline": outline, "top_abs": True,
            "bottom_abs": min(tops)-thickness, "cut": False, "deck": [list(a2), list(b2), ha, hb, half]}


# 하늘 석교: 산괴 2단 남쪽 띠 ↔ 메사 2단. 협곡 바닥 위 약 60m.
ROCKS.append(bridge("SkyBridge", (-4400, -5500), (-4600, -9300), 300, 500, 200))


def densify(polygon, step=300):
    """다각형 변을 step 이하로 나눈 고리. 지각 절단선과 몸체 윗면 테두리가 같은 점을 공유하게 한다."""
    ring = []
    for (ax, ay), (bx, by) in zip(polygon, polygon[1:]+polygon[:1]):
        n = max(1, math.ceil(math.hypot(bx-ax, by-ay)/step))
        ring += [(ax+(bx-ax)*k/n, ay+(by-ay)*k/n) for k in range(n)]
    return ring


def tunnel(a, b, half, fa, fb, height):
    """두 점 사이 직선 통로 절단체: 바닥은 fa→fb로 기울고, 윗면은 바닥+height. 지형이 윗면보다 낮은 구간은 열린 도랑이 된다."""
    n = math.hypot(b[0]-a[0], b[1]-a[1])
    nx, ny = -(b[1]-a[1])/n*half, (b[0]-a[0])/n*half
    poly = [(a[0]+nx, a[1]+ny, fa), (b[0]+nx, b[1]+ny, fb), (b[0]-nx, b[1]-ny, fb), (a[0]-nx, a[1]-ny, fa)]
    return ("tunnel", poly, [z+height for _, _, z in poly])


def _fillet(points, radius, step=100):
    """꺾인 경로선의 각 모서리를 반경 radius 원호로 바꾼다(양 끝점 유지). 반경이 통로 반폭보다 커야 안쪽 벽이 스스로 겹치지 않는다."""
    out = [points[0]]
    for i in range(1, len(points)-1):
        (ax, ay), (bx, by), (cx, cy) = points[i-1], points[i], points[i+1]
        d1, d2 = math.hypot(bx-ax, by-ay), math.hypot(cx-bx, cy-by)
        u1, u2 = ((bx-ax)/d1, (by-ay)/d1), ((cx-bx)/d2, (cy-by)/d2)
        turn = math.acos(max(-1.0, min(1.0, u1[0]*u2[0]+u1[1]*u2[1])))
        if turn < 1e-3:
            out.append((bx, by))
            continue
        r = min(radius, 0.45*min(d1, d2)/math.tan(turn/2))
        t = r*math.tan(turn/2)
        p0 = (bx-u1[0]*t, by-u1[1]*t)
        side = 1 if u1[0]*u2[1]-u1[1]*u2[0] > 0 else -1
        cx0, cy0 = p0[0]-u1[1]*r*side, p0[1]+u1[0]*r*side
        a0 = math.atan2(p0[1]-cy0, p0[0]-cx0)
        n = max(2, math.ceil(r*turn/step))
        out += [(cx0+r*math.cos(a0+side*turn*k/n), cy0+r*math.sin(a0+side*turn*k/n)) for k in range(n+1)]
    out.append(points[-1])
    return out


def loft_corridor(path, half, height, h_start, h_end, level_start, level_end, mouth=True, twist_len=400, step=100):
    """경로선을 따라 단면(바닥 좌·우, 천장 우·좌)을 쓸어 만든 통로 절단체 하나. 꺾이는 곳은 반경 12m 원호, 바닥은 끊김 없이 이어진다.
    바닥 중심 높이: 처음 level_start는 h_start, 마지막 level_end는 h_end, 그 사이는 직선. mouth면 첫 고리의 좌·우 바닥을
    그 자리 지면(경사진 잔도)에 5cm 아래로 맞추고 twist_len에 걸쳐 수평으로 풀어 입구 턱과 비틀린 면을 없앤다(V011)."""
    pts = _fillet(list(path), 1200)
    dense = [pts[0]]
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        n = max(1, math.ceil(math.hypot(bx-ax, by-ay)/step))
        dense += [(ax+(bx-ax)*k/n, ay+(by-ay)*k/n) for k in range(1, n+1)]
    arc = [0.0]
    for (ax, ay), (bx, by) in zip(dense, dense[1:]):
        arc.append(arc[-1]+math.hypot(bx-ax, by-ay))
    total = arc[-1]
    rings = []
    left0 = right0 = None
    for i, (x, y) in enumerate(dense):
        ax, ay = dense[max(0, i-1)]
        bx, by = dense[min(len(dense)-1, i+1)]
        m = math.hypot(bx-ax, by-ay)
        nx, ny = -(by-ay)/m, (bx-ax)/m
        sarc = arc[i]
        if sarc <= level_start:
            f = h_start
        elif sarc >= total-level_end:
            f = h_end
        else:
            f = h_start+(h_end-h_start)*(sarc-level_start)/(total-level_start-level_end)
        lx, ly, rx, ry = x+nx*half, y+ny*half, x-nx*half, y-ny*half
        if mouth and i == 0:
            left0, right0 = ground(lx, ly)-5, ground(rx, ry)-5
        fl = fr = f
        if mouth and sarc < twist_len:
            w = smooth(sarc/twist_len)
            fl, fr = left0+(f-left0)*w, right0+(f-right0)*w
        top = max(fl, fr)+height
        rings.append([(lx, ly, fl), (rx, ry, fr), (rx, ry, top), (lx, ly, top)])
    return ("loft", rings, None)


def chamber(polygon, floor, ceiling):
    return ("chamber", [(x, y, floor) for x, y in polygon], [ceiling]*len(polygon))


def pit(polygon, bottom, top):
    """방 천장을 지면까지 뚫는 싱크홀. 바닥은 방 천장 아래로 겹친다(걷는 면 아님)."""
    return ("pit", [(x, y, bottom) for x, y in polygon], [top]*len(polygon))


def terrain_cave(rid, footprint, bottom, parts):
    """지형 아래 지하 공간. 윗면이 지형 높이장을 그대로 샘플링한 닫힌 몸체(지각 대신)에서 parts를 EXACT Boolean으로 판다.
    지각은 같은 테두리 고리(300cm 간격)에서 정확히 잘라, 몸체 윗면과 테두리 점을 공유한다."""
    ring = densify(footprint)
    return {"id": rid, "stage": "Full", "role": "Cave", "outline": [(x, y, ground(x, y)) for x, y in ring], "top_abs": True,
            "terrain_top": True, "cut_scale": 1.0, "bottom_abs": bottom, "cave": {"parts": parts}}


def island(rid, center, radius, top, depth, sides=16, phase=0.3):
    """지각을 자르지 않는 부유섬. 윗면은 평탄(절대 높이 top), 밑은 좁아지는 뿔대다."""
    outline = [(center[0]+radius*math.cos(phase+i*math.tau/sides)*(1+0.04*math.sin(3*i)),
                center[1]+radius*math.sin(phase+i*math.tau/sides)*(1+0.04*math.sin(3*i)), top) for i in range(sides)]
    return {"id": rid, "stage": "Full", "role": "Island", "outline": outline, "top_abs": True,
            "bottom_abs": top-depth, "bottom_scale": 0.3, "cut": False, "island_top": top}


# 대전당 동쪽 입구: 골짜기 층계참(평탄, -24m) 동쪽 끝 안쪽(입구 양 모서리가 평탄면 위)에서 북동쪽 골짜기 사면 안으로 수평 6m 들어간 뒤, 반경 12m 원호로
# 북쪽→서쪽으로 꺾이며 약 12°로 내려가고(잔도 아래를 지나지 않는다),
# 마지막 4.5m는 홀 바닥 높이로 대전당 동쪽 벽에 수직으로 들어간다(V011).
# V010까지는 경사 25°인 잔도 옆에 문을 냈다. 문 양끝 높이가 3.7m 달라 바닥이 비틀리고, 내리막 쪽 안쪽 지형이 문 바닥보다 낮아
# 옆문과 경사 터널의 이음매에 발밑 턱이, 비스듬한 터널 끝면에는 홀 벽 턱이 남았다(사용자 WhereAmI 3곳).
_HALL_MOUTH = (8250, 350)
HALL_EAST_PATH = [_HALL_MOUTH, (9400, 1600), (9100, 3200), (7400, 4500), (5300, 5700), (4400, 5700), (4200, 5700)]
# 산괴 동굴 바닥은 협곡 바닥 높이(-8.4m)에 맞춰 협곡 입구를 평탄하게 둔다.
CAVERN_FLOOR = ground(-9000, -6950)-10

# 지하 전투 공간 2곳. Pod를 하나씩 두는 크기이고 입구가 여럿이다.
CAVES = [
    # 대분지 지하 대전당: 북쪽 대지 밑 47×38m, 바닥 -40m·천장 -12m(28m, 비행형 출현 가능), 지붕 22m 이상.
    # 동쪽 벽은 경사 터널 기울기(약 22°)를 지키려고 그대로 두고 서·북·남으로 넓혔다(V007 38×32m).
    # 입구: 대분지 바닥에서 걸어 들어가는 남쪽 문(폭 10m), 골짜기 층계참에서 내려가는 동쪽 통로(약 12°), 대지 위 싱크홀(낙하·훅).
    terrain_cave("BasinHall",
                 [(1700, 1600), (3300, 1600), (4300, 2900), (8000, 1300), (8100, 150), (9300, 150), (9900, 1500), (9700, 3700), (5700, 7000), (4900, 8300),
                  (-600, 8300), (-600, 3900), (1700, 3500)],
                 -4500,
                 [chamber([(-300, 4200), (4400, 4200), (4400, 8000), (-300, 8000)], -4000, -1200),
                  tunnel((2500, 1800), (2500, 4250), 500, -4010, -4000, 1000),
                  loft_corridor(HALL_EAST_PATH, 400, 900, ground(*_HALL_MOUTH)-5, -4000, 600, 450, mouth=False),
                  pit([(3400, 6300), (4300, 6300), (4300, 7500), (3400, 7500)], -1250, 3500)]),
    # 산괴 지하 동굴: 서쪽 산괴(1·2단) 밑 34×35m, 바닥 -8.4m(협곡 바닥)·천장 +5m(13.5m), 지붕 6m 이상. 지상 적 전용 편성.
    # 남쪽(협곡 쪽)은 지붕이 얇아져 그대로 두고 서·북·동으로 넓혔다(V007 27×31m).
    # 입구: 서쪽 저지에서 들어가는 터널(계단 발치 남쪽), 협곡 바닥에서 올라오는 터널.
    terrain_cave("MassifCavern",
                 [(-13300, -5300), (-11400, -5600), (-10300, -6000), (-9700, -7150), (-8300, -7150), (-7900, -6100),
                  (-7300, -5800), (-7200, -1700), (-11400, -1700), (-13300, -3900)],
                 -1100,
                 [chamber([(-11000, -5500), (-7600, -5500), (-7600, -2000), (-11000, -2000)], CAVERN_FLOOR, CAVERN_FLOOR+1350),
                  tunnel((-13000, -4600), (-10950, -4600), 400, ground(-13000, -4600)-10, CAVERN_FLOOR, 750),
                  tunnel((-9000, -6950), (-9000, -5450), 400, CAVERN_FLOOR, CAVERN_FLOOR, 750)]),
]
# 부유섬 2개: 지름 약 58m 전투 섬(Pod 하나씩, V009에서 50m→58m). V007의 36m·+24/+30m는 좁고 낮아 언덕처럼 보였다.
# 골짜기 섬은 골짜기 바닥에서 약 80m, 정상보다 21m 낮아 정상에서 활공으로 건너간다. 서쪽 섬은 3단 높이로 산괴를 마주한다.
ISLANDS = [
    island("ValleySkyIsle", (7800, -5600), 2900, 5500, 2300),
    island("WestSkyIsle", (-14600, 4800), 2900, 6000, 2300, 16, 0.8),
]


def boulder(rid, center, size, top):
    """부유섬 위 엄폐 암반: 섬 윗면에 1m 묻힌 낮은 각기둥."""
    cx, cy = center
    outline = [(cx-size, cy-size*0.6, top+300), (cx+size*0.8, cy-size, top+350), (cx+size*1.1, cy+size*0.5, top+280),
               (cx-size*0.4, cy+size, top+320)]
    return {"id": rid, "stage": "Full", "role": "Cover", "outline": outline, "top_abs": True, "bottom_abs": top-100, "cut": False}


ISLAND_COVER = [
    boulder("ValleyIsleCoverA", (6700, -4900), 300, 5500), boulder("ValleyIsleCoverB", (8900, -6500), 350, 5500),
    boulder("ValleyIsleCoverC", (7200, -7000), 280, 5500),
    boulder("WestIsleCoverA", (-15700, 4100), 300, 6000), boulder("WestIsleCoverB", (-13500, 5700), 350, 6000),
    boulder("WestIsleCoverC", (-14200, 3300), 280, 6000),
]


def outcrop(rid, center, size, height, phase=0.0, sides=5):
    """외곽 작은 바위: 불규칙 볼록 다각형, 정점마다 지면 위 높이가 다르다."""
    cx, cy = center
    outline = []
    for i in range(sides):
        a = phase+i*math.tau/sides
        r = size*(0.8+0.25*math.sin(2.3*i+phase))
        outline.append((cx+r*math.cos(a), cy+r*math.sin(a), height*(0.6+0.4*math.cos(1.7*i+phase))))
    return {"id": rid, "stage": "Full", "role": "Outcrop", "outline": outline}


OUTER_ROCKS = [
    outcrop("WestOutcropN", (-16500, 9400), 450, 450, 0.4),
    outcrop("NorthwestOutcrop", (-15800, 13600), 550, 600, 1.1),
    outcrop("NorthOutcropA", (-12300, 18500), 400, 350, 2.0), outcrop("NorthOutcropB", (-11000, 18800), 300, 250, 0.7, 4),
    outcrop("SouthwestOutcrop", (-15800, -15100), 500, 500, 1.6),
    outcrop("SouthOutcropA", (-9600, -16500), 450, 400, 0.9), outcrop("SouthOutcropB", (-2600, -17900), 550, 550, 2.4),
    outcrop("MesaFootOutcrop", (2400, -11600), 400, 350, 0.2),
    outcrop("SoutheastOutcropA", (13600, -7400), 500, 500, 1.3), outcrop("SoutheastOutcropB", (18600, -6700), 400, 400, 0.5, 4),
    outcrop("NortheastOutcropA", (10800, 12200), 450, 450, 2.2), outcrop("EastOutcrop", (20700, 5200), 350, 300, 1.8, 4),
]
ROCKS += CAVES+ISLANDS+ISLAND_COVER+OUTER_ROCKS


def cave_parts(rock):
    """지하 공간 절단 형상 [(종류, 바닥 다각형 [(x, y, h)], 꼭짓점별 윗면 h)]. 빌더 Boolean과 바닥 판정이 같이 쓴다."""
    return rock["cave"]["parts"]


def loft_floor(rings, x, y):
    """쓸어 만든 통로 바닥 높이: 이웃 고리 사이 사각형 안이면 길이·폭 방향으로 보간한다."""
    for r0, r1 in zip(rings, rings[1:]):
        quad = [r0[0][:2], r1[0][:2], r1[1][:2], r0[1][:2]]
        if not (inside(x, y, quad) or signed_distance(x, y, quad) > -1):
            continue
        (l0x, l0y, l0), (r0x, r0y, rr0) = r0[0], r0[1]
        (l1x, l1y, l1), (r1x, r1y, rr1) = r1[0], r1[1]
        mx0, my0, mx1, my1 = (l0x+r0x)/2, (l0y+r0y)/2, (l1x+r1x)/2, (l1y+r1y)/2
        t = max(0.0, min(1.0, segment(x, y, mx0, my0, mx1, my1)[1]))
        lx, ly = l0x+(l1x-l0x)*t, l0y+(l1y-l0y)*t
        rx, ry = r0x+(r1x-r0x)*t, r0y+(r1y-r0y)*t
        w = max(0.0, min(1.0, segment(x, y, lx, ly, rx, ry)[1]))
        hl, hr = l0+(l1-l0)*t, rr0+(rr1-rr0)*t
        return hl+(hr-hl)*w
    return None


def cave_floor(x, y):
    """지하 공간 걷는 바닥 높이(방·통로, 경계 포함). 밖이면 None. 겹치면 가장 낮은 바닥(Boolean 결과)이다."""
    best = None
    for rock in active(CAVES):
        for kind, poly, _ in cave_parts(rock):
            if kind == "pit":
                continue
            if kind == "loft":
                h = loft_floor(poly, x, y)
                if h is not None:
                    best = h if best is None else min(best, h)
                continue
            flat = [(px, py) for px, py, _ in poly]
            if not (inside(x, y, flat) or signed_distance(x, y, flat) > -1):
                continue
            if kind == "tunnel":
                (ax, ay, h0), (bx, by, h1), (cx, cy, _), (dx, dy, _) = poly
                t = max(0.0, min(1.0, segment(x, y, (ax+dx)/2, (ay+dy)/2, (bx+cx)/2, (by+cy)/2)[1]))
                h = h0+(h1-h0)*t
            else:
                h = poly[0][2]
            best = h if best is None else min(best, h)
    return best


def rock_top(rock, x, y, top):
    """암반 외곽 정점의 윗면 높이(지면 위 높이 또는 절대 높이)."""
    return top if rock.get("top_abs") else ground(x, y)+top


def layer_height(x, y, layer="surface"):
    """경로·검토점 층: surface=지면, deck=석교·부유섬 윗면 포함, cave=지하 공간 바닥 우선."""
    if layer == "deck":
        return walk_height(x, y)
    if layer == "cave":
        h = cave_floor(x, y)
        return ground(x, y) if h is None else h
    return ground(x, y)


def walk_height(x, y):
    """걷는 면 높이: 지면, 석교 상판·부유섬 윗면 위라면 그 윗면."""
    h = ground(x, y)
    for rock in active(ROCKS):
        if "island_top" in rock and inside(x, y, rock_outline(rock)):
            h = max(h, rock["island_top"])
        if "deck" in rock:
            (ax, ay), (bx, by), ha, hb, half = rock["deck"]
            d, t = segment(x, y, ax, ay, bx, by)
            if d <= half:
                h = max(h, ha+(hb-ha)*t)
    return h


def feature_points(spacing=200):
    """지각 삼각분할에 넣을 형상선 표본: 경사로·협곡의 가장자리와 어깨, 단 가장자리(절벽 위·아래), 분지 테.
    4m 격자만으로는 6m 폭 경사로와 6m 폭 절벽 띠가 기울어진 면으로 뭉개진다."""
    points = []

    def line(ax, ay, bx, by, offsets):
        """offsets는 숫자 목록 또는 t(0~1)를 받아 목록을 돌려주는 함수(폭이 변하는 경로)."""
        n = math.hypot(bx-ax, by-ay)
        if n < 1:
            return
        nx, ny = -(by-ay)/n, (bx-ax)/n
        for k in range(int(n/spacing)+1):
            t = k*spacing/n
            for o in (offsets(t) if callable(offsets) else offsets):
                points.append((ax+(bx-ax)*t+nx*o, ay+(by-ay)*t+ny*o))
    for path in active(PATHS):
        sh = path["shoulder"]
        for (ax, ay, _, aw), (bx, by, _, bw) in zip(path_points(path), path_points(path)[1:]):
            def offsets(t, aw=aw, bw=bw, sh=sh):
                w = aw+(bw-aw)*t
                return [-w-sh, -w-sh*0.75, -w-sh/2, -w-sh/4, -w, -w/2, 0, w/2, w, w+sh/4, w+sh/2, w+sh*0.75, w+sh]
            line(ax, ay, bx, by, offsets)
    path_count = len(points)
    for plate in active(PLATES):
        poly = plate["polygon"]
        area = sum(a[0]*b[1]-b[0]*a[1] for a, b in zip(poly, poly[1:]+poly[:1]))
        sign = -1 if area > 0 else 1  # 반시계면 왼쪽 법선이 안쪽 → 바깥은 음수 쪽.
        e = plate["edge"]
        start = len(points)
        normals = []
        for a, b in zip(poly, poly[1:]+poly[:1]):
            before = len(points)
            line(a[0], a[1], b[0], b[1], [sign*o for o in (-100, 0, e*0.25, e*0.5, e*0.75, e, e+150)])
            n = math.hypot(b[0]-a[0], b[1]-a[1])
            normals += [(sign*-(b[1]-a[1])/n, sign*(b[0]-a[0])/n)]*(len(points)-before)
        for i, (nx, ny) in enumerate(normals):
            x, y = points[start+i]
            j = edge_jitter(x, y)
            points[start+i] = (x+nx*j, y+ny*j)
    for basin in active(BASINS+PADS):
        bx, by = basin["xy"]
        for r in (basin["radius"], basin["radius"]+basin["falloff"]/2, basin["radius"]+basin["falloff"]):
            n = int(math.tau*r/spacing)
            points += [(bx+r*math.cos(i*math.tau/n), by+r*math.sin(i*math.tau/n)) for i in range(n)]
    # 경로 통로 안의 단 가장자리·분지 테 표본은 버린다. 경로 높이가 그 자리를 소유하고, 섞이면 경로 면에 가는 삼각형이 생긴다.
    kept = points[:path_count]
    for x, y in points[path_count:]:
        if not any(path_profile(path, x, y)[0] < path_profile(path, x, y)[2]+path["shoulder"]+100 for path in active(PATHS)):
            kept.append((x, y))
    return kept


def route_layer(route):
    return "deck" if route.get("deck") else route.get("layer", "surface")


def ground_point(xy, lift=0.0):
    return (xy[0], xy[1], ground(*xy)+lift)


def placement():
    nodes = [{"id": n["id"], "local": [*n["xy"], layer_height(*n["xy"], n.get("layer", "surface"))]} for n in active(NODES)]
    pods = [{"id": p["id"], "condition": p["condition"], "layer": p.get("layer", "surface"), "flyers": pod_has_flyers(p),
             "local": [*p["xy"], layer_height(*p["xy"], p.get("layer", "surface"))], "enemy_formations": pod_formations(p)}
            for p in active(PODS)]
    anchors = []
    for a in active(ANCHORS):
        lx, ly = a["landing"]
        anchors.append({"id": a["id"], "role": a["role"], "layer": a.get("layer", "surface"),
                        "local": [lx, ly, layer_height(lx, ly, a.get("layer", "surface"))+a["above"]],
                        "landing": list(a["landing"]), "from": a["from"], "max_distance": a["max_distance"]})
    routes = [{"id": r["id"], "kind": r["kind"], "layer": route_layer(r), "xy": [list(p) for p in r["xy"]]} for r in active(ROUTES)]
    return {"nodes": nodes, "pods": pods, "anchors": anchors, "routes": routes}
