"""Electric Dreams 암괴 배치를 먼저 정한 Z1 비교본의 기반면과 배치 원본."""
import json
import math
from pathlib import Path

import RidgeEDBaseSpec as base

VERSION = "V012_Z1Composition"
WEST_COVER_VERSION = "V012_Z1WestCover"
ridge = base.ridge


def ground(x, y):
    original = base.base_ground(x, y)
    # Z1 바깥 V012로 돌아가는 폭 14m의 전이 구간이다.
    mask = ridge.smooth(min(x + 5300, 8200 - x, y + 6200, 4700 - y) / 1400)
    if not mask:
        return original
    for cut in base.CUTS:
        # 기존 동굴 몸체와 만나는 경계 높이를 공유한다.
        mask *= ridge.smooth((-ridge.signed_distance(x, y, cut) - 100) / 650)
    if not mask:
        return original

    # 서쪽 암벽 앞을 낮추고, 벽 뒤쪽으로 들어가며 기존 산괴 높이에 연결한다.
    wall = -2000 + 340 * math.sin((y + 700) / 1550)
    west = ridge.smooth((wall - x) / 2000)
    # 하류 입구에서 분지 동쪽까지 이어지는 하상과 바깥쪽 둑이다.
    channel_y = -3250 + 3200 * ridge.smooth((x + 200) / 5600)
    channel = 1 - ridge.smooth(abs(y - channel_y) / 875)
    south = ridge.smooth((channel_y - y - 1050) / 2200)
    east = ridge.smooth((x - 5900) / 1900)
    bed = -3950 - 105 * channel + 730 * south + 850 * east
    shaped = bed + west * (5200 - bed)
    return original + mask * (shaped - original)


MEGASCANS = "/Game/Megascans/3D_Assets/"
SMART_MESHES = "/Game/SmartAssets/AssetMeshes/"

# H는 현지 반지름 기준 절대 높이다. 큰 절벽의 이음은 몇 m 겹쳐 드러나는 부위를 숨긴다.
ASSEMBLY = (
    ("WestUpperShelf", "MassiveSandstoneCliff_05", (-2130, -770, -550), -93, (-6, -75), (.26, .24, .26)),
    ("WestWallSouth", "Cliff_06", (-2410, -1330, -4210), -93, (2, -8), (1.20, 1.05, 1.52)),
    ("WestCrown", "MassiveSandstoneCliff_01", (-2300, -405, -620), -102, (-3, 6), (1.16, .92, 1.17)),
    ("NorthBankShelf", "MassiveSandstoneCliff_03", (2090, -580, -4050), -27, (-2, -74), (.32, .28, .33)),
    ("NorthCrown", "HugeSandstoneCliff_05", (-2790, 1520, -1580), -105, (-4, 5), (.78, .86, 1.22)),
    ("OutletWestBank", "HugeSandstoneCliff_02", (-420, -3950, -4220), -37, (6, -8), (1.10, .91, 1.08)),
    ("OutletEastBank", "HugeSandstoneCliff_07", (2210, -4030, -4130), 35, (-4, 9), (.85, .81, 1.24)),
    ("WestWallNorth", "Cliff_06", (-2050, 1270, -4190), -101, (-3, -8), (1.15, 1.02, 1.40)),
    ("PillarShoulder", "Cliff_02", (3440, -1570, -3500), 19, (8, -7), (1.42, 1.25, 1.26)),
    ("PillarApron", "CanyonSandstoneRidge_03", (3100, -2300, -4040), 31, (5, -7), (.77, .73, 1.16)),
    ("NorthLedge", "MassiveCanyonSandstoneMesa_01", (1240, 2090, -3960), -18, (-4, 8), (.80, 1.05, 1.10)),
    ("NorthCaveFace", "Cliff_06", (2910, 2350, -4260), 174, (2, 6), (.97, .90, 1.17)),
    ("OutletShelf", "Cliff_05", (2070, -3110, -4070), 53, (3, -9), (.92, 1.04, 1.38)),
    ("WestTalus", "ForestRockFormation_06", (190, -1580, -4060), -53, (4, -4), (1.58, 1.25, 2.0)),
    ("NorthTalus", "NordicBoulder_02", (740, 1370, -4100), 70, (-4, 5), (.92, 1.03, .95)),
)


# Y, 기반면이 높아지기 시작하는 X, 왕관 뒤에서 만나는 X와 H.
# 절벽 외피의 하단과 상단 단면을 지나지 않도록 서쪽으로 물린다.
WEST_BACKING = (
    (-3800, -2230, -4000, 120),
    (-3100, -2020, -3590, 1010),
    (-2400, -1840, -3310, 1140),
    (-1700, -1790, -3010, 1280),
    (-1050, -2200, -3020, 1540),
    (-520, -2200, -3030, 1970),
    (220, -1990, -3090, 1720),
    (960, -1880, -3230, 1610),
    (1630, -1900, -3330, 1770),
    (2250, -1900, -3370, 1510),
    (2970, -2150, -3370, 670),
    (3600, -2400, -3220, -510),
)

WEST_COVER = (
    ("WestWallSouthReturn", "MassiveSandstoneCliff_01", (-2300, -3720, -4250), -137, (-3, -6), (.86, .88, 1.15)),
    ("WestWallSouth", "Cliff_06", (-1600, -2580, -4375), -113, (3, -6), (1.28, 1.20, 1.63)),
    ("WestWallCenter", "Cliff_06", (-1595, -720, -4365), -92, (-4, -4), (1.20, .98, 1.58)),
    ("WestWallNorth", "Cliff_06", (-1540, 1370, -4350), -83, (-4, -8), (1.29, 1.10, 1.59)),
    ("WestWallNorthReturn", "HugeSandstoneCliff_05", (-1690, 3010, -4250), -64, (-8, 4), (.82, .91, 2.80)),
    ("WestCrown", "MassiveSandstoneCliff_01", (-2320, -570, -1340), -102, (-3, 6), (1.19, 1.12, 1.29)),
    ("WestCrownSouth", "MassiveSandstoneCliff_05", (-2720, -2840, -1490), -118, (-3, 6), (.66, .87, .94)),
    ("NorthCrown", "MassiveSandstoneCliff_05", (-2300, 1660, -1500), -75, (-5, 4), (.72, .83, 1.03)),
)


