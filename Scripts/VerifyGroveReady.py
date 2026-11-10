"""라이브 에디터 ProgrammaticToolset에서 Grove 대표 구역의 실제 충돌 광선을 확인한다."""
import json
import math

OUT = "D:/UnrealProjects/LootNPop/Saved/SliceOctant/Grove_00/V042/"
LEVEL = "/Game/Maps/_WorkInProgress/Grove_00/V042/LVI_Octant_Grove_00"

def read(name):
    return json.loads(execute_tool("editor_toolset.toolsets.asset.AssetTools.read_file", json.dumps({"file_path":OUT+name}))["returnValue"])

def write(name, value):
    execute_tool("editor_toolset.toolsets.asset.AssetTools.write_file", json.dumps({"file_path":OUT+name,"content":json.dumps(value,indent=2)}))

def current():
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.get_current_level","{}")["returnValue"]

def find(cls):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.find_actors",json.dumps({"name":"","tag":"","collision_channels":[],"actor_type":{"refPath":cls}}))["returnValue"]

def components(actor):
    return execute_tool("editor_toolset.toolsets.actor.ActorTools.get_components",json.dumps({"actor":actor,"component_type":{"refPath":"/Script/Engine.StaticMeshComponent"}}))["returnValue"]

def schema(obj):
    return json.loads(execute_tool("editor_toolset.toolsets.object.ObjectTools.list_properties",json.dumps({"instance":obj}))["returnValue"])

def get(obj,names):
    return json.loads(execute_tool("editor_toolset.toolsets.object.ObjectTools.get_properties",json.dumps({"instance":obj,"properties":names}))["returnValue"])

def trace(start,end):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.trace_world",json.dumps({"start":start,"end":end}))["returnValue"]

def point(local):
    x,y,h=local
    raw=[30000/math.sqrt(3)-x/math.sqrt(6)-y/math.sqrt(2),30000/math.sqrt(3)-x/math.sqrt(6)+y/math.sqrt(2),30000/math.sqrt(3)+2*x/math.sqrt(6)]
    scale=(30000-h)/math.sqrt(sum(v*v for v in raw))
    return dict(zip(("x","y","z"),[v*scale for v in raw]))

def run():
    assert current()==LEVEL
    probes=[probe for name in read("native-probe-index.json") for probe in read(name)]
    placement=read("placement.json")
    geometry=find("/Script/Engine.StaticMeshActor")
    assert len(geometry)==len(read("manifest.json")["exports"])
    for actor in geometry:
        c=components(actor)[0]
        assert all(n in schema(c) for n in ["staticMesh","componentTags","bodyInstance","mobility"])
        p=get(c,["staticMesh","componentTags","bodyInstance","mobility"])
        support="LNP.Surface.Support" in p["componentTags"]
        assert p["mobility"]=="Static"
        assert p["bodyInstance"]["collisionProfileName"]==("LNPStaticTerrain" if support else "LNPStaticBlocker")
        mesh=p["staticMesh"]
        assert "bodySetup" in schema(mesh)
        body=get(mesh,["bodySetup"])["bodySetup"]
        assert "collisionTraceFlag" in schema(body)
        assert get(body,["collisionTraceFlag"])["collisionTraceFlag"]=="CTF_UseComplexAsSimple"
    maximum=0
    counts={"Floor":0,"Wall":0,"PassageClearance":0}
    for check in probes:
        start,end,expected,kind=check
        hit=trace(point(start),point(end))
        counts[kind]+=1
        if expected is None:
            assert hit is None,("Native passage blocked",check,hit)
            continue
        assert hit is not None,("Missing native floor",check)
        error=abs(hit-expected)
        assert error<.1,("Native/source floor mismatch",check,hit,error)
        maximum=max(maximum,error)
    headroom=0
    for pod in placement["pods"]:
        x,y,h=pod["local"]
        for i in range(9):
            r=0 if i==8 else 300
            a=i*math.tau/8
            xx,yy=x+r*math.cos(a),y+r*math.sin(a)
            assert trace(point([xx,yy,h+100]),point([xx,yy,h+pod.get("headroom",3000)])) is None
            headroom+=1
    nodes={n["id"]:n for n in placement["nodes"]}
    a,b=nodes["ClearingA"]["local"],nodes["ClearingB"]["local"]
    assert trace(point([a[0],a[1],a[2]+190]),point([b[0],b[1],b[2]+190])) is not None
    approaches=0
    for anchor in placement["anchors"]:
        for source in [anchor["from"]]+anchor.get("also_from",[]):
            a=list(nodes[source]["local"])
            if source=="EastGround":
                distance=trace(point([a[0],a[1],a[2]+200]),point([a[0],a[1],a[2]-200]))
                assert distance is not None
                a[2]+=200-distance
            start=a if source=="VaultInner" else [a[0],a[1],a[2]+190]
            assert trace(point(start),point(anchor["local"])) is None,(anchor["id"],source)
            approaches+=1
    ids=[]
    for actor in find("/Script/LootNPop.LNPPlacementMarker"):
        assert "markerId" in schema(actor)
        p=get(actor,["markerId","tags","elementClass","grappleMaxDistance"])
        assert "LNP.Grapple.AuthoredOnly" in p["tags"]
        assert p["grappleMaxDistance"]>0
        ids.append(p["markerId"])
    assert len(ids)==len(set(ids))==len(placement["anchors"])
    spawns=find("/Script/LootNPop.LNPMassSpawnPoint")
    spawn_ids=[]
    for actor in spawns:
        assert "spawnPointId" in schema(actor)
        spawn_ids.append(get(actor,["spawnPointId"])["spawnPointId"])
    assert len(spawn_ids)==len(set(spawn_ids))==len(placement["pods"])
    report={"geometry":len(geometry),"native_scope":read("geometry-verification.json").get("native_scope","full_geometry"),"native_floor_rays":counts["Floor"],"native_wall_rays":counts["Wall"],"native_passage_clearance_rays":counts["PassageClearance"],"native_source_max_difference_cm":maximum,"pod_headroom_rays":headroom,"pod_headroom_cm":{p["id"]:p.get("headroom",3000) for p in placement["pods"]},"cover_rays":1,"hook_visibility_rays":approaches,"unique_marker_ids":len(ids),"unique_spawn_ids":len(spawn_ids),"limits":["Visibility-channel rays only; not capsule sweeps, Mover, hook inertia or NPC combat"]}
    write("native-verification.json",report)
    return report
