"""리타깃 몽타주 사본에 남은 원본 참조 두 가지를 사본 쪽으로 다시 잇는다(단위 2-0b).

LootNPop 에디터 Output Log의 Cmd 창에서 RetargetLyraToUEFN.py 다음에 실행한다.
    py "<프로젝트>/Scripts/AnimRetarget/FixRetargetedMontages.py" [대상 폴더]
1. 블렌드 프로필: 배치 리타깃은 몽타주의 BlendProfileIn/Out을 원본 스켈레톤(Lyra SK_Mannequin)의
   서브오브젝트로 남긴다. 그대로 UEFN 메시에서 재생하면 본 인덱스가 맞지 않아
   UBlendProfile::FillBoneScalesArray에서 SkeletonToCompactPose.IsValidIndex ensure가 난다(대시 몽타주의 FastFeet).
   대상 스켈레톤의 같은 이름 프로필로 바꾸고, 없으면 None으로 비운다.
2. 원본 시퀀스 참조: 세그먼트는 사본을 가리키는데 배치 직후 저장본에는 원본 Lyra 시퀀스 의존성이 남는다.
   메모리의 참조는 이미 사본이라 교체 수는 0이지만, 명령의 PostEditChange가 링크 정보를 다시 만들어 의존성이 사라진다(2-0b 실측).
   그래서 의존성 중 원본 애니를 같은 이름 _UEFN 사본 쌍으로 LNP.Editor.ReplaceAssetRefs에 넘긴다.
저장하지 않는다. 보고서를 확인한 뒤 대상 폴더만 저장한다. 결과는 Saved/AnimRetarget/FixRetargetedMontages.json.
"""
import json, sys
from pathlib import Path
import unreal

TARGET_PATH = sys.argv[1] if len(sys.argv) > 1 else "/Game/Characters/Retargeted/Lyra"
SUFFIX = "_UEFN"
SOURCE_PREFIXES = ("/Game/Characters/Lyra_Mannequin/", "/Game/Weapons/")
OUT = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimRetarget" / "FixRetargetedMontages.json"


def deps(path):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
    return [str(d) for d in (reg.get_dependencies(path, opts) or [])]


res = {"target_path": TARGET_PATH, "fixed": [], "cleared": [], "source_refs": [], "source_no_copy": []}
try:
    for p in unreal.EditorAssetLibrary.list_assets(TARGET_PATH, recursive=True, include_folder=False):
        a = unreal.load_asset(p)
        if not isinstance(a, unreal.AnimMontage):
            continue
        pkg = p.split(".")[0]
        skel = a.get_editor_property("skeleton")
        for prop in ("blend_profile_in", "blend_profile_out"):
            bp = a.get_editor_property(prop)
            if bp is None or bp.get_outer() == skel:
                continue
            repl = unreal.find_object(None, skel.get_path_name() + ":" + bp.get_name())
            a.modify()
            a.set_editor_property(prop, repl)
            res["fixed" if repl else "cleared"].append([pkg, prop, bp.get_path_name(), repl.get_path_name() if repl else None])

        pairs = []
        for d in deps(pkg):
            if not d.startswith(SOURCE_PREFIXES) or "/Meshes/" in d:
                continue
            copy = TARGET_PATH + "/" + d.rsplit("/", 1)[1] + SUFFIX
            if unreal.EditorAssetLibrary.does_asset_exist(copy):
                pairs.append(d + "=" + copy)
            else:
                res["source_no_copy"].append([pkg, d])
        if pairs:
            unreal.SystemLibrary.execute_console_command(None, "LNP.Editor.ReplaceAssetRefs " + pkg + " " + " ".join(pairs))
            res["source_refs"].append([pkg, pairs])
except Exception:
    import traceback
    res["error"] = traceback.format_exc()
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(res, indent=1, ensure_ascii=False), encoding="utf-8")
unreal.log("FixRetargetedMontages: report written to " + str(OUT))