def west_cover_ground(x, y):
    original = ground(x, y)
    along = ridge.smooth((y + 4900) / 1550) * ridge.smooth((4480 - y) / 1450)
    across = ridge.smooth((x + 8000) / 4100) * ridge.smooth((-900 - x) / 350)
    if not along or not across:
        return original
    for cut in base.CUTS:
        across *= ridge.smooth((-ridge.signed_distance(x, y, cut) - 100) / 650)
    if not across:
        return original

    # 각 단면은 벽 앞 분지, 암벽 뒤 사면, 왕관 높이 순서로 이어진다.
    clamped_y = max(WEST_BACKING[0][0], min(y, WEST_BACKING[-1][0]))
    for first, second in zip(WEST_BACKING, WEST_BACKING[1:]):
        if clamped_y <= second[0]:
            t = (clamped_y - first[0]) / (second[0] - first[0])
            toe, crest, top = (a + (b - a) * t for a, b in zip(first[1:], second[1:]))
            break
    # 현재 하상에 연결하고 암벽 뒤쪽까지 올라갈 수 있는 연속 기반면을 남긴다.
    floor = ground(-1250, y)
    backing = floor + (top - floor) * ridge.smooth((toe - x) / (toe - crest))
    return original + along * across * (backing - original)


# V001은 고원을 x -3,000에서 시작해 02 벽 바로 뒤 봉우리(약 +57m)가 남았다. V002는 벽 안쪽 x -2,200부터 깎는다.
MASSIF_COVER_VERSIONS = {"V001": ("V012_Z1MassifCover", -3000, 700), "V002": ("V012_Z1MassifCover_V002", -2200, 500)}
MASSIF_COVER = "V002"
MASSIF_COVER_VERSION = MASSIF_COVER_VERSIONS[MASSIF_COVER][0]
# ED CloseRange 산괴(구운 LargeAssembly)를 앉힐 고원. 높이는 분지 바닥 기준이며 02 절벽 벽 상단(약 +48m)보다 조금 낮춘다.
MASSIF_PLATEAU_CM = 4600
MASSIF_CENTER = (-6500, 300)


def massif_cover_weight(x, y, version=None):
    # 서쪽 덮개 기반면을 고원으로 바꾸는 가중치(1이면 고원). 동굴 절개 둘레는 0으로 돌아간다.
    _, start, ramp = MASSIF_COVER_VERSIONS[version or MASSIF_COVER]
    # 02 벽 안쪽에서 고원으로 들어가고, x −10,000 서쪽에서 V012 사면으로 돌아간다. 남북 범위는 서쪽 덮개와 같다.
    along = ridge.smooth((y + 4900) / 1550) * ridge.smooth((4480 - y) / 1450)
    across = ridge.smooth((start - x) / ramp) * ridge.smooth((x + 11500) / 1500)
    if not along or not across:
        return 0.0
    for cut in base.CUTS:
        across *= ridge.smooth((-ridge.signed_distance(x, y, cut) - 100) / 650)
    return along * across


def massif_cover_ground(x, y, version=None):
    original = west_cover_ground(x, y)
    weight = massif_cover_weight(x, y, version)
    if not weight:
        return original
    plateau = ground(-1250, y) + MASSIF_PLATEAU_CM
    return original + weight * (plateau - original)


# CloseRange 하상 자리. ED CloseRange의 하상 바닥은 원점 높이(-6~+10cm)라 드레이프하면 고원 지각과 같은 높이에 겹쳐
# 지각이 하상 메시를 덮는다. V003은 V002 고원과 같고, 렌더링용 지각 메시에서만 하상 띠를 얕게 판다.
# 드레이프 배치 높이는 계속 V002(파지 않은 고원)를 쓴다.
MASSIF_COVER_VERSIONS["V003"] = ("V012_Z1MassifCover_V003", -2200, 500)
MASSIF_CHANNEL_VERSION = MASSIF_COVER_VERSIONS["V003"][0]
# ED Ditch 스플라인 단면 트레이스(200cm 간격, 좌우 ±14m)에서 하상(원점 +40cm 미만) 띠는 폭 5~7m이고
# 스플라인 왼쪽 법선으로 약 4.5m 치우친다. 아래 점열은 그 띠 중심을 4m 간격으로 뽑은 닫힌 고리(CR 원점 기준 cm)다.
CLOSERANGE_CHANNEL_CR = ((-1720, -4958), (-1462, -4844), (-1190, -4652), (-917, -4410), (-661, -4148), (-468, -3903), (-318, -3582), (-123, -3208), (67, -2854), (258, -2500), (451, -2145), (666, -1770), (927, -1418), (1184, -1099), (1443, -784), (1456, -643), (1488, -274), (1505, 117), (1528, 566), (1724, 1052), (1854, 1374), (1902, 1624), (1886, 1682), (1684, 1817), (1378, 1941), (1036, 2032), (692, 2086), (384, 2085), (-24, 2049), (-415, 2012), (-790, 1965), (-1121, 1896), (-1241, 1843), (-1359, 1583), (-1812, 1094), (-2247, 901), (-2661, 772), (-3061, 668), (-3516, 589), (-3927, 556), (-4269, 516), (-4416, 457), (-4546, 271), (-4627, 230), (-4695, 489), (-4326, 509), (-3897, 545), (-3400, 508), (-2953, 392), (-2509, 204), (-2075, -164), (-1800, -593), (-1647, -905), (-1456, -1150), (-1185, -1429), (-891, -1751), (-609, -2186), (-460, -2678), (-432, -3176), (-516, -3676), (-767, -4173), (-1200, -4560), (-1650, -4746), (-2022, -4866), (-2198, -4944), (-2223, -5013), (-2308, -4900), (-2043, -4966), (-1777, -4969))
# (띠 반폭, 가장자리 폭, 깊이) cm. 하상 바닥 메시가 지각 위로 드러나도록 50cm 판다. 둑 아래는 둑 메시가 가린다.
MASSIF_CHANNEL_SHAPE = (450, 200, 50)


