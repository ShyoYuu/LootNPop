"""Phase 5 §44: 대시 중 캡슐 이동과 표시 메시 추종을 매 틱 기록하는 읽기 전용 샘플러.

PIE 중 에디터 콘솔에서 실행한다.
  py "D:/UnrealProjects/LootNPop/Scripts/AnimOverhaul/SampleDashMotion_5-2.py" start <라벨>
  py "D:/UnrealProjects/LootNPop/Scripts/AnimOverhaul/SampleDashMotion_5-2.py" stop
start는 로컬 플레이어 폰을 매 틱 읽고, stop(또는 120초 경과·PIE 종료)에서
Saved/AnimationOverhaul/2026-11-02_UAF53DashCompare/<라벨>.json을 쓴다.
에셋 저장·몽타주 재생·캐릭터 이동·CVar 변경은 하지 않는다.
"""
import json
import sys
from pathlib import Path
import unreal

MAX_SECONDS = 120.0
OUTPUT_DIR = Path(unreal.Paths.project_saved_dir()) / "AnimationOverhaul/2026-11-02_UAF53DashCompare"


def vec(v):
    return [v.x, v.y, v.z]


def find_mesh(pawn, name):
    for comp in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
        if comp.get_name() == name:
            return comp
    return None


class Sampler:
    def __init__(self, label):
        self.label = label
        self.samples = []
        self.meta = {}
        self.start_time = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        print("[DashSampler] start " + label)

    def tick(self, delta):
        # 매 틱 콜백에서 예외가 나면 같은 오류가 프레임마다 쌓이므로 첫 예외에서 기록을 멈춘다.
        try:
            self.sample()
        except Exception as error:
            self.finish("error: " + repr(error))

    def sample(self):
        world =unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            self.finish("pie-ended")
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not pawn:
            return
        now = unreal.GameplayStatics.get_time_seconds(world)
        if self.start_time is None:
            self.start_time = now
            self.meta = {"pawn": pawn.get_path_name(), "class": pawn.get_class().get_path_name()}
        elif now - self.start_time > MAX_SECONDS:
            self.finish("timeout")
            return
        source = find_mesh(pawn, "AnimSourceMesh")
        visual = find_mesh(pawn, "VisualMesh")
        anim = source.get_anim_instance() if source else None
        montage = anim.get_current_active_montage() if anim else None
        row = {"t": now, "frame": unreal.SystemLibrary.get_frame_count(),
               "loc": vec(pawn.get_actor_location()), "fwd": vec(pawn.get_actor_forward_vector()),
               "up": vec(pawn.get_actor_up_vector()), "vel": vec(pawn.get_velocity()),
               "montage": montage.get_name() if montage else None,
               "montagePos": anim.montage_get_position(montage) if montage else None}
        for key, mesh in (("source", source), ("visual", visual)):
            if mesh:
                row[key + "Comp"] = vec(mesh.get_world_location())
                row[key + "Root"] = vec(mesh.get_socket_location("root"))
                row[key + "Pelvis"] = vec(mesh.get_socket_location("pelvis"))
        self.samples.append(row)

    def finish(self, reason):
        if self.handle is None:
            return
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.handle = None
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        output = OUTPUT_DIR / (self.label + ".json")
        report = {"scope": "Read-only dash capsule/mesh sampling; not a network or equator verdict",
                  "label": self.label, "reason": reason, "meta": self.meta, "samples": self.samples}
        output.write_text(json.dumps(report), encoding="utf-8")
        print("[DashSampler] wrote %d samples (%s) %s" % (len(self.samples), reason, output))


command = sys.argv[1] if len(sys.argv) > 1 else "start"
existing = getattr(unreal, "_lnp_dash_sampler", None)
if command == "start":
    if existing:
        existing.finish("restarted")
    unreal._lnp_dash_sampler = Sampler(sys.argv[2] if len(sys.argv) > 2 else "Sample")
elif command == "stop":
    if existing:
        existing.finish("stopped")
        unreal._lnp_dash_sampler = None
    else:
        print("[DashSampler] not running")
