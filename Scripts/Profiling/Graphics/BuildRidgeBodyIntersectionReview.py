"""U5a-6 면 번호별 오프라인 형상 검토 화면을 만든다. 실제 UE 화면과 구별한다."""
import json
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_6_V001'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def binary(path):
    data=path.read_bytes();nv,nt=struct.unpack_from('<ii',data)
    return list(struct.iter_unpack('<fff',data[8:8+nv*12])),list(struct.iter_unpack('<iii',data[8+nv*12:]))


def main():
    source=read(OUT/'CandidateClassification.json')
    validation=read(OUT/'LocalRepairValidation.json')
    data=[]
    for body,verified in zip(source['body_results'],validation['bodies']):
        v,t=binary(ROOT/body['binary'])
        changes={r['vertex']:r['after_cm'] for r in verified['sparse_vertex_changes']}
        for shared,rows in [(False,body['pairs']),(True,body['shared_bvh_pairs'])]:
            for row in rows:
                ids=[t[f] for f in row['faces']]
                data.append({'body':body['name'],'faces':row['faces'],'shared':shared,'category':row['category'],
                    'length_cm':row['intersection_length_cm'],'origin_cm':row.get('intersection_world_cm'),
                    'before':[[v[i] for i in tri] for tri in ids],
                    'after':[[changes.get(i,v[i]) for i in tri] for tri in ids],
                    'cause':'shared vertex crossing' if shared else 'local fold with positive interior intersection segment'})
    # 사용자용 설명은 한국어로 기록한다. 런타임 식별자/단위는 원자료와 맞춘다.
    for row in data:
        row['cause']='공유 정점 바깥의 국소 접힘/관통' if row['shared'] else '서로 다른 면 내부를 지나는 국소 접힘/관통'
    report={'candidate_pairs':339,'additional_shared_vertex_crossings':126,'review_pairs':len(data),
            'note':'교차 면 쌍 수이며 서로 독립적인 결함 위치 수가 아니다. SVG는 저장 형상의 투영도이며 실제 UE 촬영이 아니다.',
            'data':data}
    assert len(data)==465
    (OUT/'PairReviewData.json').write_text(json.dumps(report,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf-8')
    template='''<!doctype html><html lang="ko"><meta charset="utf-8"><title>U5a-6 자기 교차 면 검토</title>
<style>body{margin:28px;background:#101721;color:#e4edf6;font:16px system-ui}h1{font-size:26px}p{max-width:1100px;line-height:1.6}.muted{color:#a4b4c6}select,button{background:#203246;color:#fff;border:1px solid #53708a;padding:9px;margin:4px}section{display:grid;grid-template-columns:1fr 1fr;gap:12px}.card{background:#172333;padding:16px;border-radius:10px}svg{width:100%;background:#111c28;border:1px solid #3c5065}#info{font:14px monospace;white-space:pre-wrap}a{color:#8ccbff}</style>
<h1>U5a-6 자기 교차 후보 검토 · 오프라인 국소 보정안</h1>
<p>V006의 기존 후보 <b>339쌍(12/16/311/0)</b>은 두 독립 검사에서 모두 실제 관통으로 분류됐다. 공유 정점을 이유로 제외됐던 <b>126쌍(4/1/121/0)</b>도 공유점 바깥에서 관통한다. 합계 465는 면 쌍 수이며 결함 위치 수가 아니다. 공면·접선·퇴화로 분류된 쌍은 0이다.</p>
<p class="muted">면 번호는 저장 V005 bin의 0 기반 인덱스다. UE 렌더 면 번호와 같다고 가정하지 않는다. 아래는 오프라인 보정안의 수학적 검토 투영도이며 UE 캡처가 아니다. 원본 V006과 기존417파일은 유지한다. __STAGE__</p>
<label>몸체 <select id="body"></select></label><label>면 쌍 <select id="pair"></select></label><button onclick="step(-1)">이전</button><button onclick="step(1)">다음</button>
<p id="info"></p><section><div class="card"><h2>V006 원본</h2><div id="before"></div></div><div class="card"><h2>최소 보정안</h2><div id="after"></div></div></section>
<p class="muted">빨강=첫 면, 파랑=둘째 면. 동일한 국소 좌표계/배율의 정면·사선 투영으로 비교한다. 정점 76/68/2,010/0개, 최대 이동 5.834/6.445/7.258/0cm. 면 연결·정점/면 수·경계 상자 극값 유지. 연결 메시·재질·858 보행면 정점과 하부 몸체는 고정했다. 오프라인 교차 재검사 0쌍, 51공간 16,552표본 결과 동일, 다른50공간 새 겹침0, 통로13,221표본 침범0·거리 하한252.32cm. 기존 StairRamp3의31표본과 R01 경계 민감성은 REVIEW이며 충돌/PIE 통과가 아니다.</p>
<p><a href="CandidateClassification.json">면별 판정 원자료</a> · <a href="LocalRepairValidation.json">보호/통로 검사와 정점 변경</a> · <a href="PreservationAudit.json">417파일 보존 검사</a></p>
<script>const D=__DATA__;
const B=document.getElementById('body'),P=document.getElementById('pair');
const sub=(a,b)=>a.map((x,i)=>x-b[i]),dot=(a,b)=>a.reduce((s,x,i)=>s+x*b[i],0),cross=(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]],norm=a=>{let n=Math.hypot(...a);return a.map(x=>x/n)};
for(const b of [...new Set(D.map(x=>x.body))])B.add(new Option(b,b));
let rows=[];function populate(){rows=D.filter(x=>x.body===B.value);P.innerHTML='';rows.forEach((r,i)=>P.add(new Option(`${r.faces.join(' / ')}${r.shared?' · 공유 정점':''}`,i)));draw()}
function step(d){P.selectedIndex=(P.selectedIndex+d+rows.length)%rows.length;draw()}
function draw(){let r=rows[+P.value];let all=[...r.before.flat(),...r.after.flat()];let c=all[0].map((_,i)=>all.reduce((s,p)=>s+p[i],0)/all.length);let t=r.before[0];let z=norm(cross(sub(t[1],t[0]),sub(t[2],t[0]))),x=norm(sub(t[1],t[0])),y=norm(cross(z,x));
document.getElementById('info').textContent=`${r.body} · faces ${r.faces.join('/')} · ${r.category}\n교차 선분 ${r.length_cm.toFixed(6)}cm · ${r.cause} · ${r.shared?'추가 공유 정점 검토':'기존339후보'}`;
for(const side of ['before','after']){let out='';for(const angle of [0,.85]){let yy=y.map((v,i)=>v*Math.cos(angle)+z[i]*Math.sin(angle));let coords=all.map(p=>[dot(sub(p,c),x),dot(sub(p,c),yy)]);let scale=170/Math.max(1,...coords.flat().map(Math.abs));out+='<svg viewBox="0 0 440 400">';r[side].forEach((tri,j)=>{let pts=tri.map(p=>[220+dot(sub(p,c),x)*scale,200-dot(sub(p,c),yy)*scale]);out+=`<polygon points="${pts.map(p=>p.join(',')).join(' ')}" fill="${j?'#49adff':'#ff6767'}" fill-opacity=".24" stroke="${j?'#49adff':'#ff6767'}" stroke-width="2"/>`;pts.forEach((p,i)=>out+=`<circle cx="${p[0]}" cy="${p[1]}" r="3" fill="${j?'#49adff':'#ff6767'}"/><text x="${p[0]+5}" y="${p[1]-5}" fill="#dce8f5" font-size="11">${j?'B':'A'}${i}</text>`)});out+=`<text x="12" y="25" fill="#aabacd" font-size="13">${angle?'사선':'면 정면'} · ${(40/scale).toFixed(2)}cm / 40px</text></svg>`}document.getElementById(side).innerHTML=out}}
B.onchange=populate;P.onchange=draw;populate();</script></html>'''
    stage='후속 V007 반입·실제 보존/정점/교차0·동일26비교를 마쳤으며 사용자 외관 검토 대기다. <a href="Comparison_V007.html">실제26시점 비교</a>' if (OUT/'NativeGeometryCheck_V007.json').exists() else 'V007 실제 반입/검사는 대기다.'
    (OUT/'CandidateReview.html').write_text(template.replace('__STAGE__',stage).replace('__DATA__',json.dumps(data,ensure_ascii=False,separators=(',',':'))),encoding='utf-8')
    print(json.dumps({'review_pairs':len(data),'html':str(OUT/'CandidateReview.html')}))


if __name__=='__main__':
    main()
