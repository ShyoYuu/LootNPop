"""Ridge spec 높이장 미리보기·경사·사선 검토. numpy 없이 표준 Python으로 PNG와 JSON을 쓴다.

사용: python Scripts/PreviewRidgeOctant.py [출력 폴더]
투영된 구면 좌표에서 법선과 현지 Up의 각도로 경사를 잰다. 실제 메시/충돌 검사를 대신하지 않는다.
"""
import json
import math
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/"Scripts"))
import RidgeOctantSpec as spec

STEP = 60
X0, X1, Y0, Y1 = -21300, 23000, -19000, 19000
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT/"Saved/SliceOctant/Ridge_00"/spec.VERSION/"Preview"


def sub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def norm(a):
    n = math.sqrt(sum(v*v for v in a))
    return tuple(v/n for v in a)


def slope(x, y, e=50):
    px = sub(spec.point(x+e, y, spec.ground(x+e, y)), spec.point(x-e, y, spec.ground(x-e, y)))
    py = sub(spec.point(x, y+e, spec.ground(x, y+e)), spec.point(x, y-e, spec.ground(x, y-e)))
    n = norm(cross(px, py))
    up = norm(tuple(-v for v in spec.point(x, y, 0)))
    return math.degrees(math.acos(min(1, abs(sum(a*b for a, b in zip(n, up))))))


def inside(x, y):
    return min(spec.point(x, y, 0)) > 0


def png(path, width, height, rows):
    raw = b"".join(b"\x00"+bytes(row) for row in rows)
    def chunk(tag, data):
        return struct.pack(">I", len(data))+tag+data+struct.pack(">I", zlib.crc32(tag+data) & 0xffffffff)
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw, 9))+chunk(b"IEND", b""))