def closerange_to_local(cx, cy):
    # 드레이프와 같은 평면 배치: 원점은 MASSIF_CENTER의 고원 높이, X는 옥탄트 로컬 +x 접선, Y = Up × X.
    x0, y0 = MASSIF_CENTER
    origin = ridge.point(x0, y0, ground(-1250, y0) + MASSIF_PLATEAU_CM)
    norm = math.sqrt(sum(v * v for v in origin))
    up = [-v / norm for v in origin]
    a, b = ridge.point(x0, y0, 0), ridge.point(x0 + 10, y0, 0)
    d = [q - p for p, q in zip(a, b)]
    dot = sum(u * v for u, v in zip(up, d))
    ax = [v - u * dot for u, v in zip(up, d)]
    n = math.sqrt(sum(v * v for v in ax))
    ax = [v / n for v in ax]
    ay = [up[1] * ax[2] - up[2] * ax[1], up[2] * ax[0] - up[0] * ax[2], up[0] * ax[1] - up[1] * ax[0]]
    world = [o + ax[i] * cx + ay[i] * cy for i, o in enumerate(origin)]
    return ridge.local(world)[:2]


MASSIF_CHANNEL = tuple(closerange_to_local(cx, cy) for cx, cy in CLOSERANGE_CHANNEL_CR)
_CHANNEL_BOX = (min(p[0] for p in MASSIF_CHANNEL), max(p[0] for p in MASSIF_CHANNEL),
                min(p[1] for p in MASSIF_CHANNEL), max(p[1] for p in MASSIF_CHANNEL))


def massif_channel_distance(x, y):
    reach = MASSIF_CHANNEL_SHAPE[0] + MASSIF_CHANNEL_SHAPE[1]
    if not (_CHANNEL_BOX[0] - reach <= x <= _CHANNEL_BOX[1] + reach and _CHANNEL_BOX[2] - reach <= y <= _CHANNEL_BOX[3] + reach):
        return float("inf")
    best = float("inf")
    for (ax, ay), (bx, by) in zip(MASSIF_CHANNEL, MASSIF_CHANNEL[1:] + MASSIF_CHANNEL[:1]):
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy or 1.0)))
        best = min(best, math.hypot(x - ax - t * dx, y - ay - t * dy))
    return best


def massif_channel_carve(x, y):
    # 하상 띠 안에서 파는 깊이(cm, 양수). 고원 가중치는 곱하지 않는다.
    half, edge, depth = MASSIF_CHANNEL_SHAPE
    distance = massif_channel_distance(x, y)
    if distance >= half + edge:
        return 0.0
    return depth * (1 - ridge.smooth((distance - half) / edge))


def massif_plateau_mask(x, y):
    # 고원 가중치 0.8 아래(남북 오름 사면·동굴 절개 둘레)에서는 섬 지형을 반영하지 않는다.
    return ridge.smooth((massif_cover_weight(x, y, "V002") - 0.8) / 0.2)


def massif_channel_ground(x, y):
    # V002 고원에서 하상 띠를 판다.
    height = massif_cover_ground(x, y, "V002")
    carve = massif_channel_carve(x, y)
    return height - carve * massif_plateau_mask(x, y) if carve else height


# V003 Lit 결과: ED CloseRange는 떠 있는 섬이고 하상 바깥 둑 고리 전체가 원점 +3m 단이다. 그 위 ForestGround 타일이
# 고원 지각 3m 위에 떠 있어 바깥 테두리가 허공으로 드러나 그림자를 드리웠다. V004는 ED 섬 지지면 높이장
# (CloseRangeSupport_V001.json, BuildCloseRangeSupport.py)을 따라 지각을 지지면 50cm 아래로 올리고, 바깥은 45°로 내린다.
# 하상 띠는 V003처럼 50cm 아래를 지킨다. 드레이프 배치 높이는 계속 V002다.
MASSIF_COVER_VERSIONS["V004"] = ("V012_Z1MassifCover_V004", -2200, 500)
MASSIF_ISLAND_VERSION = MASSIF_COVER_VERSIONS["V004"][0]
# V004 결과: 바깥 테두리 둥근 타일 가장자리가 바로 밖에서 내려가는 지각 위로 처마처럼 튀어나와 버섯 갓 그림자가 졌다.
# V005는 지지면 높이장 V002(50cm 격자, 타일 발자국 20% 축소, 섬 바깥을 지지면 +10cm로 1m 넓힘)를 쓴다.
MASSIF_COVER_VERSIONS["V005"] = ("V012_Z1MassifCover_V005", -2200, 500)
MASSIF_ISLAND_V005_VERSION = MASSIF_COVER_VERSIONS["V005"][0]
_SUPPORTS = {version: json.loads((Path(__file__).resolve().parent / ("Profiling/Graphics/CloseRangeSupport_%s.json"
                                                                     % version)).read_text(encoding="utf-8"))
             for version in ("V001", "V002")}


