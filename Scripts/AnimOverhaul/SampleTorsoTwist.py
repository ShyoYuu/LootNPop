"""상하체 비틀림(가상 루트 Yaw 오프셋) 진단: 로컬 플레이어 폰을 매 틱 기록하는 읽기 전용 샘플러.

PIE 중 에디터 콘솔에서 실행한다.
  py "D:/UnrealProjects/LootNPop/Scripts/AnimOverhaul/SampleTorsoTwist.py" start <라벨>
  py "D:/UnrealProjects/LootNPop/Scripts/AnimOverhaul/SampleTorsoTwist.py" stop
start는 매 틱 아래 값을 읽고, stop(또는 600초 경과·PIE 종료)에서
Saved/AnimationOverhaul/TorsoTwist/<라벨>.json을 쓴다. 각도는 모두 캐릭터 Up 둘레 부호 각(도)이다.
  control  = 캡슐 전방 → 카메라 전방(캡슐이 카메라를 따르는지)
  root     = 메시 컴포넌트 → 가상 루트(OffsetRootTransform, 하체 오프셋)
  rootBone = 캡슐 전방 → 원본 메시 root 본 전방(포즈에 실제로 반영된 루트)
  hips     = 캡슐 우측 → 오른쪽 허벅지-왼쪽 허벅지 선(표시 하체 방향, 오리엔테이션 워핑 포함)
  vel      = 캡슐 전방 → 접평면 속도
에셋 저장·캐릭터 이동·CVar 변경은 하지 않는다.
"""
import json
import math
import sys
from pathlib import Path
import unreal

MAX_SECONDS = 600.0
OUTPUT_DIR = Path(unreal.Paths.project_saved_dir()) / "AnimationOverhaul/TorsoTwist"


def find_mesh(pawn, name):
    for comp in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
        if comp.get_name() == name:
            return comp
    return None


def signed_angle(a, b, up):
    # 두 벡터를 Up 접평면에 투영한 뒤 Up 둘레 부호 각
    a = a - up * a.dot(up)
    b = b - up * b.dot(up)
    if a.length() < 1e-4 or b.length() < 1e-4:
        return None
    return math.degrees(math.atan2(up.dot(a.cross(b)), a.dot(b)))


def read(anim, name):
    try:
        return anim.get_editor_property(name)
    except Exception:
        return None


class Sampler:
    def __init__(self, label):
        self.label = label
        self.samples = []
        self.meta = {}
        self.start_time = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        print("[TwistSampler] start " + label)

    def tick(self, delta):
        # 첫 예외에서 기록을 멈춘다(같은 오류가 프레임마다 쌓이지 않게)
        try:
            self.sample()
        except Exception as error:
            self.finish("error: " + repr(error))

    def sample(self):
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            self.finish("pie-ended")
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not pawn:
            return
        now = unreal.GameplayStatics.get_time_seconds(world)
        if self.start_time is None:
            self.start_time = now
            self.meta = {"pawn": pawn.get_path_name(), "class": pawn.get_class().get_path_name(),
                         "world": world.get_path_name()}
        elif now - self.start_time > MAX_SECONDS:
            self.finish("timeout")
            return

        source = find_mesh(pawn, "AnimSourceMesh")
        anim = source.get_anim_instance() if source else None
        if not anim:
            return
        up = pawn.get_actor_up_vector()
        fwd = pawn.get_actor_forward_vector()
        right = pawn.get_actor_right_vector()
        controller = pawn.get_controller()
        control_fwd = controller.get_control_rotation().get_forward_vector() if controller else None

        row = {"t": round(now - self.start_time, 4), "frame": unreal.SystemLibrary.get_frame_count()}
        row["control"] = signed_angle(fwd, control_fwd, up) if control_fwd else None
        row["vel"] = signed_angle(fwd, pawn.get_velocity(), up)
        row["speed"] = (pawn.get_velocity() - up * pawn.get_velocity().dot(up)).length()

        root_transform = read(anim, "offset_root_transform")
        if read(anim, "has_offset_root") and root_transform:
            comp_x = source.get_world_rotation().get_forward_vector()
            root_x = root_transform.rotation.rotate_vector(unreal.Vector(1, 0, 0))
            row["root"] = signed_angle(comp_x, root_x, up)
        root_bone = source.get_socket_rotation("root").get_forward_vector()
        comp_x = source.get_world_rotation().get_forward_vector()
        # 컴포넌트 축 편차(UEFN 메시는 캡슐 대비 Yaw가 돌아가 있다)를 빼서 캡슐 기준으로 맞춘다
        comp_bias = signed_angle(fwd, comp_x, up) or 0.0
        bone_angle = signed_angle(fwd, root_bone, up)
        row["rootBone"] = None if bone_angle is None else (bone_angle - comp_bias + 540.0) % 360.0 - 180.0
        hips = source.get_socket_location("thigh_r") - source.get_socket_location("thigh_l")
        row["hips"] = signed_angle(right, hips, up)

        row["aimYaw"] = read(anim, "aim_yaw")
        row["ffd"] = read(anim, "future_facing_delta")
        row["tip"] = read(anim, "should_turn_in_place")
        state = read(anim, "movement_state")
        row["state"] = str(state).split(".")[-1] if state is not None else None
        montage = anim.get_current_active_montage()
        row["montage"] = montage.get_name() if montage else None
        self.samples.append(row)

    def finish(self, reason):
        if self.handle is None:
            return
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.handle = None
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        output = OUTPUT_DIR / (self.label + ".json")
        report = {"scope": "Read-only torso twist sampling", "label": self.label, "reason": reason,
                  "meta": self.meta, "samples": self.samples}
        output.write_text(json.dumps(report), encoding="utf-8")
        print("[TwistSampler] wrote %d samples (%s) %s" % (len(self.samples), reason, output))


command = sys.argv[1] if len(sys.argv) > 1 else "start"
existing = getattr(unreal, "_lnp_twist_sampler", None)
if command == "start":
    if existing:
        existing.finish("restarted")
    unreal._lnp_twist_sampler = Sampler(sys.argv[2] if len(sys.argv) > 2 else "Sample")
elif command == "stop":
    if existing:
        existing.finish("stopped")
        unreal._lnp_twist_sampler = None
    else:
        print("[TwistSampler] not running")
