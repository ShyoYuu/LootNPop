"""라이브 ProgrammaticToolset에서 지정한 Grove 공동 보조 조명만 추가한다."""
import json
import math

LEVEL='/Game/Maps/_WorkInProgress/Grove_00/V042/LVI_Octant_Grove_00'
OUT='D:/UnrealProjects/LootNPop/Saved/SliceOctant/Grove_00/V042/CavernFill02/'
EXPECTED_OLD_LIGHTS=45

def call(name,args):return execute_tool(name,json.dumps(args))
def find(cls):return call('editor_toolset.toolsets.scene.SceneTools.find_actors',{'name':'','tag':'','collision_channels':[],'actor_type':{'refPath':cls}})['returnValue']
def components(actor):return call('editor_toolset.toolsets.actor.ActorTools.get_components',{'actor':actor,'component_type':{'refPath':'/Script/Engine.PointLightComponent'}})['returnValue']
def get(obj,names):return json.loads(call('editor_toolset.toolsets.object.ObjectTools.get_properties',{'instance':obj,'properties':names})['returnValue'])
def set_props(obj,values):assert call('editor_toolset.toolsets.object.ObjectTools.set_properties',{'instance':obj,'values':json.dumps(values)})['returnValue']
def pose(actor):return call('editor_toolset.toolsets.actor.ActorTools.get_actor_transform',{'actor':actor})['returnValue']
def point(local):
    x,y,h=local
    p=[30000/math.sqrt(3)-x/math.sqrt(6)-y/math.sqrt(2),30000/math.sqrt(3)-x/math.sqrt(6)+y/math.sqrt(2),30000/math.sqrt(3)+2*x/math.sqrt(6)]
    k=(30000-h)/math.sqrt(sum(v*v for v in p))
    return dict(zip(('x','y','z'),[v*k for v in p]))
def trace(a,b):return call('editor_toolset.toolsets.scene.SceneTools.trace_world',{'start':point(a),'end':point(b)})['returnValue']
def write(name,data):call('editor_toolset.toolsets.asset.AssetTools.write_file',{'file_path':OUT+name,'content':json.dumps(data,indent=2)})

def run():
    assert call('editor_toolset.toolsets.scene.SceneTools.get_current_level',{})['returnValue']==LEVEL
    assert not call('EditorToolset.EditorAppToolset.IsPIERunning',{})['returnValue']
    old=find('/Script/Engine.Actor');before={a['refPath']:pose(a) for a in old}
    old_lights=find('/Script/Engine.PointLight');assert len(old_lights)==EXPECTED_OLD_LIGHTS
    fields=['mobility','intensityUnits','bUseInverseSquaredFalloff','castShadows','lightColor','sourceRadius','intensity','attenuationRadius']
    template_component=components(old_lights[0])[0]
    schema=json.loads(call('editor_toolset.toolsets.object.ObjectTools.list_properties',{'instance':template_component})['returnValue'])
    assert all(k in schema for k in fields)
    old_values={a['refPath']:get(components(a)[0],fields) for a in old_lights}
    template=old_values[old_lights[0]['refPath']]
    lights=json.loads(call('editor_toolset.toolsets.asset.AssetTools.read_file',{'file_path':OUT+'lights.json'})['returnValue'])
    added=[]
    for item in lights:
        for axis in range(3):
            a=list(item['local']);b=list(a);a[axis]-=150;b[axis]+=150
            assert trace(a,b) is None,('Light embedded in geometry',item['id'])
        actor=call('editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_class',{'actor_type':{'refPath':'/Script/Engine.PointLight'},'name':'ReviewLight_'+item['id'],'xform':{'location':point(item['local'])},'snap_to_ground':False})['returnValue']
        assert actor
        assert call('editor_toolset.toolsets.actor.ActorTools.set_label',{'actor':actor,'label':'ReviewLight_'+item['id']})['returnValue']
        call('editor_toolset.toolsets.scene.SceneTools.set_actor_folder',{'actor':actor,'folder_path':'Lighting/UndergroundReview'})
        c=components(actor)[0]
        set_props(c,{k:template[k] for k in fields if k not in ('intensity','attenuationRadius')})
        set_props(c,{'intensity':item['lumens'],'attenuationRadius':item['radius']})
        values=get(c,fields)
        assert values['mobility']=='Movable' and not values['castShadows'] and values['intensityUnits']=='Lumens'
        assert values['intensity']==item['lumens'] and values['attenuationRadius']==item['radius']
        added.append({'id':item['id'],'actor':actor,'local':item['local'],'values':values})
    total=EXPECTED_OLD_LIGHTS+len(lights)
    assert len(find('/Script/Engine.PointLight'))==total
    assert len(find('/Script/LootNPop.LNPPlacementMarker'))==30
    assert len(find('/Script/LootNPop.LNPMassSpawnPoint'))==8
    assert all(pose(a)==before[a['refPath']] for a in old)
    assert all(get(components(a)[0],fields)==old_values[a['refPath']] for a in old_lights)
    assert call('editor_toolset.toolsets.asset.AssetTools.save_assets',{'asset_paths':[LEVEL]})['returnValue']
    report={'added':added,'total_lights':total,'previous_lights_unchanged':EXPECTED_OLD_LIGHTS,'protected_actor_transforms':len(old),'anchors':30,'pods':8,'saved':True}
    write('application.json',report)
    return {'added_lights':len(added),'total_lights':total,'protected_actor_transforms':len(old),'old_lights_unchanged':EXPECTED_OLD_LIGHTS,'saved':True}
