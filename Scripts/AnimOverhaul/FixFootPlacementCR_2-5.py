# 애니메이션 개선 2-5: GASP CR_Biped_FootPlacement 복제본을 LootNPop에서 쓸 수 있게 고친다(LootNPop 에디터 Python).
#   py "<저장소>/Scripts/AnimOverhaul/FixFootPlacementCR_2-5.py" [CR 경로]
# 선행: LNP.Editor.GASPRef <spec>로 GASP CR을 /Game/Characters/ControlRigs/CR_Biped_FootPlacement_UEFN에 복제(Saved/AnimOverhaul/GASPRef_2-5_clone_spec.json).
#
# 1) 함수 참조 재연결 — 복제본의 함수 참조 노드가 GASP 원래 경로(/Game/Blueprints/ControlRigs/CR_Biped_FootPlacement)의
#    함수 라이브러리를 가리킨다(LNP.Editor.GASPRef의 참조 재연결은 하드 참조만 바꾼다). 이름이 같은 자기 라이브러리 함수로 바꾼다.
# 2) 구면 수정 — CR이 월드 축 성분(.X/.Y/.Z)을 직접 쓰는 세 곳(근거: design/GASPReference.md §2.12).
#    A. UpdateRootDamper·Reset: 루트 높이 감쇠를 "월드 Z 높이"가 아니라 "컴포넌트 Up 방향 지연량(e)"으로 바꾼다.
#       e -= (자기 이동량의 지면 법선 성분)·Up(자기 이동량 = 프레임 이동 - base 이동량, 아래 D), e는 0으로 스프링 감쇠, 루트 리그 Z = e.
#    B. Update Foot Pinning: 핀 위치를 "월드 XY는 핀, 월드 Z는 애니"로 섞던 것을 리그 공간에서 섞은 결과(To World_3) 그대로 쓴다.
#    C. ComputePinnedToeTransform: 같은 섞기와 발끝 Yaw 보정을 리그 공간에서 하고 마지막에 월드로 옮긴다.
#    D. UpdateRootDamper: base가 옮긴 몫을 프레임 이동에서 뺀다(GASP 식은 base 이동의 접선 성분을 지연량에 더한다).
#    움직이는 base 위에서는 핀 고정·루트 감쇠를 ABP 쪽(ULNPAnimInstance::IsOnStaticGround)에서 끈다.
# 3) 컴파일·저장. 다시 돌려도 된다(LNP_ 노드가 있으면 구면 수정은 건너뛴다).
import json
import sys

import unreal

CR_PATH = sys.argv[1] if len(sys.argv) > 1 else "/Game/Characters/ControlRigs/CR_Biped_FootPlacement_UEFN"
CR_NAME = CR_PATH.split("/")[-1]
OUT = unreal.Paths.project_saved_dir() + "AnimOverhaul/FixFootPlacementCR_2-5.json"

G_DAMPER = "RigVMFunctionLibrary.UpdateRootDamper.New Function_ContainedGraph"
G_RESET = "RigVMFunctionLibrary.Reset.New Function_ContainedGraph"
G_PINNING = "RigVMFunctionLibrary.Update Foot Pinning.Update Foot Pinning_ContainedGraph"
G_PINNED_TOE = "RigVMFunctionLibrary.ComputePinnedToeTransform.New Function_ContainedGraph"

S_TO_WORLD_LOC = "/Script/ControlRig.RigUnit_ToWorldSpace_Location"
S_TO_WORLD_XF = "/Script/ControlRig.RigUnit_ToWorldSpace_Transform"
S_TO_RIG_XF = "/Script/ControlRig.RigUnit_ToRigSpace_Transform"
S_VEC_SUB = "/Script/RigVM.RigVMFunction_MathVectorSub"
S_VEC_UNIT = "/Script/RigVM.RigVMFunction_MathVectorUnit"
S_VEC_DOT = "/Script/RigVM.RigVMFunction_MathVectorDot"
S_DOUBLE_NEG = "/Script/RigVM.RigVMFunction_MathDoubleNegate"

log = []
errors = []

bp = unreal.load_asset(CR_PATH)
models = {m.get_path_name().split(":")[-1]: m for m in bp.get_all_models()}


def ctrl(graph_name):
    return bp.get_controller(models[graph_name])


def check(ok, what):
    log.append(("OK " if ok else "FAIL ") + what)
    if not ok:
        errors.append(what)


def link(c, a, b):
    check(c.add_link(a, b, True, False), "link %s -> %s" % (a, b))


def unlink(c, a, b):
    check(c.break_link(a, b, True, False), "break %s -> %s" % (a, b))


def set_default(c, pin, value):
    check(c.set_pin_default_value(pin, value, False, True, False, False), "default %s = %s" % (pin, value))


def add_unit(c, struct_path, name, x, y):
    node = c.add_unit_node_from_struct_path(struct_path, "Execute", unreal.Vector2D(x, y), name, True, False)
    check(node is not None, "add %s (%s)" % (name, struct_path.split(".")[-1]))
    return node