def local_to_closerange(x, y):
    # closerange_to_local의 역변환. 옥탄트 로컬 점의 방사선과 드레이프 평면의 교점을 CR 축에 투영한다.
    x0, y0 = MASSIF_CENTER
    origin = ridge.point(x0, y0, ground(-1250, y0) + MASSIF_PLATEAU_CM)
    norm = math.sqrt(sum(v * v for v in origin))
    up = [-v / norm for v in origin]
    a, b = ridge.point(x0, y0, 0), ridge.point(x0 + 10, y0, 0)
    d = [q - p for p, q in zip(a, b)]
    dot = sum(u * v for u, v in zip(up, d))
    ax = [v - u * dot for u, v in zip(up, d)]
    n = math.sqrt(sum(v * v for v in ax))
    ax = [v / n for v in ax]
    ay = [up[1] * ax[2] - up[2] * ax[1], up[2] * ax[0] - up[0] * ax[2], up[0] * ax[1] - up[1] * ax[0]]
    ray = ridge.point(x, y, 0)
    t = sum(o * u for o, u in zip(origin, up)) / sum(r * u for r, u in zip(ray, up))
    rel = [r * t - o for r, o in zip(ray, origin)]
    return sum(r * v for r, v in zip(rel, ax)), sum(r * v for r, v in zip(rel, ay))


def closerange_support_offset(cx, cy, support="V001"):
    # 지지면 높이장(CR 원점 기준)을 쌍선형 보간한다. 격자 밖은 0이다.
    field = _SUPPORTS[support]
    rows, step = field["rows"], field["step"]
    fx, fy = (cx - field["x0"]) / step, (cy - field["y0"]) / step
    i, j = math.floor(fx), math.floor(fy)
    if not (0 <= i < len(rows[0]) - 1 and 0 <= j < len(rows) - 1):
        return 0.0
    tx, ty = fx - i, fy - j
    top = rows[j][i] * (1 - tx) + rows[j][i + 1] * tx
    bottom = rows[j + 1][i] * (1 - tx) + rows[j + 1][i + 1] * tx
    return top * (1 - ty) + bottom * ty


def massif_island_ground(x, y, support="V001"):
    # V002 고원에 ED 섬 지지면 오프셋을 더하고, 하상 띠에서는 50cm 아래를 넘지 않게 한다.
    height = massif_cover_ground(x, y, "V002")
    mask = massif_plateau_mask(x, y)
    if not mask:
        return height
    offset = closerange_support_offset(*local_to_closerange(x, y), support)
    carve = massif_channel_carve(x, y)
    if carve:
        offset = min(offset, -carve)
    return height + mask * offset


def massif_island_feature_points(support="V001"):
    # 하상 띠 특징점에 더해, 오프셋이 이웃과 20cm 넘게 다른 지지면 칸(둑·테두리 경사)을 높이장 격자 간격으로 넣는다.
    yield from massif_channel_feature_points()
    field = _SUPPORTS[support]
    rows, step = field["rows"], field["step"]
    for j in range(1, len(rows) - 1):
        for i in range(1, len(rows[0]) - 1):
            v = rows[j][i]
            if max(abs(v - rows[j + dj][i + di]) for dj, di in ((0, 1), (0, -1), (1, 0), (-1, 0))) <= 20:
                continue
            x, y = closerange_to_local(field["x0"] + i * step, field["y0"] + j * step)
            if massif_cover_weight(x, y, "V002") > 0.8:
                yield x, y


def massif_channel_feature_points():
    # 4m 격자로는 폭 5~7m의 골이 뭉개지므로 하상 띠 둘레에 1m 간격 표본을 더한다.
    yield from base.feature_points()
    half, edge, _ = MASSIF_CHANNEL_SHAPE
    for (ax, ay), (bx, by) in zip(MASSIF_CHANNEL, MASSIF_CHANNEL[1:] + MASSIF_CHANNEL[:1]):
        length = math.hypot(bx - ax, by - ay)
        nx, ny = -(by - ay) / length, (bx - ax) / length
        for i in range(max(1, round(length / 100))):
            t = i * 100 / length
            for side in (-half - edge, -half, -half / 2, 0, half / 2, half, half + edge):
                x, y = ax + (bx - ax) * t + nx * side, ay + (by - ay) * t + ny * side
                if massif_cover_weight(x, y, "V002") > 0.8:
                    yield x, y


def west_cover_rows():
    replaced = {row[0] for row in WEST_COVER} | {"WestUpperShelf", "PillarApron", "NorthLedge"}
    return tuple(row for row in ASSEMBLY if row[0] not in replaced) + WEST_COVER


def placements(rows=ASSEMBLY, ground_at=ground):
    for name, asset, (x, y, h), yaw, tilt, scale in rows:
        if asset.startswith("Cliff_"):
            mesh = SMART_MESHES + "SM_" + asset
        else:
            mesh = MEGASCANS + asset.rsplit("_", 1)[0] + "/SM_" + asset
        overrides = {"0": MEGASCANS + "HugeSandstoneCliff/MI_HugeSandstoneCliff_07"} if asset == "HugeSandstoneCliff_07" else {}
        yield {"id": "ED_Z1_" + name, "mesh": mesh, "local_xy_cm": [x, y], "height_offset_cm": h - ground_at(x, y),
               "yaw_deg": yaw, "tilt_deg": tilt, "scale": scale, "material_overrides": overrides}


def massif_island_v005_ground(x, y):
    return massif_island_ground(x, y, "V002")


def massif_island_v005_feature_points():
    return massif_island_feature_points("V002")


