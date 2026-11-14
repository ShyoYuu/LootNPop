# 경사 Idle 움찔 수정: 발 배치 CR이 루트를 경사면 높이로 내리게 한다(LootNPop 에디터 Python).
#   py "<저장소>/Scripts/AnimOverhaul/AddRootGroundOffsetCR.py" [CR 경로]
#
# 배경: Mover 바닥 판정(bUseFlatBaseForFloorChecks, 엔진 기본 켬)은 캡슐 바닥을 평평한 원판으로 다뤄 경사에서 원판 가장자리가 먼저 닿는다.
#   캡슐 중심 아래 경사면은 메시 루트보다 반지름 × tan(경사각)만큼 아래에 있다(반지름 42, 28.8°에서 21cm).
#   CR이 발을 지면에 맞추느라 그만큼 골반을 내리고, Pose History(출력 직전)가 그 포즈를 기록해 MM Idle 검색이 웅크린 자세
#   (M_Relaxed_Transition_Crouch_to_Stand 초반)를 고른다 → 1.3초 주기 움찔.
#
# 수정: UpdateRootDamper 함수 끝(Sequence.D → Return 사이)에서 루트 리그 Z를 지면 간격만큼 더 내린다.
#   - 간격은 공개 변수 LNP_RootGroundOffset(cm, ≥0)로 받는다. ULNPAnimInstance가 Mover 바닥 평면으로 계산해 매 프레임 넣는다.
#   - 루트 감쇠가 루트 리그 Z를 e로 덮어쓰므로 CR 앞(AnimGraph)에서 내리면 지워진다. 그래서 감쇠 직후, 접지 트레이스·골반·다리 IK 전에 넣는다.
#   - 비공개 변수 LNP_RootGroundOffsetDamped로 0.1s Damp한다(바닥 접점이 삼각형 경계·능선에서 튈 때 루트 팝 방지).
#   - Pose History는 루트 본 기준으로 특징을 계산하므로 검색 포즈의 골반 높이가 평지와 같아진다. 골반 월드 높이는 바뀌지 않는다(발이 정한다).
# 다시 돌려도 된다(LNP_RootGround_Set 노드가 있으면 건너뛴다).
import json
import sys

import unreal

CR_PATH = sys.argv[1] if len(sys.argv) > 1 else "/Game/Characters/ControlRigs/CR_Biped_FootPlacement_UEFN"
OUT = unreal.Paths.project_saved_dir() + "AnimOverhaul/AddRootGroundOffsetCR.json"
G_DAMPER = "RigVMFunctionLibrary.UpdateRootDamper.New Function_ContainedGraph"
VAR_INPUT = "LNP_RootGroundOffset"
VAR_DAMPED = "LNP_RootGroundOffsetDamped"
SMOOTHING_TIME = "0.100000"

log = []
errors = []


def check(ok, what):
    log.append(("OK " if ok else "FAIL ") + what)
    if not ok:
        errors.append(what)
    return ok


def finish():
    with open(OUT, "w") as f:
        json.dump({"errors": errors, "log": log}, f, indent=1, ensure_ascii=False)
    print("[RootGroundCR] %s, %d errors -> %s" % ("done" if not errors else "FAILED", len(errors), OUT))


bp = unreal.load_asset(CR_PATH)
models = {m.get_path_name().split(":")[-1]: m for m in bp.get_all_models()}
model = models[G_DAMPER]
c = bp.get_controller(model)
nodes = {n.get_name(): n for n in model.get_nodes()}

if "LNP_RootGround_Set" in nodes:
    log.append("already applied, skipped")
    finish()
    raise SystemExit(0)

# 기존 노드에서 타입·구조체 경로를 가져온다(엔진 버전마다 경로를 추측하지 않는다)
double_type = nodes["VariableNode"].find_pin("Value").get_cpp_type()          # Get WorldZDamper
damp_struct = nodes["Damp"].get_script_struct().get_path_name()               # Damp (Float)
get_struct = nodes["RigUnit_GetTransform_1"].get_script_struct().get_path_name()
set_struct = nodes["Set Transform"].get_script_struct().get_path_name()       # Set Translation
sub_struct = "/Script/RigVM.RigVMFunction_MathDoubleSub"
log.append("types: %s / %s / %s / %s" % (double_type, damp_struct, get_struct, set_struct))

# 1) 변수 — 입력은 공개(C++ SetPublicVariableValue), 평활 상태는 비공개
def variable_exists(name):
    """생성 클래스 CDO에 같은 이름 속성이 있으면 이미 추가된 변수다(RigVM 블루프린트는 new_variables를 노출하지 않는다)"""
    try:
        unreal.get_default_object(bp.generated_class()).get_editor_property(name)
        return True
    except Exception:
        return False


existing = {n for n in (VAR_INPUT, VAR_DAMPED) if variable_exists(n)}
log.append("api: add_member_variable=%s" % hasattr(bp, "add_member_variable"))
for name, public in ((VAR_INPUT, True), (VAR_DAMPED, False)):
    if name in existing:
        log.append("variable %s exists" % name)
        continue
    added = bp.add_member_variable(name, double_type, public, False, "0.000000")
    check(str(added) == name, "add variable %s (%s, public=%s) -> %s" % (name, double_type, public, added))
if errors:
    finish()
    raise SystemExit("variable creation failed")


