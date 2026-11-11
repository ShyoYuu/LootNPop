"""라이브 에디터 ProgrammaticToolset에서 Ridge 작업본 LVI의 실제 충돌 광선·역할/profile·마커 ID를 확인한다.

사용: 이 파일 내용을 ProgrammaticToolset.execute_tool_script의 script로 보낸다(VERSION 줄만 맞춘다).
원본 Blender 광선과 같은 시작/끝 점을 UE trace_world로 다시 쏘아 거리 차이를 비교한다. 캡슐·Mover·훅 관성은 아니다.
"""
import json
import math

VERSION = "V011"
OUT = "D:/UnrealProjects/LootNPop/Saved/SliceOctant/Ridge_00/"+VERSION+"/"
LEVEL = "/Game/Maps/_WorkInProgress/Ridge_00/"+VERSION+"/LVI_Octant_Ridge_00"


def call(name, args):
    return execute_tool(name, json.dumps(args))["returnValue"]


def read(name):
    return json.loads(call("editor_toolset.toolsets.asset.AssetTools.read_file", {"file_path": OUT+name}))


def write(name, value):
    call("editor_toolset.toolsets.asset.AssetTools.write_file", {"file_path": OUT+name, "content": json.dumps(value, indent=2)})


def find(cls):
    return call("editor_toolset.toolsets.scene.SceneTools.find_actors", {"name": "", "tag": "", "collision_channels": [], "actor_type": {"refPath": cls}})


def schema(obj):
    return json.loads(call("editor_toolset.toolsets.object.ObjectTools.list_properties", {"instance": obj}))


def get(obj, names):
    return json.loads(call("editor_toolset.toolsets.object.ObjectTools.get_properties", {"instance": obj, "properties": names}))


def trace(start, end):
    return call("editor_toolset.toolsets.scene.SceneTools.trace_world", {"start": start, "end": end})


def point(local):
    x, y, h = local
    raw = [30000/math.sqrt(3)-x/math.sqrt(6)-y/math.sqrt(2), 30000/math.sqrt(3)-x/math.sqrt(6)+y/math.sqrt(2), 30000/math.sqrt(3)+2*x/math.sqrt(6)]
    scale = (30000-h)/math.sqrt(sum(v*v for v in raw))
    return dict(zip(("x", "y", "z"), [v*scale for v in raw]))


def run():
    assert call("editor_toolset.toolsets.scene.SceneTools.get_current_level", {}) == LEVEL
    geometry = find("/Script/Engine.StaticMeshActor")
    assert len(geometry) == len(read("manifest.json")["exports"])
    for actor in geometry:
        c = call("editor_toolset.toolsets.actor.ActorTools.get_components", {"actor": actor, "component_type": {"refPath": "/Script/Engine.StaticMeshComponent"}})[0]
        p = get(c, ["staticMesh", "componentTags", "bodyInstance", "mobility"])
        support = "LNP.Surface.Support" in p["componentTags"]
        assert p["mobility"] == "Static"
        assert p["bodyInstance"]["collisionProfileName"] == ("LNPStaticTerrain" if support else "LNPStaticBlocker")
        body = get(p["staticMesh"], ["bodySetup"])["bodySetup"]
        assert get(body, ["collisionTraceFlag"])["collisionTraceFlag"] == "CTF_UseComplexAsSimple"
    counts, maximum = {}, 0
    for name in read("native-probe-index.json"):
        for start, end, expected, kind in read(name):
            hit = trace(point(start), point(end))
            counts[kind] = counts.get(kind, 0)+1
            if expected is None:
                assert hit is None, ("Native passage blocked", kind, start, end, hit)
                continue
            assert hit is not None, ("Missing native floor", start)
            error = abs(hit-expected)
            assert error < .5, ("Native/source floor mismatch", start, hit, expected)
            maximum = max(maximum, error)
    placement = read("placement.json")
    ids = []
    for actor in find("/Script/LootNPop.LNPPlacementMarker"):
        p = get(actor, ["markerId", "tags", "grappleMaxDistance"])
        assert "LNP.Grapple.AuthoredOnly" in p["tags"] and p["grappleMaxDistance"] > 0
        ids.append(p["markerId"])
    assert len(ids) == len(set(ids)) == len(placement["anchors"])
    spawn_ids = [get(a, ["spawnPointId"])["spawnPointId"] for a in find("/Script/LootNPop.LNPMassSpawnPoint")]
    assert len(spawn_ids) == len(set(spawn_ids)) == len(placement["pods"])
    report = {"geometry_actors": len(geometry), "native_rays": counts, "native_source_max_difference_cm": maximum,
              "unique_marker_ids": len(ids), "unique_spawn_ids": len(spawn_ids),
              "limits": ["Visibility-channel rays only; not capsule sweeps, Mover, hook inertia or NPC combat"]}
    write("native-verification.json", report)
    return report