# V005 저시점·부감 결과: 고원 남북 끝에 V012 능선(+50~57m)이 각진 기둥으로 남아 산괴 양옆을 차지하고, 분지 남북 테두리
# (북 60m·남 30m)가 맨 지각이다. V006은 V005에서 두 가지만 바꾼 렌더링 지각이다(드레이프 배치 높이는 계속 V002).
# 1) 능선 상한: 고원 남북 끝 능선을 남쪽은 고원 높이, 북쪽은 북쪽 대지(+20m) 높이로 깎는다. 섬 지형(고원 가중치)과
#    동굴 절개 둘레는 건드리지 않는다. 깎은 가장자리와 테두리는 조립체(왕관·벽)가 덮는다.
# 2) 테두리 면 물리기: 남북 테두리 조립체 앞면 선(RIM_FACES) 앞의 사면을 분지 바닥으로 눌러, 지각 발치가 조립체
#    앞면보다 RIM_RECESS[0] 뒤에서 RIM_RECESS[1] 폭 안에 올라가게 한다(GX-022·서쪽 벽 WEST_BACKING과 같은 생각).
MASSIF_COVER_VERSIONS["V006"] = ("V012_Z1MassifCover_V006", -2200, 500)
MASSIF_RIM_VERSION = MASSIF_COVER_VERSIONS["V006"][0]
# (남쪽 상한, 북쪽 상한) 현지 H(cm). 남은 고원(분지 -3,950 + 4,600), 북은 북쪽 대지 높이다.
RIDGE_CAP_CM = (650, 2000)
# 앞면 선: 분지를 보는 쪽이 진행 방향 왼쪽이 되게 점을 잇는다(남 테두리는 동→서가 아니라 서→동, 앞면은 +y).
RIM_FACES = {
    "South": ((-600, -4650), (1500, -4550), (3500, -4500), (5500, -4500), (7000, -4300)),
    "North": ((1300, 2850), (0, 2850), (-1500, 2950)),
}
# (앞면 뒤 발치 거리, 사면 오름 폭, 앞쪽 영향 폭, 뒤쪽 영향 폭, 끝 페이드) cm
RIM_RECESS = (300, 400, 1500, 2500, 600)


def ridge_cap_weight(x, y):
    # 고원 남북 끝 능선 구역(x -7,000~-500, |y| 3,000~7,500). 고원 섬·동굴 절개 둘레는 0이다.
    along = ridge.smooth((x + 8500) / 1500) * ridge.smooth((-500 - x) / 700)
    across = ridge.smooth((abs(y) - 2500) / 500) * ridge.smooth((9000 - abs(y)) / 1500)
    if not along or not across:
        return 0.0
    for cut in base.CUTS:
        across *= ridge.smooth((-ridge.signed_distance(x, y, cut) - 100) / 650)
    return along * across * (1 - massif_plateau_mask(x, y))


def rim_recess(x, y, height, ground_at):
    # 앞면 선 뒤 발치까지는 분지 바닥(선 앞 1,000cm 지점 높이)으로 누르고, 그 뒤 오름 폭 안에서 원래 사면으로 돌아간다.
    toe, ramp, front, back, fade = RIM_RECESS
    for line in RIM_FACES.values():
        for (ax, ay), (bx, by) in zip(line, line[1:]):
            dx, dy = bx - ax, by - ay
            length = math.hypot(dx, dy)
            t = ((x - ax) * dx + (y - ay) * dy) / (length * length)
            if not 0 <= t <= 1:
                continue
            # 왼쪽 법선이 분지(앞)다. d는 앞면 뒤로 들어간 거리.
            nx, ny = -dy / length, dx / length
            d = -((x - ax) * nx + (y - ay) * ny)
            if not -front < d < back:
                continue
            fx, fy = ax + dx * t + nx * 1000, ay + dy * t + ny * 1000
            floor = ground_at(fx, fy)
            pressed = floor + (height - floor) * ridge.smooth((d - toe) / ramp)
            # 선 양 끝(첫 점·끝 점)에서는 원래 지각으로 돌아간다.
            first, last = line[0], line[-1]
            end = min(math.hypot(x - first[0], y - first[1]), math.hypot(x - last[0], y - last[1]))
            weight = ridge.smooth(end / fade) * ridge.smooth((d + front) / 500) * ridge.smooth((back - d) / 500)
            height = min(height, height - weight * (height - pressed))
    return height


def massif_rim_ground(x, y):
    height = massif_island_v005_ground(x, y)
    weight = ridge_cap_weight(x, y)
    if weight:
        cap = RIDGE_CAP_CM[0] + (RIDGE_CAP_CM[1] - RIDGE_CAP_CM[0]) * ridge.smooth(y / 1000)
        if height > cap:
            height -= weight * (height - cap)
    return rim_recess(x, y, height, massif_island_v005_ground)


# V006 띠 시험(Z1RimBand V001) 결과: ED CloseRange 둑 묶음(원본 경사 중앙값 38°)을 60~87° 테두리 면에 옮기면 판·돌무더기가
# 벽에 붙은 것처럼 보였다. ED 둑 조각은 완만한 둑을 덮는 도구다. V007은 남 테두리를 둑 경사 여러 단으로 바꾼 렌더링 지각이다.
# (앞면 선 이름, 첫 둑 발치 d, 둑 수평 폭, 단 너비, 둑 수, 끝 페이드) cm. d는 앞면 선 뒤로 들어간 거리(음수는 분지 쪽).
# 둑마다 (전체 높이 / 둑 수)씩 오르고, 맨 위는 원래 지각(남쪽 대지)으로 돌아간다.
RIM_TERRACES = {"South": (-1000, 1000, 450, 3, 600)}
MASSIF_COVER_VERSIONS["V007"] = ("V012_Z1MassifCover_V007", -2200, 500)
MASSIF_TERRACE_VERSION = MASSIF_COVER_VERSIONS["V007"][0]


