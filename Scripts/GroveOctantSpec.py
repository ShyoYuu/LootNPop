"""Grove 전용 배치 원본. 단위는 cm, H는 구 중심 쪽으로 증가한다."""
import math

RADIUS = 30000.0
SEAM_MARGIN = 1500.0
SEAM_BAND = 4000.0


def point(x, y, h):
    raw = (RADIUS/math.sqrt(3)-x/math.sqrt(6)-y/math.sqrt(2),
           RADIUS/math.sqrt(3)-x/math.sqrt(6)+y/math.sqrt(2),
           RADIUS/math.sqrt(3)+2*x/math.sqrt(6))
    scale = (RADIUS-h)/math.sqrt(sum(v*v for v in raw))
    return tuple(v*scale for v in raw)


def ground(x, y):
    # 깊은 단절 없이 서쪽 공터 사이에 얕은 회복 지형을 만든다.
    return -500*math.exp(-((x+6000)/3600)**2-((y-3200)/4800)**2)


TREES = [
    {"id": "Crossing", "xy": [-10000, 0], "radius": 850, "height": 6500,
     "roots": [[0, 3300], [135, 2800], [255, 2600]]},
]

NODES = [
    {"id": "EntryWest", "xy": [-20000, 0]},
    {"id": "ClearingA", "xy": [-11600, -4600]},
    {"id": "ClearingB", "xy": [-8500, 5200]},
    {"id": "WestFlank", "xy": [-14600, 200]},
    {"id": "WestTurn", "xy": [-14000, 5200]},
    {"id": "EastFlank", "xy": [-4800, -400]},
    {"id": "Recovery", "xy": [-5500, 3500]},
]

ROUTES = [
    {"id": "NaturalEntry", "nodes": ["EntryWest", "WestFlank", "ClearingA"]},
    {"id": "WestLoop", "nodes": ["ClearingA", "WestFlank", "WestTurn", "ClearingB"]},
    {"id": "EastLoop", "nodes": ["ClearingA", "EastFlank", "Recovery", "ClearingB"]},
]

PODS = [{"id": n["id"], "xy": n["xy"]} for n in NODES if n["id"] in ("ClearingA", "ClearingB")]
ANCHORS = [{"id": "CrossingEscape", "local": [-5000, 1500, 1300], "max_distance": 9500,
            "from": "ClearingA", "landing": [-5000, 1500]}]


def footprint(tree, angle):
    return tree["radius"] + max((length*max(0, math.cos(angle-math.radians(degrees)))**18
                                     for degrees, length in tree["roots"]), default=0)


def placement():
    return {
        "nodes": [{**n, "local": [*n["xy"], ground(*n["xy"])]} for n in NODES],
        "pods": [{**p, "local": [*p["xy"], ground(*p["xy"])]} for p in PODS],
        "anchors": ANCHORS,
        "routes": ROUTES,
    }
