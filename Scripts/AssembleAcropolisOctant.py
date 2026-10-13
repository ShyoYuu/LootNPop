"""Unreal MCP ProgrammaticToolset에서 실행하는 Acropolis 전용 조립 원본.

execute_tool_script에 이 파일 내용을 전달한다. ACTION과 START/END만 바꾸어
반입 묶음과 조립을 재실행할 수 있다. 다른 옥탄트나 풀 에셋은 수정하지 않는다.
"""

import json
import math


ACTION = "import"
START = 0
END = 63
BASE = "/Game/Maps/Acropolis_00"
LEVEL = BASE + "/LVI_Octant_Acropolis_00"
SAVED = "D:/UnrealProjects/LootNPop/Saved/SliceOctant/Acropolis_00/V015/"


def read_json(name):
    return json.loads(execute_tool("editor_toolset.toolsets.asset.AssetTools.read_file",
        json.dumps({"file_path": SAVED+name}))["returnValue"])


def write_json(name, data):
    return execute_tool("editor_toolset.toolsets.asset.AssetTools.write_file",
        json.dumps({"file_path":SAVED+name,"content":json.dumps(data,indent=2)}))


def exists(path):
    return execute_tool("editor_toolset.toolsets.asset.AssetTools.exists",json.dumps({"path":path}))["returnValue"]


def import_mesh(item, folder):
    return execute_tool("editor_toolset.toolsets.static_mesh.StaticMeshTools.import_file",
        json.dumps({"folder_path":folder,"asset_name":item["name"],"source_file":item["file"],
                    "import_materials":False,"import_textures":False,"combine_meshes":True}))["returnValue"][0]


def list_props(obj):
    return json.loads(execute_tool("editor_toolset.toolsets.object.ObjectTools.list_properties",
        json.dumps({"instance":obj}))["returnValue"])


def get_props(obj, names):
    return json.loads(execute_tool("editor_toolset.toolsets.object.ObjectTools.get_properties",
        json.dumps({"instance":obj,"properties":names}))["returnValue"])


def set_props(obj, values):
    assert execute_tool("editor_toolset.toolsets.object.ObjectTools.set_properties",
        json.dumps({"instance":obj,"values":json.dumps(values)}))["returnValue"]


def bounds(obj):
    return execute_tool("editor_toolset.toolsets.static_mesh.StaticMeshTools.get_bounds",json.dumps({"mesh":obj}))["returnValue"]


def triangles(obj):
    return execute_tool("editor_toolset.toolsets.static_mesh.StaticMeshTools.get_triangle_count",json.dumps({"mesh":obj}))["returnValue"]


def current():
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.get_current_level","{}")["returnValue"]


def find(actor_class):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.find_actors",json.dumps({"name":"","tag":"",
        "collision_channels":[],"actor_type":{"refPath":actor_class}}))["returnValue"]


def remove(actor):
    assert actor["refPath"].startswith(BASE+"/")
    assert execute_tool("editor_toolset.toolsets.scene.SceneTools.remove_from_scene",json.dumps({"actor":actor}))["returnValue"]


def spawn_mesh(path,name):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_asset",
        json.dumps({"asset_path":path,"name":name,"xform":{}}))["returnValue"]


def spawn_class(path,name,xform):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_class",
        json.dumps({"actor_type":{"refPath":path},"name":name,"xform":xform}))["returnValue"]


def components(actor):
    return execute_tool("editor_toolset.toolsets.actor.ActorTools.get_components",json.dumps({"actor":actor,
        "component_type":{"refPath":"/Script/Engine.StaticMeshComponent"}}))["returnValue"]


def folder(actor,path):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.set_actor_folder",
        json.dumps({"actor":actor,"folder_path":path}))


def save(paths):
    assert execute_tool("editor_toolset.toolsets.asset.AssetTools.save_assets",json.dumps({"asset_paths":paths}))["returnValue"]


def trace(start,end):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.trace_world",
        json.dumps({"start":start,"end":end}))["returnValue"]


def asset_path(item):
    return BASE+("/" if item["role"]=="Crust" else "/Authored/")+item["name"]