def rim_terrace(x, y, height, ground_at):
    for name, (toe, run, tread, banks, fade) in RIM_TERRACES.items():
        line = RIM_FACES[name]
        top = toe + banks * run + (banks - 1) * tread
        for (ax, ay), (bx, by) in zip(line, line[1:]):
            dx, dy = bx - ax, by - ay
            length = math.hypot(dx, dy)
            t = ((x - ax) * dx + (y - ay) * dy) / (length * length)
            if not 0 <= t <= 1:
                continue
            nx, ny = -dy / length, dx / length
            d = -((x - ax) * nx + (y - ay) * ny)
            if not toe - 800 < d < top + 800:
                continue
            px, py = ax + dx * t, ay + dy * t
            floor = ground_at(px + nx * (1000 - toe), py + ny * (1000 - toe))
            crest = ground_at(px - nx * (top + 600), py - ny * (top + 600))
            rise = (crest - floor) / banks
            # 단마다 둑(run)으로 오르고 단(tread)에서 평평하다.
            level = floor
            for k in range(banks):
                start = toe + k * (run + tread)
                level = floor + rise * (k + ridge.smooth((d - start) / run))
                if d < start + run + tread:
                    break
            first, last = line[0], line[-1]
            end = min(math.hypot(x - first[0], y - first[1]), math.hypot(x - last[0], y - last[1]))
            weight = ridge.smooth(end / fade) * ridge.smooth((d - toe + 800) / 600) * ridge.smooth((top + 800 - d) / 600)
            height = height + weight * (level - height)
    return height


def massif_terrace_ground(x, y):
    return rim_terrace(x, y, massif_rim_ground(x, y), massif_rim_ground)


def massif_terrace_feature_points():
    # V006 특징점에 둑 위·아래 모서리 선을 1m 간격으로 더한다.
    yield from massif_rim_feature_points()
    for name, (toe, run, tread, banks, _) in RIM_TERRACES.items():
        line = RIM_FACES[name]
        edges = []
        for k in range(banks):
            start = toe + k * (run + tread)
            edges += [start - 100, start, start + run / 2, start + run, start + run + 100]
        for (ax, ay), (bx, by) in zip(line, line[1:]):
            dx, dy = bx - ax, by - ay
            length = math.hypot(dx, dy)
            nx, ny = -dy / length, dx / length
            for i in range(max(1, round(length / 100))):
                t = i * 100 / length
                for d in edges:
                    yield ax + dx * t - nx * d, ay + dy * t - ny * d


def massif_rim_feature_points():
    # 테두리 발치·오름 끝 선을 1m 간격으로 더해 물린 사면이 4m 격자에 뭉개지지 않게 한다.
    yield from massif_island_v005_feature_points()
    toe, ramp, *_ = RIM_RECESS
    for line in RIM_FACES.values():
        for (ax, ay), (bx, by) in zip(line, line[1:]):
            dx, dy = bx - ax, by - ay
            length = math.hypot(dx, dy)
            nx, ny = -dy / length, dx / length
            for i in range(max(1, round(length / 100))):
                t = i * 100 / length
                for d in (toe - 100, toe, toe + ramp / 2, toe + ramp, toe + ramp + 200):
                    yield ax + dx * t - nx * d, ay + dy * t - ny * d


# V029 뒤 북 테두리(분지 -39.5m → 북쪽 단 +20m): 앞면이 x -2,100~1,700(38m)뿐이고 뒤는 동굴 절개(x -600 동쪽, 홀 천장 -12m)와
# NorthLedge, 앞은 분지라 남쪽처럼 둑 5단(깊이 약 52m)을 넣을 수 없다. 사용자 결정(B안): 분지 쪽으로 둑 2단만 덧대고, 위쪽 급사면은
# 단독 조립체 하나가 덮는다. V008은 V007에 북 둑 2단을 더한 렌더링 지각이다(드레이프 배치 높이는 계속 V002).
# 선: 급사면이 둑 맨 위 높이(분지 + 2 x 1,190 ≈ -1,570)에 닿는 등고선. 진행 방향 왼쪽이 분지다(동→서). RIM_FACES와 따로 둔다
# (RIM_FACES를 바꾸면 V006·V007이 바뀐다).
RIM_BANK_LINES = {"NorthBank": ((1500, 3400), (-700, 3470), (-1300, 3780), (-2300, 3780))}
# (둑 수평 폭, 단 너비, 둑 수, 둑 높이, 끝 페이드, 둑 맨 위 뒤 페이드) cm. 둑은 선 앞(분지 쪽)으로 쌓고 지금 지각보다 낮은 곳만
# 올린다(급사면은 깎지 않는다). 동굴 절개 경계 2m 안은 건드리지 않는다.
RIM_FILL_BANKS = {"NorthBank": (900, 250, 2, 1190, 900, 600)}
MASSIF_COVER_VERSIONS["V008"] = ("V012_Z1MassifCover_V008", -2200, 500)
MASSIF_BANK_VERSION = MASSIF_COVER_VERSIONS["V008"][0]


def rim_line(name):
    # 띠(band)가 쓰는 선. 테두리 앞면 선과 덧댄 둑 선을 같은 이름 공간으로 찾는다.
    return RIM_FACES[name] if name in RIM_FACES else RIM_BANK_LINES[name]


def polyline_frame(x, y, line):
    # 가장 가까운 선분 기준 (뒤로 들어간 거리 d, 선 시작에서의 호 길이 s, 전체 길이, 그 선분의 분지 쪽 법선).
    # 왼쪽 법선이 분지(앞)이고, d는 앞면 뒤(분지 반대쪽)가 양수다.
    best, total = None, 0.0
    for (ax, ay), (bx, by) in zip(line, line[1:]):
        dx, dy = bx - ax, by - ay
        length = math.hypot(dx, dy)
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (length * length)))
        px, py = ax + dx * t, ay + dy * t
        dist = math.hypot(x - px, y - py)
        if best is None or dist < best[0]:
            nx, ny = -dy / length, dx / length
            side = -((x - px) * nx + (y - py) * ny)
            best = (dist, math.copysign(dist, side), total + length * t, (nx, ny))
        total += length
    return best[1], best[2], total, best[3]


