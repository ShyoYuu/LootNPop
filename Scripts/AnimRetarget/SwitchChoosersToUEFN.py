"""Chooser 테이블이 가리키는 Lyra 에셋을 UEFN 사본으로 바꾼다(애니메이션 개선 단위 2-0b).

LootNPop 에디터 Output Log의 Cmd 창에서 실행한다.
    py "<프로젝트>/Scripts/AnimRetarget/SwitchChoosersToUEFN.py"
Chooser 결과 행은 MCP·Python으로 쓸 수 없어 C++ 에디터 명령 LNP.Editor.ReplaceAssetRefs를 부른다.
- CHT_PSD_UEFN(CHT_PSD_Lyra 복제본): PSD 4개를 Characters/MotionMatchingData/PSD_*_UEFN으로(Jog → Run).
- CHT_Montage: 의존하는 Lyra 애니(Lyra_Mannequin/Animations, Weapons/*/Animations의 캐릭터 몽타주)를
  Retargeted/Lyra/<이름>_UEFN으로. 사본이 없으면 교체하지 않고 보고서에 남긴다.
저장하지 않는다. 보고서를 확인한 뒤 두 테이블만 저장한다. 결과는 Saved/AnimRetarget/SwitchChoosersToUEFN.json.
"""
import json
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimRetarget" / "SwitchChoosersToUEFN.json"
COPY_DIR = "/Game/Characters/Retargeted/Lyra"
LYRA_MM = "/Game/Characters/Lyra_Mannequin/MotionMatchingData"
UEFN_MM = "/Game/Characters/MotionMatchingData"
PSD_PAIRS = [
    (LYRA_MM + "/PSD_Idle_Lyra", UEFN_MM + "/PSD_Idle_UEFN"),
    (LYRA_MM + "/PSD_Walk_Lyra", UEFN_MM + "/PSD_Walk_UEFN"),
    (LYRA_MM + "/PSD_Jog_Lyra", UEFN_MM + "/PSD_Run_UEFN"),
    (LYRA_MM + "/PSD_Jump_Lyra", UEFN_MM + "/PSD_Jump_UEFN"),
]


def deps(path):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
    return [str(d) for d in (reg.get_dependencies(path, opts) or [])]


def replace(target, pairs):
    cmd = "LNP.Editor.ReplaceAssetRefs " + target + " " + " ".join(o + "=" + n for o, n in pairs)
    unreal.SystemLibrary.execute_console_command(None, cmd)


res = {}
try:
    replace(UEFN_MM + "/CHT_PSD_UEFN", PSD_PAIRS)
    res["psd_pairs"] = PSD_PAIRS

    pairs, no_copy = [], []
    for d in deps("/Game/Characters/CHT_Montage"):
        if not (d.startswith("/Game/Characters/Lyra_Mannequin/Animations/") or
                (d.startswith("/Game/Weapons/") and "/Animations/AM_" in d)):
            continue
        copy = COPY_DIR + "/" + d.rsplit("/", 1)[1] + "_UEFN"
        if unreal.EditorAssetLibrary.does_asset_exist(copy):
            pairs.append((d, copy))
        else:
            no_copy.append(d)
    replace("/Game/Characters/CHT_Montage", pairs)
    res["montage_pairs"] = len(pairs)
    res["montage_no_copy"] = no_copy
except Exception:
    import traceback
    res["error"] = traceback.format_exc()
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(res, indent=1, ensure_ascii=False), encoding="utf-8")
unreal.log("SwitchChoosersToUEFN: report written to " + str(OUT))