def find_local_identifier(c, function_name):
    """복제본 함수 라이브러리의 함수 식별자. 호스트 경로 표기는 생성 클래스·BP 경로 순으로 시도한다."""
    for host in ("%s.%s_C" % (CR_PATH, CR_NAME), "%s.%s" % (CR_PATH, CR_NAME)):
        identifier = c.find_graph_function_identifier(host, function_name)
        if identifier is not None and str(identifier.get_editor_property("library_node_path")).startswith(CR_PATH + "."):
            return identifier
    return None


# --- 1) 함수 참조를 자기 라이브러리로 ---
library = bp.get_local_function_library()
swapped = 0
for graph_name, model in models.items():
    c = bp.get_controller(model)
    for node in list(model.get_nodes()):
        if not isinstance(node, unreal.RigVMFunctionReferenceNode):
            continue
        pointer = node.get_referenced_function_header().get_editor_property("library_pointer")
        library_node_path = str(pointer.get_editor_property("library_node_path"))
        if library_node_path.startswith(CR_PATH + "."):
            continue
        function_name = library_node_path.split(".")[-1]
        if library.find_function(function_name) is None:
            check(False, "no local function %s" % function_name)
            continue
        identifier = find_local_identifier(c, function_name)
        if identifier is None:
            check(False, "no identifier for %s" % function_name)
            continue
        ok = c.swap_function_reference(node, identifier, True, True, False)
        check(ok, "swap %s/%s -> %s" % (graph_name, node.get_name(), function_name))
        swapped += 1
log.append("swapped %d function references" % swapped)
if errors:
    json.dump({"errors": errors, "log": log}, open(OUT, "w"), indent=1, ensure_ascii=False)
    raise SystemExit("function reference swap failed")

# --- 2) 구면 수정 ---
damper_nodes = {n.get_name() for n in models[G_DAMPER].get_nodes()}
if "LNP_Up" in damper_nodes:
    log.append("sphere fixes already applied, skipped")
else:
    # A. UpdateRootDamper — 컴포넌트 Up(월드) = ToWorld(0,0,1) - ToWorld(0,0,0)
    c = ctrl(G_DAMPER)
    add_unit(c, S_TO_WORLD_LOC, "LNP_UpWorld", 2600, 900)
    set_default(c, "LNP_UpWorld.Value", "(X=0.000000,Y=0.000000,Z=1.000000)")
    add_unit(c, S_VEC_SUB, "LNP_UpSub", 2800, 900)
    link(c, "LNP_UpWorld.World", "LNP_UpSub.A")
    link(c, "To World_4.World", "LNP_UpSub.B")
    add_unit(c, S_VEC_UNIT, "LNP_Up", 3000, 900)
    link(c, "LNP_UpSub.Result", "LNP_Up.Value")

    # 시퀀스 A: base 이동량은 더하지 않는다(원래 GroundMovementWorldDelta.Translation.Z). 아래 D가 base 몫을 이동량에서 뺀다
    unlink(c, "VariableNode_20.Value.Translation.Z", "Add_2.A")
    set_default(c, "Add_2.A", "0.000000")

    # 시퀀스 C: e -= (프레임 이동의 지면 법선 성분) · Up (원래 접선 성분의 월드 Z를 더하고 현재 월드 Z로 감쇠)
    add_unit(c, S_VEC_DOT, "LNP_DotNormal", 3200, 1100)
    link(c, "Scale.Result", "LNP_DotNormal.A")
    link(c, "LNP_Up.Result", "LNP_DotNormal.B")
    add_unit(c, S_DOUBLE_NEG, "LNP_NegNormal", 3400, 1100)
    link(c, "LNP_DotNormal.Result", "LNP_NegNormal.Value")
    unlink(c, "Subtract_2.Result.Z", "Add_1.A")
    link(c, "LNP_NegNormal.Result", "Add_1.A")

    # 감쇠 목표 = 0(지연 없음). 원래 현재 컴포넌트 월드 Z
    unlink(c, "To World_4.World.Z", "Critical Spring Damp.Target")
    set_default(c, "Critical Spring Damp.Target", "0.000000")

    # 루트 리그 Z = e. 원래 FromWorld((0,0,감쇠 월드 Z)).Z
    unlink(c, "From World.Global.Z", "Make_1.Elements.Z")
    link(c, "Critical Spring Damp.Value", "Make_1.Elements.Z")

    # 감쇠가 꺼지면 e = 0. 원래 현재 컴포넌트 월드 Z
    unlink(c, "To World_3.World.Z", "VariableNode_15.Value")
    set_default(c, "VariableNode_15.Value", "0.000000")

    # Reset도 e = 0
    c = ctrl(G_RESET)
    unlink(c, "To World.World.Z", "VariableNode_6.Value")
    set_default(c, "VariableNode_6.Value", "0.000000")

    # B. Update Foot Pinning — 핀 위치 = 리그 공간에서 (핀 접평면 위치, 애니 높이)를 섞은 To World_3 전체
    c = ctrl(G_PINNING)
    unlink(c, "RerouteNode_17.Value", "VariableNode_2.Value.Translation.X")
    unlink(c, "RerouteNode_18.Value", "VariableNode_2.Value.Translation.Y")
    unlink(c, "VariableNode_8.Value.Translation.Z", "VariableNode_2.Value.Translation.Z")
    link(c, "To World_3.World", "VariableNode_2.Value.Translation")

    # C. ComputePinnedToeTransform — 리그 공간에서 섞고 Yaw 보정한 뒤 월드로
    c = ctrl(G_PINNED_TOE)
    add_unit(c, S_TO_RIG_XF, "LNP_PinRig", -200, 600)
    link(c, "Entry.ToePinTransformWorld", "LNP_PinRig.Value")
    unlink(c, "To World_1.World.Rotation", "RigVMFunction_MathQuaternionGetAxis.Quaternion")
    link(c, "Entry.ToeAnimatedTransformGlobal.Rotation", "RigVMFunction_MathQuaternionGetAxis.Quaternion")
    unlink(c, "Entry.ToePinTransformWorld.Rotation", "RigVMFunction_MathQuaternionGetAxis_1.Quaternion")
    link(c, "LNP_PinRig.Global.Rotation", "RigVMFunction_MathQuaternionGetAxis_1.Quaternion")
    unlink(c, "To World_1.World.Rotation", "Multiply.B")
    link(c, "Entry.ToeAnimatedTransformGlobal.Rotation", "Multiply.B")
    add_unit(c, S_TO_WORLD_XF, "LNP_ResultWorld", 900, 600)
    link(c, "LNP_PinRig.Global.Translation.X", "LNP_ResultWorld.Value.Translation.X")
    link(c, "LNP_PinRig.Global.Translation.Y", "LNP_ResultWorld.Value.Translation.Y")
    link(c, "Entry.ToeAnimatedTransformGlobal.Translation.Z", "LNP_ResultWorld.Value.Translation.Z")
    link(c, "Multiply.Result", "LNP_ResultWorld.Value.Rotation")
    unlink(c, "Entry.ToePinTransformWorld.Translation.X", "Return.ComputePinnedToeTransformWorld.Translation.X")
    unlink(c, "Entry.ToePinTransformWorld.Translation.Y", "Return.ComputePinnedToeTransformWorld.Translation.Y")
    unlink(c, "To World.World.Z", "Return.ComputePinnedToeTransformWorld.Translation.Z")
    unlink(c, "Multiply.Result", "Return.ComputePinnedToeTransformWorld.Rotation")
    link(c, "LNP_ResultWorld.World", "Return.ComputePinnedToeTransformWorld")