def rim_fill(x, y, height, ground_at):
    for name, (run, tread, banks, rise, fade, back) in RIM_FILL_BANKS.items():
        line = RIM_BANK_LINES[name]
        toe = -(banks * run + (banks - 1) * tread)
        d, s, total, (nx, ny) = polyline_frame(x, y, line)
        if not toe < d < back:
            continue
        weight = ridge.smooth(s / fade) * ridge.smooth((total - s) / fade) * ridge.smooth((back - d) / back)
        for cut in base.CUTS:
            weight *= ridge.smooth(-ridge.signed_distance(x, y, cut) / 200)
        if not weight:
            continue
        # 분지 바닥은 같은 법선 위 둑 발치 200cm 앞 높이다.
        ahead = d - toe + 200
        floor = ground_at(x + nx * ahead, y + ny * ahead)
        u = d - toe
        level = floor
        for k in range(banks):
            start = k * (run + tread)
            level = floor + rise * (k + ridge.smooth((u - start) / run))
            if u < start + run + tread:
                break
        if level > height:
            height += weight * (level - height)
    return height


def massif_bank_ground(x, y):
    return rim_fill(x, y, massif_terrace_ground(x, y), massif_terrace_ground)


def massif_bank_feature_points():
    # V007 특징점에 덧댄 둑 위·아래 모서리 선을 1m 간격으로 더한다.
    yield from massif_terrace_feature_points()
    for name, (run, tread, banks, rise, fade, back) in RIM_FILL_BANKS.items():
        line = RIM_BANK_LINES[name]
        toe = -(banks * run + (banks - 1) * tread)
        edges = [toe - 100]
        for k in range(banks):
            start = toe + k * (run + tread)
            edges += [start, start + run / 2, start + run, start + run + 100]
        edges += [0, 100]
        for (ax, ay), (bx, by) in zip(line, line[1:]):
            dx, dy = bx - ax, by - ay
            length = math.hypot(dx, dy)
            nx, ny = -dy / length, dx / length
            for i in range(max(1, round(length / 100))):
                t = i * 100 / length
                for d in edges:
                    yield ax + dx * t - nx * d, ay + dy * t - ny * d


# V008 결과(V030·V031): V029에서 북 띠 돌무더기에 가려 있던 동굴 절개 남서 모서리 옆 V012 바위 돌출(+30.8m, x -750·y 3,825)이
# 북쪽 단(+20m) 위로 뾰족하게 드러났다. 능선 상한(ridge_cap_weight)은 절개 둘레 7.5m에서 0이라 이 돌출을 못 깎았다.
# V009는 V008에서 이 구역만 상한을 둔다. 상한은 절개 경계에서 +2,500(경계 최고 높이 +2,429 이상), 4m 밖에서 북쪽 단 +2,000이다.
SPUR_CAP = {"box": (-1800, 400, 2800, 4800), "cap": 2000, "edge_cap": 2500, "edge": 400}
MASSIF_COVER_VERSIONS["V009"] = ("V012_Z1MassifCover_V009", -2200, 500)
MASSIF_SPUR_VERSION = MASSIF_COVER_VERSIONS["V009"][0]


def spur_cap(x, y, height):
    x0, x1, y0, y1 = SPUR_CAP["box"]
    if not (x0 < x < x1 and y0 < y < y1) or height <= SPUR_CAP["cap"]:
        return height
    sd = min(-ridge.signed_distance(x, y, cut) for cut in base.CUTS)
    if sd <= 0:
        return height
    cap = SPUR_CAP["cap"] + (SPUR_CAP["edge_cap"] - SPUR_CAP["cap"]) * (1 - ridge.smooth(sd / SPUR_CAP["edge"]))
    # 상자 가장자리 6m에서는 원래 지각으로 돌아간다.
    weight = ridge.smooth(min(x - x0, x1 - x, y - y0, y1 - y) / 600)
    return height - weight * max(0.0, height - cap)


def massif_spur_ground(x, y):
    return spur_cap(x, y, massif_bank_ground(x, y))


def massif_spur_feature_points():
    yield from massif_bank_feature_points()


# V033 뒤 맨 지각 평지: 깎은 북쪽 단(+20m)과 남서 모서리(협곡 서안 위 고원 남쪽 끝 봉우리를 +6.5m로 깎은 마루)는 띠(25° 이상)도
# 드레이프(CR 섬 자리)도 닿지 않는다. 이 두 다각형 안에 CloseRange 평면 묶음을 옮겨 심는다(UE `flat`, GX-023 드레이프와 같은 규칙).
# 배치 높이는 V009 지각, 렌더링 지각 V010은 다각형 안에서만 V009보다 FLAT_CARVE 깊이만큼 낮춘다(GX-024 분리, 하상 겹침 V003과
# 같은 이유: ED 지면 타일 윗면이 지각과 같은 높이면 지각이 타일을 덮는다). 다각형은 기존 드레이프 점과 1m 넘게 떨어뜨리고,
# 동굴 절개(홀 x -600 동쪽)는 피한다. 점은 옥탄트 로컬 cm다.
FLAT_REGIONS = {
    "NorthStep": ((-7900, 4500), (-3000, 4300), (-1300, 4000), (-800, 4000), (-800, 8400), (-7900, 8400)),
    "SouthWest": ((-5000, -2800), (-2300, -2800), (-2300, -5800), (-5000, -5800)),
}
# (깊이, 다각형 안쪽 페이드 폭, 낮춤이 끝나는 경사°, 경사 페이드 폭°). 타일은 45°·나머지는 60° 넘는 자리에서 빠진다.
FLAT_CARVE = (50, 100, 60, 15)
MASSIF_COVER_VERSIONS["V010"] = ("V012_Z1MassifCover_V010", -2200, 500)
MASSIF_FLAT_VERSION = MASSIF_COVER_VERSIONS["V010"][0]
# V011은 렌더링 지각이 아니라 U5 충돌 지각이다(Blender BuildRidgeEDCollision.py). V010 높이장에 ED 지면층(납작한 타일·낮은
# 덩어리) 윗면 50cm 래스터를 특징점으로 더하고, 덩어리 프록시 발자국을 절개한다. 시각은 V010이 그대로 맡는다.
MASSIF_COVER_VERSIONS["V011"] = ("V012_Z1MassifCover_V011", -2200, 500)


