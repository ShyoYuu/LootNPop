"""원본 검사 후 별도 에디터 프로세스에서 삼각쌍뿔을 조립한다.

새 옥탄트만 수정한다. 마커 재사용으로 ID를 보존하고 저장한다.
새 패키지의 Asset Registry 정보가 확정된 뒤 RunPyramidValidation.ps1에서 별도로 베이크한다.
"""

import itertools
import gc
import json
import math
import struct
from pathlib import Path

import unreal
from editor_toolset.toolsets.scene import SceneTools

ROOT=Path(unreal.Paths.project_dir()).resolve()
VERSION="V058"
OUT=ROOT/"Saved/SliceOctant/Pyramid_00"/VERSION
ART=ROOT/"Art/Octants/Pyramid_00"
BASE="/Game/Maps/Pyramid_00"
LEVEL=BASE+"/LVI_Octant_Pyramid_00"
SURFACE=BASE+"/DA_OctantSurface_Pyramid_00"
STAGING="/Game/Maps/_WorkInProgress/Pyramid_00/"+VERSION
manifest=json.loads((OUT/"manifest.json").read_text(encoding="utf-8"))
geometry=json.loads((OUT/"geometry-verification.json").read_text(encoding="utf-8"))
assert not geometry["ramp_failures"],"Source ramp verification failed"
assert not geometry['panel_failures'],'Source panel clearance verification failed'
placement=json.loads((OUT/"placement.json").read_text(encoding="utf-8"))
helpers={"__name__":"pyramid_transform_helpers"}
exec(compile((ROOT/"Scripts/AssembleAcropolisOctant.py").read_text(encoding="utf-8"),
             "AssembleAcropolisOctant.py","exec"),helpers)


def point(local):
    return unreal.Vector(**helpers["transform"](local)["location"])


def pose(local):
    value=helpers["transform"](local)
    return unreal.Vector(**value["location"]),unreal.Rotator(**value["rotation"])


def write(name,value):
    (OUT/name).write_text(json.dumps(value,indent=2),encoding="utf-8")


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()


def production_path(item):
    if item['role']=='Crust':
        return BASE+'/'+item['name']
    return BASE+'/Authored/'+item['name'].replace('SM_Pyramid_00_','SM_Pyramid_00_'+VERSION+'_')


def native_mesh(asset):
    mesh=unreal.DynamicMesh()
    options=unreal.GeometryScriptCopyMeshFromAssetOptions()
    options.apply_build_settings=False
    options.request_tangents=False
    options.use_build_scale=False
    lod=unreal.GeometryScriptMeshReadLOD()
    lod.lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL
    mesh,outcome=unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(asset,mesh,options,lod)
    assert outcome==unreal.GeometryScriptOutcomePins.SUCCESS
    mesh,positions,gaps=unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh,False)
    assert not gaps
    return mesh,list(unreal.GeometryScript_List.convert_vector_list_to_array(positions))


def snap_seam(target):
    reference=native_mesh(unreal.load_asset("/Game/Maps/Meshes/SM_Octant_Meadow_00_R30000"))[1]
    mesh,positions=native_mesh(target)
    def key(p):
        return tuple(round(v*10) for v in (p.x,p.y,p.z))
    lookup={}
    for p in reference:
        if min(p.x,p.y,p.z)<=1500.1:
            lookup.setdefault(key(p),[]).append(p)
    count,maximum=0,0
    for i,p in enumerate(positions):
        if min(p.x,p.y,p.z)>1500.01:
            continue
        k=key(p)
        candidates=[q for d in itertools.product((-1,0,1),repeat=3)
                    for q in lookup.get(tuple(a+b for a,b in zip(k,d)),[])]
        assert candidates,("Missing native seam vertex",p)
        q=min(candidates,key=lambda q:(p-q).length_squared())
        error=(p-q).length()
        assert error<0.05,error
        positions[i]=q
        maximum=max(maximum,error)
        count+=1
    unreal.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(mesh,
        unreal.GeometryScript_List.convert_array_to_vector_list(positions))
    options=unreal.GeometryScriptCopyMeshToAssetOptions()
    options.enable_recompute_normals=False
    options.enable_recompute_tangents=False
    options.use_build_scale=False
    mesh,outcome=unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh,target,options,unreal.GeometryScriptMeshWriteLOD())
    assert outcome==unreal.GeometryScriptOutcomePins.SUCCESS
    return {"vertices":count,"max_correction_cm":maximum}