def los(a, b, steps=240):
    """두 로컬 점 사이 직선을 월드에서 표본화해 지각 높이장이 막는지 본다(암반 제외)."""
    pa, pb = spec.point(*a), spec.point(*b)
    for i in range(1, steps):
        t = i/steps
        p = tuple(pa[k]+(pb[k]-pa[k])*t for k in range(3))
        x, y, h = spec.local(p)
        if h < spec.ground(x, y):
            return False
    return True


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    nx, ny = (X1-X0)//STEP+1, (Y1-Y0)//STEP+1
    heights = [[None]*nx for _ in range(ny)]
    for j in range(ny):
        y = Y1-j*STEP
        for i in range(nx):
            x = X0+i*STEP
            if inside(x, y):
                heights[j][i] = spec.ground(x, y)
    # 표시: 높이 색 + 언덕 음영 + 2.5m 등고선 + 경사 35° 이상 붉게.
    rows = []
    for j in range(ny):
        row = []
        for i in range(nx):
            h = heights[j][i]
            if h is None:
                row += [24, 24, 28]
                continue
            x, y = X0+i*STEP, Y1-j*STEP
            t = max(0, min(1, (h+4000)/14000))
            r, g, b = 70+120*t, 60+110*t, 95+120*t
            hl = heights[j][i-1] if i > 0 else None
            hu = heights[j-1][i] if j > 0 else None
            if hl is not None and hu is not None:
                shade = max(0.55, min(1.25, 1+((h-hl)-(h-hu))/STEP*0.9))
                r, g, b = r*shade, g*shade, b*shade
                if math.floor(h/500) != math.floor(hl/500) or math.floor(h/500) != math.floor(hu/500):
                    r, g, b = r*0.7, g*0.7, b*0.7
            if i % 2 == 0 and j % 2 == 0:
                s = slope(x, y)
                if s > 45:
                    r, g, b = 150, 40, 40
                elif s > 32:
                    r, g, b = 240, 140, 40
            row += [int(max(0, min(255, v))) for v in (r, g, b)]
        rows.append(row)

    def put(x, y, color, size=2):
        i, j = round((x-X0)/STEP), round((Y1-y)/STEP)
        for dj in range(-size, size+1):
            for di in range(-size, size+1):
                if 0 <= j+dj < ny and 0 <= i+di < nx:
                    rows[j+dj][3*(i+di):3*(i+di)+3] = color
    for route in spec.active(spec.ROUTES):
        color = {"walk": [250, 250, 250], "walk-exposed": [255, 200, 60], "walk-flank": [90, 220, 255],
                 "walk-outer": [140, 255, 140], "walk-crest": [255, 140, 220]}[route["kind"]]
        for (ax, ay), (bx, by) in zip(route["xy"], route["xy"][1:]):
            n = int(math.hypot(bx-ax, by-ay)/STEP)+1
            for k in range(n+1):
                put(ax+(bx-ax)*k/n, ay+(by-ay)*k/n, color, 0)
    for rock in spec.active(spec.ROCKS):
        pts = spec.rock_outline(rock)
        for (ax, ay), (bx, by) in zip(pts, pts[1:]+pts[:1]):
            n = int(math.hypot(bx-ax, by-ay)/STEP)+1
            for k in range(n+1):
                put(ax+(bx-ax)*k/n, ay+(by-ay)*k/n, [40, 30, 40], 0)
    for pod in spec.active(spec.PODS):
        put(*pod["xy"], [255, 255, 255], 4)
    for anchor in spec.active(spec.ANCHORS):
        put(*anchor["landing"], [255, 90, 0], 3)
    png(OUT/"Height.png", nx, ny, rows)

    # 경로 표본: 길을 따라 1m 간격 경사와 높이.
    routes = {}
    for route in spec.active(spec.ROUTES):
        samples = []
        for (ax, ay), (bx, by) in zip(route["xy"], route["xy"][1:]):
            n = int(math.hypot(bx-ax, by-ay)/100)
            samples += [(ax+(bx-ax)*k/n, ay+(by-ay)*k/n) for k in range(n)]
        slopes = [slope(x, y) for x, y in samples]
        hs = [spec.ground(x, y) for x, y in samples]
        routes[route["id"]] = {"kind": route["kind"], "max_slope_deg": round(max(slopes), 2),
                               "min_h": round(min(hs)), "max_h": round(max(hs)), "samples": len(samples)}
    eye = 170
    nodes = {n["id"]: (n["xy"][0], n["xy"][1], spec.layer_height(*n["xy"], n.get("layer", "surface"))+eye) for n in spec.active(spec.NODES)}
    targets = {"Pod_"+pod["id"]: (pod["xy"][0], pod["xy"][1], spec.layer_height(*pod["xy"], pod.get("layer", "surface"))+150) for pod in spec.active(spec.PODS)}
    crag = next(r for r in spec.active(spec.ROCKS) if r["id"] == spec.LANDMARK_ROCK)
    top = max(crag["outline"], key=lambda p: p[2])
    targets["LandmarkTop"] = (top[0], top[1], spec.ground(top[0], top[1])+top[2]+200)
    sight = {a: {b: los(pa, pb) for b, pb in targets.items() if b != "Pod_"+a} for a, pa in nodes.items()}
    expect = [[a, b, want, sight.get(a, {}).get(b)] for a, b, want in spec.SIGHT_EXPECTATIONS]
    hooks = {}
    for anchor in spec.placement()["anchors"]:
        for source in anchor["from"]:
            start = nodes[source]
            pa, pb = spec.point(*start), spec.point(*anchor["local"])
            hooks[anchor["id"]+"<-"+source] = {"distance_cm": round(math.dist(pa, pb)), "max_distance": anchor["max_distance"],
                                               "clear": los(start, anchor["local"]),
                                               "landing_h": round(spec.ground(*anchor["landing"]))}
    report = {"version": spec.VERSION, "stage": spec.STAGE, "routes": routes, "sight_from_eye_170cm": sight, "hooks": hooks,
              "sight_expectations": expect,
              "relief_m": [round(min(h for row in heights for h in row if h is not None)/100, 1), round(max(h for row in heights for h in row if h is not None)/100, 1)],
              "pods": {p["id"]: round(spec.ground(*p["xy"])) for p in spec.active(spec.PODS)},
              "limits": ["Analytic height field; rocks not included in sight lines", "Not the exported mesh, collision or capsule"]}
    (OUT/"preview.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    print(json.dumps({k: report[k] for k in ("routes", "pods", "relief_m", "sight_expectations")}, indent=0))
    print(json.dumps(hooks, indent=0))


if __name__ == "__main__":
    main()