def transform(local):
    x,y,h=local
    s=math.sqrt(3)
    raw=[30000/s-x/math.sqrt(6)-y/math.sqrt(2),
         30000/s-x/math.sqrt(6)+y/math.sqrt(2),30000/s+2*x/math.sqrt(6)]
    length=math.sqrt(sum(v*v for v in raw))
    direction=[v/length for v in raw]
    location=dict(zip(("x","y","z"),(v*(30000-h) for v in direction)))
    # +Z는 구 중심 쪽, +X는 접평면 방향이다. pitch/yaw는 forward, roll은 현지 Up을 맞춘다.
    up=[-v for v in direction]
    forward=[-1/math.sqrt(6),-1/math.sqrt(6),2/math.sqrt(6)]
    dot=sum(a*b for a,b in zip(forward,up))
    forward=[a-dot*b for a,b in zip(forward,up)]
    n=math.sqrt(sum(v*v for v in forward))
    forward=[v/n for v in forward]
    yaw=math.atan2(forward[1],forward[0])
    pitch=math.atan2(forward[2],math.hypot(forward[0],forward[1]))
    y0=[-math.sin(yaw),math.cos(yaw),0]
    z0=[-math.sin(pitch)*math.cos(yaw),-math.sin(pitch)*math.sin(yaw),math.cos(pitch)]
    roll=math.atan2(sum(a*b for a,b in zip(up,y0)),sum(a*b for a,b in zip(up,z0)))
    return {"location":location,"rotation":{"pitch":math.degrees(pitch),"yaw":math.degrees(yaw),"roll":math.degrees(roll)}}


