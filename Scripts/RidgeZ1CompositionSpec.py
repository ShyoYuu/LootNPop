"""Electric Dreams 암괴 배치를 먼저 정한 Z1 비교본의 기반면과 배치 원본."""
import math

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


def massif_cover_ground(x, y, version=None):
    original = west_cover_ground(x, y)
    _, start, ramp = MASSIF_COVER_VERSIONS[version or MASSIF_COVER]
    # 02 벽 안쪽에서 고원으로 들어가고, x −10,000 서쪽에서 V012 사면으로 돌아간다. 남북 범위는 서쪽 덮개와 같다.
    along = ridge.smooth((y + 4900) / 1550) * ridge.smooth((4480 - y) / 1450)
    across = ridge.smooth((start - x) / ramp) * ridge.smooth((x + 11500) / 1500)
    if not along or not across:
        return original
    for cut in base.CUTS:
        across *= ridge.smooth((-ridge.signed_distance(x, y, cut) - 100) / 650)
    if not across:
        return original
    plateau = ground(-1250, y) + MASSIF_PLATEAU_CM
    return original + along * across * (plateau - original)


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
