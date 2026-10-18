"""오프라인 배치 리타깃 전용 리타기터 RTG_Lyra_to_UEFN_Offline을 만들거나 설정을 다시 맞춘다(단위 2-0a).

LootNPop 에디터 Output Log의 Cmd 창에서 실행한다.
    py "<프로젝트>/Scripts/AnimRetarget/SetupOfflineRetargeter.py"
런타임 리타기터 RTG_Lyra_to_GASP(적 VisualMesh가 사용)는 건드리지 않고, 복사본에 세 가지를 고친다.
- Root Motion op의 SourceRoot·TargetRoot = root, TargetPelvis = pelvis. 비어 있으면 op가 아무것도 하지 않아
  루트 이동량이 골반에 남고 root가 원점에 고정된다(루트모션 소실).
- 대상 LeftToe·RightToe 체인 매핑 해제. 원본 다리 체인에 매핑돼 ball_l/r가 최대 약 100° 흔들렸다.
  Lyra IK 리그에는 발가락 체인이 없다.
- 스택 맨 앞에 Copy Base Pose op. 본 이름으로 원본 로컬 포즈를 복사해, 체인 밖 본(발가락·트위스트)이
  기준 포즈에 멈추지 않고 Lyra 애니를 따른다. 전역 설정의 bCopyBasePose는 5.8 op 스택에서 무시된다.
저장하지 않는다. 결과를 확인한 뒤 이 에셋만 저장한다.
"""
import json
from pathlib import Path
import unreal

SRC = "/Game/Characters/Retargeters/RTG_Lyra_to_GASP"
DST = "/Game/Characters/Retargeters/RTG_Lyra_to_UEFN_Offline"
OUT = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimRetarget" / "SetupOfflineRetargeter.json"

res = {}
try:
    if not unreal.EditorAssetLibrary.does_asset_exist(DST):
        unreal.EditorAssetLibrary.duplicate_asset(SRC, DST)
        res["duplicated"] = True
    rc = unreal.IKRetargeterController.get_controller(unreal.load_asset(DST))
    for i in range(rc.get_num_retarget_ops()):
        if str(rc.get_op_name(i)) == "Root Motion":
            oc = rc.get_op_controller(i)
            oc.set_source_root_bone("root")
            oc.set_target_root_bone("root")
            oc.set_target_pelvis_bone("pelvis")
            res["root_motion"] = oc.get_settings().export_text()
    for ch in ("LeftToe", "RightToe"):
        rc.set_source_chain("None", ch)
        res["map_" + ch] = str(rc.get_source_chain(ch))
    names = [str(rc.get_op_name(i)) for i in range(rc.get_num_retarget_ops())]
    if "Copy Base Pose" not in names:
        rc.add_retarget_op("/Script/IKRig.IKRetargetCopyBasePoseOp")
        rc.move_retarget_op_in_stack(rc.get_num_retarget_ops() - 1, 0)
    res["ops"] = [str(rc.get_op_name(i)) for i in range(rc.get_num_retarget_ops())]
except Exception:
    import traceback
    res["error"] = traceback.format_exc()

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(res, indent=1, ensure_ascii=False), encoding="utf-8")
unreal.log("SetupOfflineRetargeter: ops " + str(res.get("ops")))
