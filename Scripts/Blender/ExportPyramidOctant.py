"""확정 원본에서 FBX와 조립 입력을 복원한다. 승인된 배치는 다시 찾지 않는다."""
import hashlib,json,shutil
from pathlib import Path
import bpy,bmesh

ROOT=Path(__file__).resolve().parents[2]
ART=ROOT/'Art/Octants/Pyramid_00'
VERSION='V058'
OUT=ROOT/'Saved/SliceOctant/Pyramid_00'/VERSION
manifest=json.loads((ART/f'Pyramid_00_{VERSION}_Manifest.json').read_text())
source=ART/f'Pyramid_00_{VERSION}.blend'
assert hashlib.sha256(source.read_bytes()).hexdigest()==manifest['source_blend_sha256'],'Approved source changed; validate a new version'
ns={'__file__':str(ROOT/'Scripts/Blender/PyramidOctantCommon.py'),'__name__':'pyramid_helpers'}
exec(compile(Path(ns['__file__']).read_text(encoding='utf-8'),ns['__file__'],'exec'),ns)
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(source))
scene=bpy.data.scenes['Pyramid_00_'+VERSION]
bpy.context.window.scene=scene
bm=bmesh.new(); bm.from_mesh(scene.objects['ProjectedEnvelope'].data)
assert ns['topology'](bm)==manifest['topology'],'Source topology changed'
bm.free()
for item in manifest['exports']:
    ns['export'](scene.objects[item['name']],item['role'])
    actual=ns['EXPORTS'][-1]
    assert actual['triangles']==item['triangles'],item['name']
    assert all(abs(a-b)<0.01 for side in ('min','max') for a,b in zip(actual['bounds_cm'][side],item['bounds_cm'][side])),item['name']
manifest['exports']=ns['EXPORTS']
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
for source_name,target in [('Placement.json','placement.json'),
        (f'Pyramid_00_{VERSION}_GeometryProof.json','geometry-verification.json'),
        (f'Pyramid_00_{VERSION}_RouteProof.json','rupture-anchor-verification.json'),
        (f'Pyramid_00_{VERSION}_FacadeProof.json','facade-proof.json')]:
    shutil.copy2(ART/source_name,OUT/target)
print('PYRAMID_FINAL_EXPORT_READY',sum(e['triangles'] for e in ns['EXPORTS']))
