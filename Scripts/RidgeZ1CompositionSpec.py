"""Electric Dreams 암괴 배치를 먼저 정한 Z1 비교본의 기반면과 배치 원본."""
import math

import RidgeEDBaseSpec as base

VERSION = "V012_Z1Composition"
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


def placements():
    for name, asset, (x, y, h), yaw, tilt, scale in ASSEMBLY:
        if asset.startswith("Cliff_"):
            mesh = SMART_MESHES + "SM_" + asset
        else:
            mesh = MEGASCANS + asset.rsplit("_", 1)[0] + "/SM_" + asset
        overrides = {"0": MEGASCANS + "HugeSandstoneCliff/MI_HugeSandstoneCliff_07"} if asset == "HugeSandstoneCliff_07" else {}
        yield {"id": "ED_Z1_" + name, "mesh": mesh, "local_xy_cm": [x, y], "height_offset_cm": h - ground(x, y),
               "yaw_deg": yaw, "tilt_deg": tilt, "scale": scale, "material_overrides": overrides}
