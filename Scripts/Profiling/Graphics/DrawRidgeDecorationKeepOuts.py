"""장식 규칙 에셋의 보호 형상(2D 동선·암반, 3D Pod·착지·훅·카메라)을 에디터 뷰포트에 디버그 선으로 그린다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/DrawRidgeDecorationKeepOuts.py" [draw|clear]
규칙 에셋(DA_RidgeDecorationRules)을 읽기만 한다. 선은 저장되지 않는 디버그 선이며 clear로 지운다.
C++ 샘플러와 같은 판정 경계를 그린다: 열린 선은 선분마다 두 정점 반폭 중 큰 값 + Margin.

색 규약
  노랑: 동선 중심선 · 연두: Fern 경계(반폭+Margin+FERN_PADDING) · 빨강: 작은 바위 경계(+BLOCKER_PADDING)
  분홍: 큰 바위 경계(+BIG_ROCK_PADDING) · 하양: 암반·동굴 외곽 다각형(그 밖으로 Margin 더 보호)
  역할별 경계는 메시 전체(회전·스케일 포함 바닥 다각형)가 넘으면 안 되는 선이다.
  주황: Pod 편성 구(지피만 허용) · 하늘: 앵커 착지부·훅 조준선 · 보라: 벤치마크 카메라·검토 시점
  초록: 장식 구역(ZONE) 외곽
"""
import math
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))
import RidgeOctantSpec as spec

RULES = "/Game/Graphics/Environment/ElectricDreams/RidgeDecoration_V001/DA_RidgeDecorationRules"
# BuildRidgeDecorationPCG.py와 같은 값(그 모듈은 import 시 규칙 상수가 많아 복사한다).
FERN_PADDING = -50
BLOCKER_PADDING = 300
BIG_ROCK_PADDING = 450
LIFT = 40.0             # 지면 위로 띄우는 높이(cm). 지면에 묻혀 안 보이는 것을 막는다.
STEP = 150.0            # 곡면을 따라가게 선을 잘게 나누는 간격(cm)
DURATION = 1.0e7        # 사실상 영구. clear로 지운다.
THICK = 6.0
VIEW_MARGIN = 3000.0    # 구역 밖으로 이 거리까지만 그린다.

COLORS = {"center": (1, .9, 0), "fern": (.5, 1, .2), "edge": (1, .1, .1), "big": (1, .3, .8), "polygon": (1, 1, 1),
          "pod": (1, .5, 0), "hook": (.2, .8, 1), "camera": (.6, .3, 1), "zone": (.1, 1, .2)}


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def color(name):
    r, g, b = COLORS[name]
    return unreal.LinearColor(r, g, b, 1)


def ground(x, y):
    return unreal.Vector(*spec.point(x, y, spec.ground(x, y) + LIFT))


def near_zone(x, y):
    return spec.signed_distance(x, y, list(ZONE)) > -VIEW_MARGIN


def line2d(a, b, name):
    """spec 로컬 XY 선분을 지면을 따라 잘게 나눠 그린다."""
    n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / STEP))
    prev = None
    for i in range(n + 1):
        t = i / n
        x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
        p = ground(x, y) if near_zone(x, y) else None
        if prev is not None and p is not None:
            unreal.SystemLibrary.draw_debug_line(world(), prev, p, color(name), DURATION, THICK)
        prev = p


def circle2d(cx, cy, r, name):
    n = max(24, int(2 * math.pi * r / STEP))
    pts = [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n + 1)]
    for a, b in zip(pts, pts[1:]):
        line2d(a, b, name)


def offset_segment(a, b, d):
    """선분 ab를 왼쪽·오른쪽으로 d만큼 민 두 선분과, 끝을 둥글게 닫는 반원 점들."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dy) or 1.0
    nx, ny = -dy / length * d, dx / length * d
    return ((a[0] + nx, a[1] + ny), (b[0] + nx, b[1] + ny)), ((a[0] - nx, a[1] - ny), (b[0] - nx, b[1] - ny))


def draw_polyline(points, margin):
    for (ax, ay, aw), (bx, by, bw) in zip(points, points[1:]):
        line2d((ax, ay), (bx, by), "center")
        half = max(aw, bw)  # C++ KeepOut2DClearance와 같은 보수적 반폭
        for d, name in ((half + margin + FERN_PADDING, "fern"), (half + margin + BLOCKER_PADDING, "edge"),
                        (half + margin + BIG_ROCK_PADDING, "big")):
            for seg in offset_segment((ax, ay), (bx, by), d):
                line2d(seg[0], seg[1], name)
    for x, y, w in (points[0], points[-1]):  # 끝은 원으로 닫힌다(선분 거리 판정)
        circle2d(x, y, w + margin + BLOCKER_PADDING, "edge")


def draw():
    rules = unreal.load_asset(RULES)
    assert rules, RULES
    clear()
    counts = {}
    for shape in rules.get_editor_property("keep_out2d"):
        pts = [(p.x, p.y, p.z) for p in shape.get_editor_property("points")]
        margin = shape.get_editor_property("margin")
        sid = str(shape.get_editor_property("id"))
        if shape.get_editor_property("closed_polygon"):
            for a, b in zip(pts, pts[1:] + pts[:1]):
                line2d(a[:2], b[:2], "polygon")
        elif len(pts) == 1:
            circle2d(pts[0][0], pts[0][1], pts[0][2] + margin, "edge")
        else:
            draw_polyline(pts, margin)
        counts[sid.split("_")[0]] = counts.get(sid.split("_")[0], 0) + 1
    for shape in rules.get_editor_property("keep_out3d"):
        sid = str(shape.get_editor_property("id"))
        start, end = shape.get_editor_property("start"), shape.get_editor_property("end")
        radius = shape.get_editor_property("radius")
        kind = "pod" if sid.startswith("Pod_") else "camera" if sid.startswith(("Benchmark", "View_")) else "hook"
        if (start - end).length() < 1.0:
            x, y, _ = spec.local([start.x, start.y, start.z])
            circle2d(x, y, radius, kind)
        else:
            unreal.SystemLibrary.draw_debug_line(world(), start, end, color(kind), DURATION, THICK)
        counts[sid.split("_")[0]] = counts.get(sid.split("_")[0], 0) + 1
    zone = list(ZONE)
    for a, b in zip(zone, zone[1:] + zone[:1]):
        line2d(a, b, "zone")
    unreal.log("Ridge decoration keep-outs drawn: %s" % counts)


def clear():
    unreal.SystemLibrary.flush_persistent_debug_lines(world())


ZONE = None


def load_zone():
    global ZONE
    rules = unreal.load_asset(RULES)
    ZONE = [(v.x, v.y) for v in rules.get_editor_property("zone")]


if __name__ == "__main__":
    load_zone()
    mode = sys.argv[1] if len(sys.argv) > 1 else "draw"
    {"draw": draw, "clear": clear}[mode]()