def flat_region_weight(x, y):
    # 다각형 안쪽 거리로 0→1(FLAT_CARVE 페이드 폭). 다각형 밖은 0이다.
    feather = FLAT_CARVE[1]
    best = 0.0
    for polygon in FLAT_REGIONS.values():
        if ridge.inside(x, y, polygon):
            best = max(best, ridge.smooth(ridge.signed_distance(x, y, polygon) / feather))
    return best


def massif_flat_ground(x, y):
    height = massif_spur_ground(x, y)
    weight = flat_region_weight(x, y)
    if not weight:
        return height
    depth, _, slope_end, slope_fade = FLAT_CARVE
    gx = (massif_spur_ground(x + 50, y) - massif_spur_ground(x - 50, y)) / 100.0
    gy = (massif_spur_ground(x, y + 50) - massif_spur_ground(x, y - 50)) / 100.0
    slope = math.degrees(math.atan(math.hypot(gx, gy)))
    return height - depth * weight * ridge.smooth((slope_end - slope) / slope_fade)


def massif_flat_feature_points():
    # V009 특징점에 다각형 경계와 페이드 안쪽 끝 선을 1m 간격으로 더한다.
    yield from massif_spur_feature_points()
    feather = FLAT_CARVE[1]
    for polygon in FLAT_REGIONS.values():
        for (ax, ay), (bx, by) in zip(polygon, polygon[1:] + polygon[:1]):
            length = math.hypot(bx - ax, by - ay)
            nx, ny = -(by - ay) / length, (bx - ax) / length
            for i in range(max(1, round(length / 100))):
                t = i * 100 / length
                px, py = ax + (bx - ax) * t, ay + (by - ay) * t
                for d in (-feather, 0, feather):
                    x, y = px + nx * d, py + ny * d
                    yield x, y


# U6 러프 PIE(2026-11-23) 7번: 대분지(-40m)와 ValleyRise(-24m) 사이 BossValley 경사면은 V012 설계에서 x 4,000~7,800에 걸친
# 24° 경사였는데, 분지 바닥 평탄화로 x 6,600~7,800 구간이 46~52°가 되어 걸어 오를 수 없었다. 경로 중심선을 따라 걸을 수 있는
# 경사로 하나를 렌더링 지각에 다시 낸다(나머지 사면은 그대로). 높이는 양 끝을 잇는 직선이고, 반폭 안은 그 높이로 맞추며 바깥
# 섞임 폭에서 원래 지각으로 돌아간다(메우기·깎기 모두). 끝 너머도 섞임 폭에서 돌아간다. 배치 지각(드레이프·띠·평면 묶음)은
# 바꾸지 않는다(GX-024 분리). V012는 렌더링 지각이고, 충돌 지각은 U5가 이것을 바탕으로 다시 굽는다.
# (이름: 시작 (x, y, H), 끝 (x, y, H), 반폭 cm, 섞임 폭 cm)
RAMPS = {"BossValley": ((4400, -300, -4000), (7700, -150, -2400), 400, 600)}
MASSIF_COVER_VERSIONS["V012"] = ("V012_Z1MassifCover_V012", -2200, 500)
MASSIF_RAMP_VERSION = MASSIF_COVER_VERSIONS["V012"][0]


def ramp_weight(x, y, ramp):
    (ax, ay, ah), (bx, by, bh), half, blend = ramp
    dx, dy = bx - ax, by - ay
    length = math.hypot(dx, dy)
    t = ((x - ax) * dx + (y - ay) * dy) / (length * length)
    tc = max(0.0, min(1.0, t))
    across = math.hypot(x - (ax + dx * tc), y - (ay + dy * tc))
    along = abs(t - tc) * length
    weight = ridge.smooth((half + blend - across) / blend) * ridge.smooth((blend - along) / blend)
    return weight, ah + (bh - ah) * tc


def massif_ramp_ground(x, y):
    height = massif_flat_ground(x, y)
    for ramp in RAMPS.values():
        weight, target = ramp_weight(x, y, ramp)
        if weight:
            height += weight * (target - height)
    return height


def massif_ramp_feature_points():
    # V010 특징점에 경사로 반폭·섞임 경계선을 1m 간격으로 더한다(경사로 모서리가 삼각분할에 남도록).
    yield from massif_flat_feature_points()
    for (ax, ay, _), (bx, by, _), half, blend in RAMPS.values():
        length = math.hypot(bx - ax, by - ay)
        ux, uy = (bx - ax) / length, (by - ay) / length
        for i in range(-round(blend / 100), round((length + blend) / 100) + 1):
            px, py = ax + ux * i * 100, ay + uy * i * 100
            for d in (-half - blend, -half, 0, half, half + blend):
                yield px - uy * d, py + ux * d
# V013은 V012(경사로) 렌더링 지각과 U6 정리본(Z1Cull V001)에서 U5가 다시 구운 충돌 전용 지각이다(BuildRidgeEDCollision -- build V003).
MASSIF_COVER_VERSIONS["V013"] = ("V012_Z1MassifCover_V013", -2200, 500)
