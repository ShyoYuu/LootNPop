"""U5a-6 실제 동일26카메라 전후 화면을 검증하고 비교 화면/검토용 축소 모음을 만든다."""
import base64
import hashlib
import html
import json
from pathlib import Path
import struct
import bpy
import numpy as np

ROOT=Path(__file__).resolve().parents[2]
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_6_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def pixels(path):
    image=bpy.data.images.load(str(path),check_existing=False)
    a=np.asarray(image.pixels[:],dtype=np.float32).reshape(image.size[1],image.size[0],4)
    bpy.data.images.remove(image);return a


def main():
    before=read(UNIT/'BeforeCaptureMetadata.json');after=read(UNIT/'AfterCaptureMetadata.json')
    assert len(before)==len(after)==26 and set(before)==set(after)
    for folder in ('Views_Before','Views_After'):
        for p in (UNIT/folder).glob('*.txt'):
            p.with_suffix('.png').write_bytes(base64.b64decode(p.read_text(encoding='utf-8-sig')))
    rows=[];thumbnails=[]
    for name in before:
        a,b=before[name],after[name]
        errors={k:max(abs(a[k][axis]-b[k][axis]) for axis in a[k]) for k in ('cameraLocation','cameraRotation')}
        assert max(errors.values())<.000001 and a['cameraFOV']==b['cameraFOV']==90
        paths=[UNIT/folder/(name.lower()+'.png') for folder in ('Views_Before','Views_After')]
        for path in paths:assert struct.unpack('>II',path.read_bytes()[16:24])==(1127,724)
        old,new=[pixels(p) for p in paths]
        delta=np.abs(old[:,:,:3]-new[:,:,:3])
        rows.append({'name':name,'camera_errors':errors,'fov':90,'size':[1127,724],
            'before_sha256':sha(paths[0]),'after_sha256':sha(paths[1]),
            'mean_absolute_rgb_delta':float(delta.mean()),'pixels_delta_above_1pct':int((delta.max(2)>.01).sum()),
            'pixel_difference_scope':'render comparison only; wind/streaming/exposure may differ; preservation judged by actual state/hash checks'})
        yy=np.linspace(0,723,241).astype(int);xx=np.linspace(0,1126,376).astype(int)
        thumbnails.append(np.concatenate([old[yy][:,xx],new[yy][:,xx]],axis=1))
    sheets=[]
    for start in range(0,26,7):
        subset=thumbnails[start:start+7]
        h=len(subset)*249;canvas=np.full((h,752,4),.13,np.float32);canvas[:,:,3]=1
        for j,thumb in enumerate(subset):canvas[(len(subset)-1-j)*249:(len(subset)-1-j)*249+241]=thumb
        img=bpy.data.images.new('ComparisonSheet',width=752,height=h,alpha=True,float_buffer=False)
        img.pixels.foreach_set(canvas.ravel());img.file_format='PNG';path=UNIT/f'ComparisonSheet_{start//7+1:02}.png'
        img.filepath_raw=str(path);img.save();bpy.data.images.remove(img)
        sheets.append({'file':path.relative_to(ROOT).as_posix(),'rows_top_to_bottom':[r['name'] for r in rows[start:start+7]],
                       'columns':['V006 before','V007 after']})
    (UNIT/'ComparisonValidation_V007.json').write_text(json.dumps({'pass':True,'camera_count':26,'comparisons':rows,'contact_sheets':sheets},indent=1)+'\n',encoding='utf-8')
    page='''<!doctype html><html lang="ko"><meta charset="utf-8"><title>U5a-6 V006 → V007 실제26시점</title><style>body{background:#101721;color:#e6edf5;font:16px system-ui;margin:26px}p{line-height:1.6;max-width:1200px}a{color:#8dcaff}nav a{margin:6px}section{margin:35px 0}.pair{display:grid;grid-template-columns:1fr 1fr;gap:10px}img{width:100%}h1{font-size:27px}.tag{color:#a5b8cb}</style><h1>U5a-6 · V006 → V007 실제 동일26카메라 비교</h1><p>전후 모두 새로 촬영했다. 동일 위치/회전·FOV90°·1127×724이며 원래 UV/재질 슬롯·21소유자/26자세·기존 ED 외피를 유지했다. V007은 세 몸체의2,154정점만 국소 보정하고 하부/연결은 기존 메시를 참조한다. 표본/자기 교차 검사와 시각 채택·충돌/PIE를 구별한다. 외관은 사용자 검토 대기다.</p><nav>'''
    page+=' '.join(f'<a href="#{r["name"]}">{html.escape(r["name"])}</a>' for r in rows)+'</nav>'
    for r in rows:
        name=r['name'];page+=f'<section id="{name}"><h2>{html.escape(name)}</h2><p class="tag">왼쪽 V006 · 오른쪽 V007 · 동일 카메라/FOV/해상도</p><div class="pair"><img loading="lazy" src="Views_Before/{name.lower()}.png"><img loading="lazy" src="Views_After/{name.lower()}.png"></div></section>'
    page+='<p><a href="ComparisonValidation_V007.json">카메라/해상도 대조</a> · <a href="CandidateReview.html">면별 형상 검토</a></p></html>'
    (UNIT/'Comparison_V007.html').write_text(page,encoding='utf-8')
    print(json.dumps({'same_cameras':26,'maximum_camera_error':max(v for r in rows for v in r['camera_errors'].values()),'contact_sheets':4}))


if __name__=='__main__':main()