def run():
    manifest=read_json("manifest.json")
    if ACTION=="import":
        verified=[]
        for item in manifest["exports"][START:END]:
            path=asset_path(item)
            obj={"refPath":path+"."+item["name"]} if exists(path) else import_mesh(item,path.rsplit("/",1)[0])
            assert "bodySetup" in list_props(obj)
            body=get_props(obj,["bodySetup"])["bodySetup"]
            assert "collisionTraceFlag" in list_props(body)
            set_props(body,{"collisionTraceFlag":"CTF_UseComplexAsSimple","bDoubleSidedGeometry":False})
            set_props(obj,{"lODForCollision":0})
            actual=bounds(obj)
            error=max(abs(actual[side][axis]-item["bounds_cm"][side][i])
                for side in ("min","max") for i,axis in enumerate(("x","y","z")))
            count=triangles(obj)
            assert count==item["triangles"],(item["name"],count,item["triangles"])
            assert error<0.05,(item["name"],error)
            assert get_props(body,["collisionTraceFlag"])["collisionTraceFlag"]=="CTF_UseComplexAsSimple"
            verified.append({"path":path,"triangles":count,"bounds_error_cm":error})
        save([v["path"] for v in verified])
        write_json(f"import-{START:02d}-{END:02d}.json",verified)
        return {"imported":len(verified),"max_bounds_error_cm":max(v["bounds_error_cm"] for v in verified)}
    assert current()==LEVEL,"Open only the Acropolis level before assembly"
    if ACTION=="geometry":
        for actor in find("/Script/Engine.StaticMeshActor"):
            remove(actor)
        for item in manifest["exports"]:
            actor=spawn_mesh(asset_path(item),"Crust" if item["role"]=="Crust" else item["name"].replace("SM_Acropolis_00_",""))
            component=components(actor)[0]
            schema=list_props(component)
            assert "componentTags" in schema and "bodyInstance" in schema
            support=item["role"] in ("Floor","Crust")
            tags=["LNP.Surface.Blocker","LNP.Surface.Static"]
            if support:
                tags.append("LNP.Surface.Support")
            if item["role"]=="Crust":
                tags.append("LNP.Surface.CoarseSupport")
            material="Ground" if item["role"]=="Crust" else "AirRock" if "AirCover" in item["name"] else "Stone"
            set_props(component,{"componentTags":tags,"mobility":"Static","bUseDefaultCollision":False,
                "bodyInstance":{"collisionProfileName":"LNPStaticTerrain" if support else "LNPStaticBlocker"},
                "overrideMaterials":[{"refPath":BASE+f"/M_Acropolis_{material}.M_Acropolis_{material}"}]})
            folder(actor,"Geometry/"+("Terrain" if item["role"]=="Crust" else "Pockets" if "Pocket" in item["name"] else "AirCover" if "AirCover" in item["name"] else "Ruins" if "Ruins" in item["name"] else "Islands"))
        save([LEVEL])
        return {"geometry_actors":len(manifest["exports"])}
    if ACTION=="markers":
        placement=read_json("placement.json")
        for cls in ("/Script/LootNPop.LNPPlacementMarker","/Script/LootNPop.LNPMassSpawnPoint","/Script/Engine.TargetPoint"):
            for actor in find(cls):
                remove(actor)
        for i,item in enumerate(placement["anchors"]):
            actor=spawn_class("/Script/LootNPop.LNPPlacementMarker","Anchor_"+item["id"],transform(item["local"]))
            list_props(actor)
            set_props(actor,{"elementClass":{"refPath":"/Game/WorldDevice/BP_GrappleAnchor.BP_GrappleAnchor_C"},
                "grappleMaxDistance":item["max_distance"],"tags":["LNP.Grapple.AuthoredOnly"]})
            folder(actor,"Markers/Grapple")
        for i,item in enumerate(placement["pods"]):
            actor=spawn_class("/Script/LootNPop.LNPMassSpawnPoint","Spawn_"+item["id"],transform(item["local"]))
            list_props(actor)
            set_props(actor,{"targetSpawnSetId":"None"})
            folder(actor,"Markers/Pods")
        reviews=[{"id":"AcropolisBoss","local":[0,0,-1600]}]
        reviews.extend({"id":"Acropolis"+n["id"],"local":[n["local"][0],n["local"][1],n["local"][2]+200]}
                       for n in placement["nodes"] if n["id"].startswith(("Entry","Upper")))
        reviews.extend({"id":"Acropolis"+p["id"],"local":[p["local"][0],p["local"][1],p["local"][2]+200]}
                       for p in placement["pods"] if p["id"].startswith("Pocket"))
        for item in reviews:
            actor=spawn_class("/Script/Engine.TargetPoint","Review_"+item["id"],transform(item["local"]))
            list_props(actor)
            set_props(actor,{"tags":["LNP.ReviewPoint",item["id"]],"bIsEditorOnlyActor":True})
            folder(actor,"Markers/Review")
        save([LEVEL,BASE+"/M_Acropolis_Ground",BASE+"/M_Acropolis_Stone",BASE+"/M_Acropolis_AirRock"])
        return {"anchors":len(placement["anchors"]),"pods":len(placement["pods"]),"reviews":len(reviews)}
    if ACTION=="verify":
        placement=read_json("placement.json")
        geometry=find("/Script/Engine.StaticMeshActor")
        assert len(geometry)==len(manifest["exports"])
        for actor in geometry:
            c=components(actor)[0]
            p=get_props(c,["componentTags","bodyInstance","mobility"])
            support="LNP.Surface.Support" in p["componentTags"]
            assert p["mobility"]=="Static"
            assert p["bodyInstance"]["collisionProfileName"]==("LNPStaticTerrain" if support else "LNPStaticBlocker")
        floor_checks=[]
        for pod in placement["pods"]:
            x,y,h=pod["local"]
            for radius in (0,250):
                for i in range(8 if radius else 1):
                    a=i*math.tau/8
                    xx,yy=x+radius*math.cos(a),y+radius*math.sin(a)
                    start=transform([xx,yy,h+50])["location"]
                    end=transform([xx,yy,h-70])["location"]
                    hit=trace(start,end)
                    assert hit is not None and abs(hit-50)<3,(pod["id"],hit)
                    floor_checks.append(hit)
        pocket_checks=[]
        visibility=read_json("geometry-verification.json")
        for check in visibility["pocket_floor_checks"]:
            x,y,h=check["local"]
            hit=trace(transform([x,y,h+50])["location"],transform([x,y,h-70])["location"])
            assert hit is not None and abs(hit-50)<3,(check,hit)
            pocket_checks.append(hit)
        for pocket in manifest["pockets"]:
            x,y,h=pocket["pod"]
            hit=trace(transform([x,y,h+200])["location"],transform([x,y,h+3800])["location"])
            assert hit is not None and abs(hit-3000)<3,(pocket["id"],"ceiling",hit)
        anchors={a["id"]:a for a in placement["anchors"]}
        for edge in visibility["hook_visibility_edges"]:
            start=transform(edge["start_local"])["location"]
            end=transform(anchors[edge["to"]]["local"])["location"]
            hit=trace(start,end)
            assert hit is None,("Blocked native hook ray",edge["from"],edge["to"],hit)
        marker_ids=[]
        for actor in find("/Script/LootNPop.LNPPlacementMarker"):
            p=get_props(actor,["markerId","elementClass","grappleMaxDistance","tags"])
            assert p["grappleMaxDistance"]>0 and "LNP.Grapple.AuthoredOnly" in p["tags"]
            assert "BP_GrappleAnchor_C" in p["elementClass"]["refPath"]
            marker_ids.append(p["markerId"])
        spawn_ids=[get_props(a,["spawnPointId"])["spawnPointId"] for a in find("/Script/LootNPop.LNPMassSpawnPoint")]
        assert len(marker_ids)==len(placement["anchors"]) and len(set(marker_ids))==len(marker_ids)
        assert len(spawn_ids)==33 and len(set(spawn_ids))==33
        assert all(i!="00000000-0000-0000-0000-000000000000" for i in marker_ids+spawn_ids)
        report={"geometry_actors":len(geometry),"collision_profiles_verified":len(geometry),
                "native_pod_floor_rays":len(floor_checks),"max_pod_floor_error_cm":max(abs(v-50) for v in floor_checks),
                "native_pocket_floor_rays":len(pocket_checks),"native_pocket_ceilings":5,
                "native_hook_visibility_rays":len(visibility["hook_visibility_edges"]),
                "unique_anchor_ids":len(marker_ids),"unique_spawn_ids":len(spawn_ids),
                "limits":["Visibility line traces only; no capsule sweeps, bake, pool registration or PIE"]}
        write_json("unreal-verification.json",report)
        return report
    raise ValueError("Unknown assembly action")
