"""라이브 ProgrammaticToolset에서 최신 풀 정의를 보존하고 준비된 Grove를 한 번 등록한다."""
import json

POOL='/Game/Maps/OctantPoolData'
OUT='D:/UnrealProjects/LootNPop/Saved/SliceOctant/Grove_00/Production/'
def call(name,args):return execute_tool(name,json.dumps(args))['returnValue']
def run():
    assert not call('EditorToolset.EditorAppToolset.IsPIERunning',{})
    assert not call('editor_toolset.toolsets.asset.AssetTools.is_dirty',{'asset_path':POOL})
    report=json.loads(call('editor_toolset.toolsets.asset.AssetTools.read_file',{'file_path':OUT+'integration-ready.json'}))
    pool=call('editor_toolset.toolsets.asset.AssetTools.load_asset',{'asset_path':POOL})
    schema=json.loads(call('editor_toolset.toolsets.object.ObjectTools.list_properties',{'instance':pool}))
    assert 'octantDefinitions' in schema and 'octantPool' in schema
    def read():return json.loads(call('editor_toolset.toolsets.object.ObjectTools.get_properties',{'instance':pool,'properties':['octantDefinitions','octantPool']}))
    before=read();definitions=before['octantDefinitions']
    assert len(definitions)==report['definition_count']-1
    assert all('/Grove_00/' not in d['levelAsset'] for d in definitions)
    level=call('editor_toolset.toolsets.asset.AssetTools.load_asset',{'asset_path':'/Game/Maps/Grove_00/LVI_Octant_Grove_00'})
    surface=call('editor_toolset.toolsets.asset.AssetTools.load_asset',{'asset_path':'/Game/Maps/Grove_00/DA_OctantSurface_Grove_00'})
    new={'levelAsset':level,'surfaceData':surface,'allowedSlotRotations':255,'seamSignature':definitions[0]['seamSignature']}
    assert call('editor_toolset.toolsets.object.ObjectTools.set_properties',{'instance':pool,'values':json.dumps({'octantDefinitions':definitions+[new]})})
    after=read()
    assert after['octantDefinitions'][:-1]==definitions and after['octantPool']==before['octantPool']
    assert after['octantDefinitions'][-1]['allowedSlotRotations']==255
    assert call('editor_toolset.toolsets.asset.AssetTools.save_assets',{'asset_paths':[POOL]})
    report.update({'pool_registered':True,'pool_properties_before':before,'pool_properties_after':after})
    call('editor_toolset.toolsets.asset.AssetTools.write_file',{'file_path':OUT+'integration.json','content':json.dumps(report,indent=2)})
    return {'definitions':len(after['octantDefinitions']),'previous_definitions_preserved':len(definitions),'saved':True}
