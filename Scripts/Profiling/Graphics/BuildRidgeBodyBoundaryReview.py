"""U5a-7 R01 접합 검토와 기존 경사로의 실제 겹침 구간을 그림/표로 정리한다."""
import base64
import html
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_7_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))


def main():
    d=read(UNIT/'BoundarySurvey.json');view=read(UNIT/'ReviewViewState.json')
    encoded=UNIT/'R01_Selected_West.txt'
    encoded.with_suffix('.png').write_bytes(base64.b64decode(encoded.read_text(encoding='utf-8-sig')))
    grid=d['R01_grid'];hits=d['legacy_hits'];cells=''
    for row in grid['rows']:
        x=75+(row['horizontal_cm']+100)*2;y=75+(100-row['vertical_cm'])*2
        color='#6bcc9a' if row['body_hit_count'] else '#ed7d7d'
        value=f"{row['first_segment_thickness_cm']/100:.2f}m" if row['first_segment_thickness_cm'] is not None else '미검출'
        cells+=f'<rect x="{x}" y="{y}" width="47" height="47" fill="{color}"/><text x="{x+23}" y="{y+28}" text-anchor="middle" font-size="11" fill="#12202b">{value}</text>'
    svg=f'<svg viewBox="0 0 600 570" xmlns="http://www.w3.org/2000/svg"><rect width="600" height="570" fill="#172333"/><text x="40" y="30" fill="white" font-size="18">R01 주변 2m × 2m · 간격25cm · 81광선</text><text x="40" y="54" fill="#b2c4d8" font-size="13">값: 첫 몸체 구간 두께 / 빨강: 해당 방향 몸체 미검출</text>{cells}<text x="65" y="545" fill="#b2c4d8" font-size="13">아래 y=-100cm의 9미검출은 기존 암석 접합부 육안 검토 대상</text></svg>'
    (UNIT/'R01_RayGrid.svg').write_text(svg,encoding='utf-8')
    overlay=''
    for name,p in view['projected_points'].items():
        color='#ffd05a' if name=='R01' else '#55d8ff'
        overlay+=f'<circle cx="{p["x"]*1127}" cy="{p["y"]*724}" r="12" fill="none" stroke="{color}" stroke-width="3"/><text x="{p["x"]*1127+18}" y="{p["y"]*724}" fill="{color}" font-size="17">{name}</text>'
    table=''.join(f'<tr><td>{r["sample_index"]}</td><td>{r["station"]}/31</td><td>{r["lateral_offset_cm"]:+}</td><td>{r["height_offset_cm"]}</td><td>{html.escape(r["owner"])}</td><td>{r["surface_distance_cm"]:.2f}</td></tr>' for r in hits)
    central=sorted({r['station'] for r in hits if r['lateral_offset_cm']==0 and r['owner']!='kept_connection'})
    station_svg='<svg viewBox="0 0 1040 120" xmlns="http://www.w3.org/2000/svg"><rect width="1040" height="120" fill="#172333"/>'
    for i in range(32):
        color='#ef8585' if i in central else '#82aacb'
        station_svg+=f'<rect x="{10+i*32}" y="35" width="27" height="28" fill="{color}"/><text x="{23+i*32}" y="85" text-anchor="middle" fill="white" font-size="12">{i}</text>'
    station_svg+='<text x="10" y="22" fill="white" font-size="15">기존 Ramp3 · 빨강은 중앙선도 하부 몸체 내부인 표본 station</text></svg>'
    page='''<!doctype html><html lang="ko"><meta charset="utf-8"><title>U5a-7 R01·기존 Ramp3 검토</title><style>body{background:#101721;color:#e6edf5;font:16px system-ui;margin:26px}p{max-width:1100px;line-height:1.6}a{color:#8dccff}img,svg{max-width:100%}table{border-collapse:collapse;font-size:14px}td,th{border:1px solid #405269;padding:6px}.frame{position:relative;max-width:1127px}.frame img{display:block;width:100%}.overlay{position:absolute;inset:0;width:100%;height:100%}.grid{max-width:600px}</style><h1>U5a-7 · 수정 전 V007 조사 기록</h1><p>후속 사용자 확인과 V008 국소 보강 결과는 <a href="Comparison_V008.html">최종 비교</a>·NativeGeometryCheck_V008.json을 따른다. 아래 미검출/수동 대기는 최초 조사 시점 이력이다.</p><p>형상·맵·재질·보호 폭을 바꾸지 않았다. V007 시각 채택은 Acceptance_V007이 소유한다. R01 주변 아래쪽 미검출은 기존 암석과의 접합을 육안 확인 중이며, 이를 구멍 확정이나 폐쇄 완료로 표시하지 않는다. 현재 몸체/연결은 NoCollision이고 실제 보행/PIE 통과가 아니다.</p><h2>R01 선택 화면과 표본</h2>'''
    page+=f'<div class="frame"><img src="R01_Selected_West.png"><svg class="overlay" viewBox="0 0 1127 724">{overlay}</svg></div><div class="grid">{svg}</div>'
    page+=f'<p>기록점은 닫힌 몸체 밖8.298cm다. 중앙 광선은 몸체 안을8.098m 통과하고, 만난72광선의 최소 두께는1.028m다. 아래9개는 유한한 몸체를 만나지 않는다. 이 줄은 저작 지면보다 약6.42m 위여서 지면 아래라 무시하지 않는다. 기존 ED 외피/암석 접합을 수동 검토한다. 캐릭터 캡슐42/96cm는 치수 참고일 뿐 이 검사는 연속 캡슐/물리 검사가 아니다.</p><h2>기존 StairRamp3는 중앙 구간도 겹친다</h2>{station_svg}'
    page+='<p>기존192표본/3m 폭을 유지했다. 하부28+연결3=31표본이며, 중앙선 station19~23은 위100/250cm 모두 몸체 안이다. 가장자리 일부 접촉으로 축소하지 않는다. 기존 경로는 계속 REVIEW로 남긴다. 채택한 별도 연결의3m/858정점·13,221표본은 몸체 침범0·거리 하한252.321cm다. 기존 경로를 빼서 PASS로 만들지 않는다.</p>'
    page+=f'<table><tr><th>표본</th><th>station</th><th>가로(cm)</th><th>높이(cm)</th><th>소유자</th><th>표면 거리(cm)</th></tr>{table}</table>'
    page+='<p>전체51보호 공간/16,552표본도 재검사해 다른50공간 새 겹침0·기존31 그대로다. <a href="BoundarySurvey.json">좌표·광선·전체 회귀 원자료</a></p></html>'
    (UNIT/'BoundaryReview.html').write_text(page,encoding='utf-8')
    print(json.dumps({'rays':81,'misses':9,'legacy_hits':31,'central_blocked_stations':central,'geometry_changed':False}))


if __name__=='__main__':main()
