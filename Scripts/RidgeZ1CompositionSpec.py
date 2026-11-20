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
