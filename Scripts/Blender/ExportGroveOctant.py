"""확정 V040 형상과 V042 배치에서 반입 입력을 복원한다. 지형을 다시 설계하지 않는다."""
import hashlib,json,shutil,sys
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parents[2]
ART=ROOT/'Art/Octants/Grove_00'
OUT=ROOT/'Saved/SliceOctant/Grove_00/ProductionExport'
manifest=json.loads((ART/'Grove_00_V042_Manifest.json').read_text(encoding='utf-8'))
source=ART/'Grove_00_V040.blend'
assert hashlib.sha256(source.read_bytes()).hexdigest()==manifest['source_blend_sha256'],'Approved Grove source changed'
sys.path.insert(0,str(ROOT/'Scripts/Blender'))
import BuildGroveOctantV002 as geometry
geometry.OUT=OUT;geometry.EXPORTS=[]
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(source))
for item in manifest['exports']:
    geometry.export(bpy.data.objects[item['name']],item['role'])
    actual=geometry.EXPORTS[-1]
    assert actual['triangles']==item['triangles'],item['name']
    assert all(abs(a-b)<.01 for side in ('min','max') for a,b in zip(actual['bounds_cm'][side],item['bounds_cm'][side])),item['name']
manifest['exports']=geometry.EXPORTS
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
for source_name,target in (('Placement.json','placement.json'),('UndergroundLights.json','underground-lights.json'),('Grove_00_V042_GeometryProof.json','geometry-verification.json')):
    shutil.copy2(ART/source_name,OUT/target)
print('GROVE_FINAL_EXPORT_READY',sum(e['triangles'] for e in geometry.EXPORTS))
