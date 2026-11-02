"""Phase 5 §45: 리슨 2인 PIE에서 모든 PIE 월드의 플레이어 폰(UAF 시험·기존 ABP)을 매 틱 기록하는 읽기 전용 샘플러.

같은 플레이어의 사본(서버 권위·소유 클라·시뮬레이티드 프록시)을 PlayerId로 짝지어 비교하기 위한 원자료다.
PIE 중 에디터 콘솔에서 실행한다.
  py "D:/UnrealProjects/LootNPop/Scripts/AnimOverhaul/SampleNetPawns_5-2.py" start <라벨>
  py "D:/UnrealProjects/LootNPop/Scripts/AnimOverhaul/SampleNetPawns_5-2.py" stop
  py "D:/UnrealProjects/LootNPop/Scripts/AnimOverhaul/SampleNetPawns_5-2.py" probe
start는 매 틱 기록하고 stop(또는 MAX_SECONDS 경과·PIE 종료)에서
Saved/AnimationOverhaul/2026-11-02_UAF54NetPawns/<라벨>.json을 쓴다.
probe는 한 틱만 읽어 probe.json을 쓴다(API·월드 탐색 확인용).
에셋 저장·몽타주 재생·캐릭터 이동·CVar 변경은 하지 않는다.
"""
import json
import sys
from pathlib import Path
import unreal

MAX_SECONDS = 360.0
OUTPUT_DIR = Path(unreal.Paths.project_saved_dir()) / "AnimationOverhaul/2026-11-02_UAF54NetPawns"


def vec(v):
    return [round(v.x, 2), round(v.y, 2), round(v.z, 2)]


def unit(v):
    return [round(v.x, 5), round(v.y, 5), round(v.z, 5)]


def find_mesh(pawn, name):
    for comp in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
        if comp.get_name() == name:
            return comp
    return None


def pie_worlds():
    # 리슨 2인은 한 프로세스 안에 PIE 월드가 둘이다. get_game_world는 하나만 돌려준다.
    worlds = list(unreal.EditorLevelLibrary.get_pie_worlds(False) or [])
    if not worlds:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        worlds = [world] if world else []
    return worlds


def enum_name(value):
    return getattr(value, "name", str(value)) if value is not None else None


def read_pawn(pawn):
    state = pawn.get_player_state() if hasattr(pawn, "get_player_state") else pawn.player_state
    source = find_mesh(pawn, "AnimSourceMesh")
    visual = find_mesh(pawn, "VisualMesh")
    anim = source.get_anim_instance() if source else None
    montage = anim.get_current_active_montage() if anim else None
    row = {"id": state.get_editor_property("player_id") if state else -1,
           "role": enum_name(pawn.get_local_role()),
           "local": pawn.is_locally_controlled(),
           "loc": vec(pawn.get_actor_location()), "fwd": unit(pawn.get_actor_forward_vector()),
           "up": unit(pawn.get_actor_up_vector()), "vel": vec(pawn.get_velocity()),
           "montage": montage.get_name() if montage else None,
           "montagePos": round(anim.montage_get_position(montage), 4) if montage else None,
           "anim": anim.get_class().get_name() if anim else None}
    if anim and hasattr(anim, "get_offset_root_translation_mode"):
        row["rootT"] = enum_name(anim.get_offset_root_translation_mode())
        row["rootR"] = enum_name(anim.get_offset_root_rotation_mode())
    for key, mesh in (("source", source), ("visual", visual)):
        if mesh:
            row[key + "Comp"] = vec(mesh.get_world_location())
            row[key + "Root"] = vec(mesh.get_socket_location("root"))
            row[key + "Pelvis"] = vec(mesh.get_socket_location("pelvis"))
    return row


def read_frame():
    rows = []
    for world in pie_worlds():
        net_mode = "Server" if unreal.SystemLibrary.is_server(world) else "Client"
        now = unreal.GameplayStatics.get_time_seconds(world)
        # 기존 ABP 폰과 비교할 수 있게 시험 폰의 상위 클래스로 찾는다. 행의 anim이 경로를 구분한다.
        for pawn in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LNPPlayerCharacter):
            row = read_pawn(pawn)
            row["world"] = world.get_path_name()
            row["net"] = net_mode
            row["t"] = round(now, 4)
            rows.append(row)
    return rows


class Sampler:
    def __init__(self, label):
        self.label = label
        self.frames = []
        self.start_frame = None
        self.start_wall = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        print("[NetPawnSampler] start " + label)

    def tick(self, delta):
        # 매 틱 콜백에서 예외가 나면 같은 오류가 프레임마다 쌓이므로 첫 예외에서 기록을 멈춘다.
        try:
            self.sample()
        except Exception as error:
            self.finish("error: " + repr(error))

    def sample(self):
        worlds = pie_worlds()
        if not worlds:
            self.finish("pie-ended")
            return
        wall = unreal.GameplayStatics.get_time_seconds(worlds[0])
        if self.start_wall is None:
            self.start_wall = wall
        elif wall - self.start_wall > MAX_SECONDS:
            self.finish("timeout")
            return
        rows = read_frame()
        if rows:
            self.frames.append({"frame": unreal.SystemLibrary.get_frame_count(), "pawns": rows})

    def finish(self, reason):
        if self.handle is None:
            return
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.handle = None
        write(self.label, reason, self.frames)


def write(label, reason, frames):
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output = OUTPUT_DIR / (label + ".json")
    report = {"scope": "Read-only per-tick sampling of UAF spike pawns in every PIE world; not a gate verdict",
              "label": label, "reason": reason, "frames": frames}
    output.write_text(json.dumps(report), encoding="utf-8")
    print("[NetPawnSampler] wrote %d frames (%s) %s" % (len(frames), reason, output))


command = sys.argv[1] if len(sys.argv) > 1 else "start"
existing = getattr(unreal, "_lnp_net_pawn_sampler", None)
if command == "start":
    if existing:
        existing.finish("restarted")
    unreal._lnp_net_pawn_sampler = Sampler(sys.argv[2] if len(sys.argv) > 2 else "Sample")
elif command == "stop":
    if existing:
        existing.finish("stopped")
        unreal._lnp_net_pawn_sampler = None
    else:
        print("[NetPawnSampler] not running")
elif command == "probe":
    try:
        write("probe", "probe", [{"frame": unreal.SystemLibrary.get_frame_count(), "pawns": read_frame(),
                                  "worlds": [w.get_path_name() for w in pie_worlds()]}])
    except Exception as error:
        write("probe", "error: " + repr(error), [])