def add_unit(struct_path, name, x, y):
    node = c.add_unit_node_from_struct_path(struct_path, "Execute", unreal.Vector2D(x, y), name, True, False)
    check(node is not None, "add %s (%s)" % (name, struct_path.split(".")[-1]))


def add_var(name, getter, node_name, x, y):
    node = c.add_variable_node(name, double_type, None, getter, "", unreal.Vector2D(x, y), node_name, True, False)
    check(node is not None, "add variable node %s (%s)" % (node_name, "get" if getter else "set"))


def link(a, b):
    check(c.add_link(a, b, True, False), "link %s -> %s" % (a, b))


def unlink(a, b):
    check(c.break_link(a, b, True, False), "break %s -> %s" % (a, b))


def set_default(pin, value):
    check(c.set_pin_default_value(pin, value, False, True, False, False), "default %s = %s" % (pin, value))


X, Y = 4200, 1600
add_var(VAR_INPUT, True, "LNP_RootGround_Target", X, Y)
add_var(VAR_DAMPED, True, "LNP_RootGround_Prev", X, Y + 150)
add_unit(damp_struct, "LNP_RootGround_Damp", X + 300, Y)
add_var(VAR_DAMPED, False, "LNP_RootGround_Store", X + 600, Y - 200)
add_unit(get_struct, "LNP_RootGround_Get", X + 300, Y + 350)
add_unit(sub_struct, "LNP_RootGround_Sub", X + 700, Y + 300)
add_unit(set_struct, "LNP_RootGround_Set", X + 1000, Y - 200)

# 평활: Damped = Damp(Damped → Target, 0.1s)
link("LNP_RootGround_Prev.Value", "LNP_RootGround_Damp.Value")
link("LNP_RootGround_Target.Value", "LNP_RootGround_Damp.Target")
set_default("LNP_RootGround_Damp.SmoothingTime", SMOOTHING_TIME)
link("LNP_RootGround_Damp.Result", "LNP_RootGround_Store.Value")

# 루트 리그(Global) 위치 = (X, Y, Z - Damped). 자식 전파(몸 전체가 같이 내려간 뒤 접지·골반·다리 IK가 풀린다)
set_default("LNP_RootGround_Get.Item", '(Type=Bone,Name="root")')
set_default("LNP_RootGround_Get.Space", "GlobalSpace")
set_default("LNP_RootGround_Set.Item", '(Type=Bone,Name="root")')
set_default("LNP_RootGround_Set.Space", "GlobalSpace")
set_default("LNP_RootGround_Set.bPropagateToChildren", "True")
set_default("LNP_RootGround_Set.Weight", "1.000000")
link("LNP_RootGround_Get.Transform.Translation.X", "LNP_RootGround_Set.Value.X")
link("LNP_RootGround_Get.Transform.Translation.Y", "LNP_RootGround_Set.Value.Y")
link("LNP_RootGround_Get.Transform.Translation.Z", "LNP_RootGround_Sub.A")
link("LNP_RootGround_Damp.Result", "LNP_RootGround_Sub.B")
link("LNP_RootGround_Sub.Result", "LNP_RootGround_Set.Value.Z")

# 실행 순서: Sequence.D → Store → Set → Return
# 재배선 도중에는 함수 그래프가 잠깐 Return에 닿지 않아 컨트롤러가 검증 에러를 로그로 남기고, UE Python은 그 로그를 예외로 올린다.
# 작업 자체는 끝난 상태이므로 예외는 기록만 하고, 최종 링크 집합으로 성공 여부를 판정한다.
def rewire(call, what):
    try:
        call()
        log.append("OK " + what)
    except Exception as e:
        log.append("WARN %s: %s" % (what, str(e).splitlines()[0]))


rewire(lambda: c.break_link("RigVMFunction_Sequence.D", "Return.ExecuteContext", True, False), "break D -> Return")
rewire(lambda: c.add_link("LNP_RootGround_Store.ExecuteContext", "LNP_RootGround_Set.ExecutePin", True, False), "link Store -> Set")
rewire(lambda: c.add_link("LNP_RootGround_Set.ExecutePin", "Return.ExecuteContext", True, False), "link Set -> Return")
rewire(lambda: c.add_link("RigVMFunction_Sequence.D", "LNP_RootGround_Store.ExecuteContext", True, False), "link D -> Store")

links = {l.get_source_pin().get_pin_path() + " -> " + l.get_target_pin().get_pin_path() for l in model.get_links()}
for expected in ("RigVMFunction_Sequence.D -> LNP_RootGround_Store.ExecuteContext",
                 "LNP_RootGround_Store.ExecuteContext -> LNP_RootGround_Set.ExecutePin",
                 "LNP_RootGround_Set.ExecutePin -> Return.ExecuteContext"):
    check(expected in links, "final link " + expected)
check("RigVMFunction_Sequence.D -> Return.ExecuteContext" not in links, "old link D -> Return removed")

if errors:
    finish()
    raise SystemExit("graph edit failed — asset not saved (reload the asset to discard)")

try:
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log.append("compiled")
except Exception as e:
    log.append("WARN compile: %s" % str(e).splitlines()[0])
check(unreal.EditorAssetLibrary.save_asset(CR_PATH, False), "save " + CR_PATH)
finish()
