"""Ridge spec 높이장의 대표 단면 SVG를 Art/Octants/Ridge_00/Sections.svg로 쓴다(해석 높이, 암반은 막대로 표시)."""
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/"Scripts"))
import RidgeOctantSpec as s

SECTIONS = [
    ("W→E: 서쪽 진입 → 계단 상승로(1·2·3단) → 왕관 정상 +84m → 동쪽 절벽 → 대분지 -35m → 상승 골짜기 → 보스 고원 → 쌍둥이 첨탑",
     [(-19500, 0), (-12300, -400), (-3000, -800), (3300, -300), (12800, 0), (21000, 0)]),
    ("S→N(x≈-4500): 메사 1단 → 메사 정상 +42m → 협곡(위로 하늘 석교) → 산괴 2단 → 왕관 정상 → 3단 → 2단 고개 → 북쪽",
     [(-4600, -14200), (-4600, -9800), (-4500, -7300), (-4400, -5300), (-3000, -800), (-3800, 4500), (-5400, 9000), (-5000, 13500)]),
    ("N→S(x≈2000): 북쪽 대지 +21m → 잔도 → 대분지 -35m → 협곡 출구 → 메사 동쪽 끝 → 남쪽 저지",
     [(2000, 9000), (2200, 3500), (3300, -300), (1200, -2600), (-1000, -9800), (0, -14500)]),
]
W, H = 1200, 280


def main():
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" font-family="sans-serif" font-size="12">' % (W, H*len(SECTIONS)+20),
           '<rect width="100%" height="100%" fill="#fff"/>']
    for k, (title, pts) in enumerate(SECTIONS):
        samples, dist, prev = [], 0.0, None
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            n = int(math.hypot(bx-ax, by-ay)/100)
            for i in range(n+1):
                x, y = ax+(bx-ax)*i/n, ay+(by-ay)*i/n
                if prev:
                    dist += math.hypot(x-prev[0], y-prev[1])
                prev = (x, y)
                samples.append((dist, s.ground(x, y), x, y))
        total, oy = samples[-1][0], k*H+30

        def sx(d):
            return 40+d/total*(W-60)

        def sy(h):
            return oy+190-(h+4000)/17000*170
        out.append('<text x="40" y="%d" font-weight="bold">%s</text>' % (oy, title))
        for h in range(-4000, 13001, 2000):
            out.append('<line x1="40" x2="%d" y1="%.1f" y2="%.1f" stroke="#ddd"/><text x="5" y="%.1f">%dm</text>' % (W-20, sy(h), sy(h), sy(h)+4, h//100))
        out.append('<polyline fill="none" stroke="#6a6276" stroke-width="2" points="%s"/>' % " ".join("%.1f,%.1f" % (sx(d), sy(h)) for d, h, _, _ in samples))
        for rock in s.active(s.ROCKS):
            poly = s.rock_outline(rock)
            for d, h, x, y in samples[::3]:
                inside = False
                for a, b in zip(poly, poly[1:]+poly[:1]):
                    if (a[1] > y) != (b[1] > y) and x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
                        inside = not inside
                if inside:
                    top = max(s.rock_top(rock, px, py, pt) for px, py, pt in rock["outline"])
                    bottom = rock.get("bottom_abs", h)
                    out.append('<line x1="%.1f" x2="%.1f" y1="%.1f" y2="%.1f" stroke="#444" stroke-width="3"/>' % (sx(d), sx(d), sy(bottom), sy(min(top, 13000))))
        for pod in s.active(s.PODS):
            for d, h, x, y in samples:
                if math.hypot(x-pod["xy"][0], y-pod["xy"][1]) < 150:
                    out.append('<circle cx="%.1f" cy="%.1f" r="5" fill="#f0a020"/><text x="%.1f" y="%.1f">Pod %s</text>' % (sx(d), sy(h), sx(d)+6, sy(h)-8, pod["id"]))
                    break
        out.append('<text x="%d" y="%d">거리 %.0fm(접평면 X/Y). 세로 축척 약 1.7배, 암반은 +130m에서 자름</text>' % (W-430, oy+215, total/100))
    out.append('</svg>')
    (ROOT/"Art/Octants/Ridge_00/Sections.svg").write_text("\n".join(out), encoding="utf-8")


if __name__ == "__main__":
    main()
