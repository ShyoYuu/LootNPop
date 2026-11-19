"""V012 기반면 높이장. 좌표와 보존 형상은 Ridge V011 규약을 쓴다."""
from functools import lru_cache
import math

import RidgeOctantSpec as ridge

VERSION = "V012"
SEAM_BLEND_CM = ridge.SEAM_BAND  # native 15m 띠에서 55m까지 블렌드한다.
MAX_HEIGHT_CM = 7780  # 투영된 삼각형 내부의 구면 처짐까지 고려해 +78m 여유를 둔다.
BLUR_GRID_CM = 850
KERNEL = (1, 8, 28, 56, 70, 56, 28, 8, 1)
RELEASE_CM = 1400
ROCKS = [rock for rock in ridge.active(ridge.ROCKS) if rock["role"] in ("Cave", "Island", "Bridge")]
CUTS = [ridge.rock_outline(rock, rock.get("cut_scale", .995)) for rock in ROCKS if rock.get("cut", True)]

PATH_SEGMENTS = [((ax, ay, aw + 100), (bx, by, bw + 100))
                 for path in ridge.active(ridge.PATHS)
                 for (ax, ay, _, aw), (bx, by, _, bw) in zip(ridge.path_points(path), ridge.path_points(path)[1:])]
ROUTE_SEGMENTS = [((*a, 300), (*b, 300)) for route in ridge.active(ridge.ROUTES) if ridge.route_layer(route) != "cave"
                  for a, b in zip(route["xy"], route["xy"][1:])]
PADS = [(*pod["xy"], 650) for pod in ridge.active(ridge.PODS) if not pod.get("layer")]
PADS += [(*anchor["landing"], 600) for anchor in ridge.active(ridge.ANCHORS) if not anchor.get("layer")]
PADS += [(*abutment, 500) for rock in ROCKS if "deck" in rock for abutment in rock["deck"][:2]]
CAVE_BOUNDS = [(min(x for x, _ in cut), min(y for _, y in cut), max(x for x, _ in cut), max(y for _, y in cut))
               for cut in CUTS]


def distance_to_protected_ground(x, y):
    """보존 노면의 안쪽은 0이고, 바깥은 가장 가까운 경계까지 거리다."""
    distance = min(RELEASE_CM, *(math.hypot(x - px, y - py) - radius for px, py, radius in PADS))
    for (ax, ay, aw), (bx, by, bw) in PATH_SEGMENTS + ROUTE_SEGMENTS:
        span = max(aw, bw) + RELEASE_CM
        if min(ax, bx) - span <= x <= max(ax, bx) + span and min(ay, by) - span <= y <= max(ay, by) + span:
            offset, t = ridge.segment(x, y, ax, ay, bx, by)
            distance = min(distance, offset - (aw + (bw - aw) * t))
    for cut, (xmin, ymin, xmax, ymax) in zip(CUTS, CAVE_BOUNDS):
        span = RELEASE_CM + 100
        if xmin - span <= x <= xmax + span and ymin - span <= y <= ymax + span:
            distance = min(distance, -ridge.signed_distance(x, y, cut) - 100)
    return max(0.0, distance)


@lru_cache(maxsize=None)
def sample(i, j):
    return ridge.ground(i * BLUR_GRID_CM, j * BLUR_GRID_CM)


@lru_cache(maxsize=None)
def blur_x(i, j):
    return sum(weight * sample(i + offset, j) for offset, weight in enumerate(KERNEL, -4)) / sum(KERNEL)


@lru_cache(maxsize=None)
def blurred_sample(i, j):
    return sum(weight * blur_x(i, j + offset) for offset, weight in enumerate(KERNEL, -4)) / sum(KERNEL)


def base_ground(x, y):
    """V011의 저역 통과 높이와 보존 노면을 부드럽게 연결한다(현지 H, cm)."""
    blend = ridge.smooth(distance_to_protected_ground(x, y) / RELEASE_CM)
    if blend == 0:
        return min(ridge.ground(x, y), MAX_HEIGHT_CM)
    gx, gy = x / BLUR_GRID_CM, y / BLUR_GRID_CM
    i, j = math.floor(gx), math.floor(gy)
    u, v = gx - i, gy - j
    low = blurred_sample(i, j) * (1 - u) + blurred_sample(i + 1, j) * u
    high = blurred_sample(i, j + 1) * (1 - u) + blurred_sample(i + 1, j + 1) * u
    blurred = low * (1 - v) + high * v
    if blend == 1:
        return min(blurred, MAX_HEIGHT_CM)
    return min(ridge.ground(x, y) * (1 - blend) + blurred * blend, MAX_HEIGHT_CM)


def feature_points():
    # 보존 노면만 V011의 세밀한 형상선 삼각분할을 이어 받는다.
    return (point for point in ridge.feature_points() if distance_to_protected_ground(*point) < 350)