def import_assets():
    result=[]
    for item in manifest["exports"]:
        final=production_path(item)
        asset_name=final.rsplit('/',1)[1]
        existing=unreal.EditorAssetLibrary.does_asset_exist(final)
        path=final if existing else STAGING+"/"+asset_name
        if existing or not unreal.EditorAssetLibrary.does_asset_exist(path):
            task=unreal.AssetImportTask()
            task.filename=item["file"]
            task.destination_path=final.rsplit("/",1)[0] if existing else STAGING
            task.destination_name=asset_name
            task.automated=True
            task.replace_existing=existing
            task.replace_existing_settings=existing
            task.save=True
            options=unreal.FbxImportUI()
            options.automated_import_should_detect_type=False
            options.import_mesh=True
            options.import_as_skeletal=False
            options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
            options.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH
            options.import_materials=False
            options.import_textures=False
            options.import_animations=False
            options.static_mesh_import_data.combine_meshes=True
            options.static_mesh_import_data.remove_degenerates=False
            task.options=options
            task.factory=unreal.FbxFactory()
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        if not existing:
            asset=unreal.load_asset(path)
            assert asset and asset.get_num_triangles(0)==item["triangles"],path
            assert unreal.EditorAssetLibrary.rename_asset(path,final),final
        asset=unreal.load_asset(final)
        assert asset.get_num_triangles(0)==item["triangles"],final
        body=asset.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag",unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        body.set_editor_property("double_sided_geometry",False)
        asset.set_editor_property("lod_for_collision",0)
        seam=snap_seam(asset) if item["role"]=="Crust" else None
        asset.get_editor_property("body_setup").set_editor_property("collision_trace_flag",unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        save(asset)
        positions=native_mesh(asset)[1]
        bounds={side:[function(getattr(p,axis) for p in positions) for axis in ('x','y','z')]
                for side,function in (('min',min),('max',max))}
        error=max(abs(a-b) for side in bounds for a,b in zip(bounds[side],item['bounds_cm'][side]))
        assert error<0.05,('Native bounds differ',final,error)
        result.append({"path":final,"triangles":asset.get_num_triangles(0),"native_seam":seam,
                       'bounds_cm':bounds,'max_bounds_error_cm':error})
    write("native-import.json",result)


def assemble():
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        # 반입/이름 변경 중 참조 정리가 로드된 World 패키지를 dirty로 만들 수 있다.
        assert unreal.EditorAssetLibrary.save_asset(LEVEL,only_if_is_dirty=False)
        assert levels.load_level(LEVEL)
    else:
        assert levels.new_level(LEVEL)
    by_label={a.get_actor_label():a for a in actors.get_all_level_actors()}
    def spawn(cls,label,local,folder):
        actor=by_label.get(label)
        if actor is None:
            actor=actors.spawn_actor_from_class(cls,*pose(local))
            actor.set_actor_label(label)
        else:
            actor.set_actor_location_and_rotation(*pose(local),False,False)
        actor.set_folder_path(folder)
        return actor
    for item in manifest["exports"]:
        label="Crust" if item["role"]=="Crust" else item["name"].replace("SM_Pyramid_00_","")
        actor=spawn(unreal.StaticMeshActor,label,[0,0,0],"Geometry")
        actor.set_actor_transform(unreal.Transform(),False,False)
        component=actor.static_mesh_component
        path=production_path(item)
        component.set_static_mesh(unreal.load_asset(path))
        support=item["role"]!="Shell"
        tags=["LNP.Surface.Blocker","LNP.Surface.Static"]
        if support:
            tags.append("LNP.Surface.Support")
        if item["role"] in ("Crust","Coarse"):
            tags.append("LNP.Surface.CoarseSupport")
        component.set_editor_property("component_tags",tags)
        component.set_editor_property("use_default_collision",False)
        component.set_collision_profile_name("LNPStaticTerrain" if support else "LNPStaticBlocker")
        component.set_mobility(unreal.ComponentMobility.STATIC)
        material="M_Acropolis_Ground" if item["role"]=="Crust" else "M_Acropolis_Stone"
        component.set_material(0,unreal.load_asset("/Game/Maps/Acropolis_00/"+material))
    anchor_class=unreal.load_class(None,"/Game/WorldDevice/BP_GrappleAnchor.BP_GrappleAnchor_C")
    panel_path=BASE+"/BP_PyramidMovingPanel"
    if not unreal.EditorAssetLibrary.does_asset_exist(panel_path):
        assert unreal.EditorAssetLibrary.duplicate_asset("/Game/Maps/Hollow_00/BP_HollowMovingPanel",panel_path)
    panel_asset=unreal.load_asset(panel_path)
    unreal.BlueprintEditorLibrary.compile_blueprint(panel_asset)
    save(panel_asset)
    panel_class=unreal.load_class(None,panel_path+".BP_PyramidMovingPanel_C")
    exterior_path=BASE+'/BP_PyramidExteriorPanel'
    if not unreal.EditorAssetLibrary.does_asset_exist(exterior_path):
        assert unreal.EditorAssetLibrary.duplicate_asset(panel_path,exterior_path)
    exterior_asset=unreal.load_asset(exterior_path)
    unreal.BlueprintEditorLibrary.compile_blueprint(exterior_asset)
    exterior_class=unreal.load_class(None,exterior_path+'.BP_PyramidExteriorPanel_C')
    # 실내 12m 판과 외부 24m 판은 기본값만 다르며 경로/중력 회전 코드는 공통이다.
    unreal.get_default_object(exterior_class).get_editor_property('panel_mesh').set_relative_scale3d(unreal.Vector(24,24,0.3))
    save(exterior_asset)
    for item in placement["anchors"]:
        actor=spawn(unreal.LNPPlacementMarker,"Anchor_"+item["id"],item["local"],"Markers/Grapple")
        actor.set_editor_property("element_class",anchor_class)
        actor.set_editor_property("grapple_max_distance",item["max_distance"])
        actor.set_editor_property("tags",["LNP.Grapple.AuthoredOnly"])
    for item in placement["pods"]:
        actor=spawn(unreal.LNPMassSpawnPoint,"Spawn_"+item["id"],item["local"],"Markers/Pods")
        actor.set_editor_property("target_spawn_set_id","None")
        formations=[]
        for values in item.get('enemy_formations',[]):
            formation=unreal.LNPSpawnEnemyFormation()
            formation.set_editor_property('enemy_config_path',values['enemyConfigPath'])
            formation.set_editor_property('count',values['count'])
            formation.set_editor_property('offset',unreal.Vector2D(**values['offset']))
            formation.set_editor_property('radius',values['radius'])
            formations.append(formation)
        actor.set_editor_property('enemy_formations',formations)
    for item in placement["panels"]:
        middle=[(a+b)/2 for a,b in zip(item["from"],item["to"])]
        actor=spawn(unreal.LNPPlacementMarker,"Panel_"+item["id"],middle,"Markers/Panels")
        actor.set_editor_property("element_class",exterior_class if item.get('external',False) else panel_class)
        actor.set_editor_property("path_speed",item["speed"])
        actor.set_editor_property("path_end_hold_seconds",2.5)
        spline=actor.get_editor_property("path")
        spline.clear_spline_points(False)
        for i,local in enumerate((item["from"],item["to"])):
            spline.add_spline_point(point(local),unreal.SplineCoordinateSpace.WORLD,False)
            spline.set_spline_point_type(i,unreal.SplinePointType.LINEAR,False)
        spline.set_closed_loop(False,False)
        spline.update_spline()
    for item in placement["reviews"]:
        actor=spawn(unreal.TargetPoint,"Review_"+item["id"],item["local"],"Markers/Review")
        actor.set_editor_property("tags",["LNP.ReviewPoint",item["id"]])
        actor.set_editor_property("is_editor_only_actor",True)
    # 새 설계에 없는 이전 형상/마커는 제거한다. 다른 맵에는 접근하지 않는다.
    labels={'Crust'} | {e['name'].replace('SM_Pyramid_00_','') for e in manifest['exports']}
    labels |= {'Anchor_'+e['id'] for e in placement['anchors']}
    labels |= {'Spawn_'+e['id'] for e in placement['pods']}
    labels |= {'Panel_'+e['id'] for e in placement['panels']}
    labels |= {'Review_'+e['id'] for e in placement['reviews']}
    for label,actor in by_label.items():
        if label not in labels and (isinstance(actor,unreal.StaticMeshActor) or
                label.startswith(('Anchor_','Spawn_','Panel_','Review_'))):
            actors.destroy_actor(actor)
    assert levels.save_current_level()


def verify():
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    by_label={a.get_actor_label():a for a in actors.get_all_level_actors()}
    positions=[by_label['Spawn_'+p['id']].get_actor_location() for p in placement['pods']]
    spacing=min((a-b).length() for i,a in enumerate(positions) for b in positions[i+1:])
    assert spacing>=2000,('Native authored pod spacing',spacing)
    floor_rays=[]
    for item in placement["pods"]:
        x,y,h=item["local"]
        scale=math.sqrt(30000**2+x*x+y*y)/(30000-h)
        radii=(0,150*scale,250*scale)
        for radius in radii:
            for i in range(16 if radius else 1):
                a=i*math.tau/16
                xx,yy=x+radius*math.cos(a),y+radius*math.sin(a)
                hit=SceneTools.trace_world(point([xx,yy,h+50]),point([xx,yy,h-100]))
                assert hit is not None and abs(hit-50)<6,("Pod floor",item["id"],hit)
                floor_rays.append(hit)
        for radius in (0,200*scale):
            for i in range(8 if radius else 1):
                a=i*math.tau/8
                xx,yy=x+radius*math.cos(a),y+radius*math.sin(a)
                assert SceneTools.trace_world(point([xx,yy,h+100]),point([xx,yy,h+2635])) is None,("Pod headroom",item['id'])
    ramp_rays=0
    for ramp in placement["ramps"]:
        for a,b in zip(ramp["path"],ramp["path"][1:]):
            dx,dy=b[0]-a[0],b[1]-a[1]
            length=math.hypot(dx,dy)
            for i in range(21):
                local=[x+(y-x)*i/20 for x,y in zip(a,b)]
                for side in (-ramp['width_cm']*0.30,0,ramp['width_cm']*0.30):
                    x,y,h=local[0]-dy/length*side,local[1]+dx/length*side,local[2]
                    hit=SceneTools.trace_world(point([x,y,h+50]),point([x,y,h-100]))
                    assert hit is not None and abs(hit-50)<8,("Ramp floor",ramp["id"],hit)
                    assert SceneTools.trace_world(point([x,y,h+180]),point([x,y,h+450])) is None,("Ramp headroom",ramp["id"],local)
                    ramp_rays+=1
    route_proof=json.loads((OUT/'rupture-anchor-verification.json').read_text(encoding='utf-8'))
    facade_proof=json.loads((OUT/'facade-proof.json').read_text(encoding='utf-8'))
    regional={a['id']:a for a in placement['anchors'] if a.get('route_group')}
    expected={name for region in route_proof['coverage'] for name in region['anchors']}
    assert set(regional)==expected,'Regional anchors lost during placement'
    assert len([a for a in regional.values() if a['region_kind']=='hall'])==26

    def check_floor(local,radius,label):
        x,y,h=local
        scale=math.sqrt(30000**2+x*x+y*y)/(30000-h)
        for r in (0,radius*scale):
            for i in range(8 if r else 1):
                a=i*math.tau/8
                xx,yy=x+r*math.cos(a),y+r*math.sin(a)
                hit=SceneTools.trace_world(point([xx,yy,h+60]),point([xx,yy,h-150]))
                assert hit is not None and abs(hit-60)<9,('Route floor',label,hit)
        assert SceneTools.trace_world(point([x,y,h+20]),point([x,y,h+300])) is None,('Standing clearance',label)

    def check_corridor(source_local,target_local,radius,label):
        source,target=point(source_local),point(target_local)
        delta=target-source
        forward=delta*(1/delta.length())
        up=target*(-1/target.length())
        side=unreal.Vector(forward.y*up.z-forward.z*up.y,
                           forward.z*up.x-forward.x*up.z,
                           forward.x*up.y-forward.y*up.x)
        side=side*(radius/side.length())
        up=up*(radius*0.7)
        for offset in (unreal.Vector(),side,-side,up,-up):
            assert SceneTools.trace_world(source+offset,target+offset) is None,('Route corridor',label)
        return delta.length()

    for item in regional.values():
        check_floor(item['landing'],250,item['id'])
        target=point(item['local'])
        assert (by_label['Anchor_'+item['id']].get_actor_location()-target).length()<0.1
        x,y,h=item['landing']
        assert SceneTools.trace_world(point([x,y,h+15]),target) is None,('Rupture vertical clearance',item['id'])
    for route in route_proof['routes']:
        label=route['from']+'->'+route['to']
        target=regional[route['to']]
        assert route['target']==target['local']
        if route['from'] in regional:
            source=regional[route['from']]
            assert source['landing']==route['source_landing']
            assert SceneTools.trace_world(point(source['local']),point(target['local'])) is None,('Anchor LOS',label)
        for local in route['walk']:
            check_floor(local,75,label)
        a,b=route['walk'][0],route['walk'][-1]
        if (point(a)-point(b)).length()>1:
            check_corridor([a[0],a[1],a[2]+110],[b[0],b[1],b[2]+110],70,label)
        distance=check_corridor(route['origin'],target['local'],100,label)
        assert 1000<=distance<target['max_distance'],('Grapple range',label,distance)
    for region in route_proof['coverage']:
        nodes=set(region['anchors'])
        for start in nodes:
            reached={start}
            while True:
                expanded=reached|{r['to'] for r in region['routes'] if r['from'] in reached and r['to'] in nodes}
                if expanded==reached:
                    break
                reached=expanded
            assert reached==nodes,('Bottom-free route connectivity',region['route_group'],start)
    assert not facade_proof['failures'] and len(facade_proof['exterior_access'])==6
    for access in facade_proof['exterior_access']:
        for local in access['walk']:
            check_floor(local,100,access['ledge'])
        a,b=access['walk'][0],access['walk'][-1]
        assert SceneTools.trace_world(point([*a[:2],a[2]+200]),point([*b[:2],b[2]+200])) is None,('Exterior access',access['ledge'])
    report={"native_pod_floor_rays":len(floor_rays),"max_floor_error_cm":max(abs(h-50) for h in floor_rays),
            'native_pod_min_spacing_cm':spacing,
            "native_ramp_floor_rays":ramp_rays,"native_ramp_headroom_rays":ramp_rays,
            "geometry_actors":len(manifest["exports"]),"panels":len(placement["panels"]),
            'regional_anchors':list(regional),'native_anchor_landing_rays':len(regional)*9,
            'native_route_corridors':len(route_proof['routes']),
            'native_route_approach_rays':len(route_proof['routes'])*5,
            'native_route_walk_floor_rays':sum(len(r['walk'])*9 for r in route_proof['routes']),
            'native_exterior_walk_floor_rays':sum(len(a['walk'])*9 for a in facade_proof['exterior_access']),
            'bottom_free_regions':len([r for r in route_proof['coverage'] if r['anchors']]),
            "limits":["Grapple momentum, capsule traversal and Mover boarding require user PIE"]}
    write("native-verification.json",report)
    return report


def archive_previous_meshes():
    # Authored 바로 아래에는 채택된 메시만 남긴다. 이전 버전은 로컬 복구 폴더로 이동한다.
    for path in unreal.EditorAssetLibrary.list_assets(BASE+'/Authored',recursive=False,include_folder=False):
        name=path.rsplit('/',1)[1].split('.')[0]
        if name.startswith('SM_Pyramid_00_V') and not name.startswith('SM_Pyramid_00_'+VERSION+'_'):
            if unreal.AssetRegistryHelpers.is_redirector(unreal.EditorAssetLibrary.find_asset_data(path)):
                continue
            if not isinstance(unreal.load_asset(path),unreal.StaticMesh):
                continue
            version=name[len('SM_Pyramid_00_'):].split('_')[0]
            destination='/Game/Maps/_WorkInProgress/Pyramid_00/'+version+'/'+name
            assert unreal.EditorAssetLibrary.rename_asset(path,destination),(path,destination)
            assert unreal.EditorAssetLibrary.save_asset(destination,only_if_is_dirty=False)
    assert unreal.EditorAssetLibrary.save_asset(LEVEL,only_if_is_dirty=False)


def audit_final_assets():
    # 구버전 리다이렉터는 프로세스가 닫힌 뒤 복구 폴더로 보관한다.
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([BASE],True)
    options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
    assets=registry.get_assets_by_path(BASE,recursive=True)
    retired={str(a.package_name) for a in assets if unreal.AssetRegistryHelpers.is_redirector(a)}
    for package in retired:
        external=[str(p) for p in registry.get_referencers(package,options)
                  if str(p) not in retired and not str(p).startswith('/Game/Maps/_WorkInProgress/')]
        assert not external,('Old redirector still referenced',package,external)
    pending=[str(a.package_name) for a in assets if str(a.package_name) not in retired]
    visited=set()
    while pending:
        package=pending.pop()
        if package in visited:
            continue
        visited.add(package)
        for dependency in registry.get_dependencies(package,options):
            name=str(dependency)
            assert not name.startswith('/Game/Maps/_WorkInProgress/'),('Production work dependency',package,name)
            if name.startswith('/Game/'):
                pending.append(name)
    write('asset-finalization.json',{'retired_redirectors':sorted(retired),
          'checked_dependency_packages':len(visited),'work_dependencies':[]})


def bake():
    # 신규 저장·이동 패키지의 디스크 메타데이터를 수집한 뒤 source hash를 만든다.
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([BASE],True)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    surface_file=ROOT/"Content/Maps/Pyramid_00/DA_OctantSurface_Pyramid_00.uasset"
    before=surface_file.stat().st_mtime_ns if surface_file.exists() else None
    unreal.SystemLibrary.execute_console_command(world,"LNP.SurfaceNav.BakeOctant "+LEVEL)
    assert surface_file.exists() and surface_file.stat().st_mtime_ns!=before,"Bake did not save SurfaceData"
    data=unreal.load_asset(SURFACE)
    layer_count=struct.unpack_from('<H',bytes(data.get_editor_property('support_payload')),2)[0]
    assert layer_count<=256,('Runtime Support/Nav layer limit exceeded',layer_count)
    helper={"__name__":"pyramid_seam_helpers"}
    exec(compile((ROOT/"Scripts/InspectAcropolisBake.py").read_text(encoding="utf-8"),"InspectAcropolisBake.py","exec"),helper)
    reference=helper["seams"]("/Game/Maps/Meadow_00/DA_OctantSurface_Meadow_00")
    current=helper["seams"](SURFACE)
    assert current==reference,"Baked seam differs from Meadow"
    header=data.get_editor_property("header")
    streams={name:{"elements":header.get_editor_property(name).get_editor_property("element_count"),
                   "bytes":header.get_editor_property(name).get_editor_property("uncompressed_size")}
             for name in ("support","navigation","traversal","spawn","conditional_patch")}
    payload=bytes(data.get_editor_property('spawn_payload'))
    version,reserved,authored,random_count=struct.unpack_from('<HHII',payload)
    assert version==3
    offset=12
    for _ in range(authored):
        offset+=16
        length=struct.unpack_from('<i',payload,offset)[0]
        offset+=4+length+28+16
        formations=struct.unpack_from('<I',payload,offset)[0]
        offset+=4
        for _ in range(formations):
            length=struct.unpack_from('<i',payload,offset)[0]
            offset+=4+length+24
    heights=[]
    for _ in range(random_count):
        x,y,z=struct.unpack_from('<fff',payload,offset+4)
        heights.append(30000-math.sqrt(x*x+y*y+z*z))
        offset+=40
    assert offset==len(payload)
    # 최저 승강기 피트는 첫 층보다 120cm 낮다. 원본의 Shaft 절단 깊이와 맞춘다.
    lowest_floor=min(manifest['levels_cm'])-120
    assert min(heights)>=lowest_floor-5,('Spawn candidate below lowest shaft floor',min(heights))
    pool=unreal.load_asset("/Game/Maps/OctantPoolData")
    definitions=[d for d in pool.get_editor_property("octant_definitions")
                 if 'LVI_Octant_Pyramid_00' not in d.export_text()]
    definition=unreal.LNPOctantDefinition()
    definition.set_editor_property("level_asset",unreal.load_asset(LEVEL))
    definition.set_editor_property("surface_data",data)
    definition.set_editor_property("allowed_slot_rotations",255)
    definition.set_editor_property("seam_signature",definitions[0].get_editor_property("seam_signature"))
    for name,items in (("Mixed",definitions+[definition]),("Only",[definition])):
        path=BASE.replace("/Pyramid_00","/_WorkInProgress/Pyramid_00/Validation")+"/DA_Pyramid"+name+"ValidationPool"
        validation=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset("/Game/Maps/OctantPoolData",path)
        validation.set_editor_property("octant_definitions",items)
        save(validation)
    (ART/"OctantDefinition.txt").write_text(definition.export_text(),encoding="utf-8")
    write("bake-summary.json",{"streams":streams,"matching_seam_samples":sum(map(len,current)),
                               "support_layers":layer_count,
                               "random_candidate_height_cm":{"min":min(heights),"max":max(heights)},
                               "level_asset":LEVEL,"surface_data":SURFACE,"production_pool_changed":False})
    unreal.log("PYRAMID_BAKE_READY "+json.dumps(streams))


if __name__=="__main__":
    import_assets()
    assemble()
    archive_previous_meshes()
    # 액터/컴포넌트의 Python 지역 참조가 사라진 뒤 저장된 맵을 다시 연다.
    gc.collect()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
    verify()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/Maps/TestMap03")
    unreal.log("PYRAMID_ASSEMBLY_READY")