# D. UpdateRootDamper — 지연량은 "자기 이동량(base가 옮긴 몫을 뺀 프레임 이동)"의 지면 법선 성분만 뺀다.
#    GASP 식은 base 이동량의 접선 성분을 지연량에 더하는 셈이라, 움직이는 발판(프레임당 30cm) 위에서 Mover 바닥 법선이
#    몇 도만 기울어도 루트가 수 cm 튀었다(2-5 PIE 전신 움찔). 정확한 식: e -= (n·D_own)(n·Up), D_own = D - base.T
damper_nodes = {n.get_name() for n in models[G_DAMPER].get_nodes()}
if "LNP_OwnDelta" in damper_nodes:
    log.append("own-delta fix already applied, skipped")
else:
    c = ctrl(G_DAMPER)
    if "LNP_DotBase" in damper_nodes:  # 첫 수정본(base·Up을 더하던 것)에서 옮겨 온다
        unlink(c, "LNP_DotBase.Result", "Add_2.A")
        check(c.remove_node_by_name("LNP_DotBase", True, False), "remove LNP_DotBase")
        set_default(c, "Add_2.A", "0.000000")
    add_unit(c, S_VEC_SUB, "LNP_OwnDelta", 3200, 700)
    link(c, "Subtract_1.Result", "LNP_OwnDelta.A")
    link(c, "VariableNode_20.Value.Translation", "LNP_OwnDelta.B")
    unlink(c, "Subtract_1.Result", "Dot__.A")
    link(c, "LNP_OwnDelta.Result", "Dot__.A")

# --- 3) 남은 GASP 경로 참조 확인 → 컴파일 → 저장 ---
remaining = 0
for model in bp.get_all_models():
    for node in model.get_nodes():
        if isinstance(node, unreal.RigVMFunctionReferenceNode):
            pointer = node.get_referenced_function_header().get_editor_property("library_pointer")
            if not str(pointer.get_editor_property("library_node_path")).startswith(CR_PATH + "."):
                remaining += 1
log.append("remaining foreign function references: %d" % remaining)

if not errors and remaining == 0:
    bp.recompile_vm()
    saved = unreal.EditorAssetLibrary.save_asset(CR_PATH, only_if_is_dirty=False)
    log.append("compiled, saved=%s" % saved)
else:
    log.append("not saved (errors)")

json.dump({"errors": errors, "log": log}, open(OUT, "w"), indent=1, ensure_ascii=False)
