"""Lyra 애니를 UEFN 스켈레톤으로 오프라인 리타깃한다(애니메이션 개선 단위 2-0a).

LootNPop 에디터 Output Log의 Cmd 창에서 실행한다.
    py "<프로젝트>/Scripts/AnimRetarget/RetargetLyraToUEFN.py" [목록 파일] [대상 폴더]
목록 파일 기본값은 같은 폴더의 LyraToUEFN_RetargetList.txt, 대상 폴더 기본값은 /Game/Characters/Retargeted/Lyra다.
목록의 에셋과 그것이 참조하는 애니(몽타주 세그먼트·AO 샘플·가산 기준 포즈)를 RTG_Lyra_to_UEFN_Offline으로
복제·리타깃하고 이름 끝에 _UEFN을 붙인다. 결과는 Saved/AnimRetarget/<목록 이름>_report.json에 남긴다.
아무것도 저장하지 않는다. 저장은 보고서를 확인한 뒤 대상 폴더만 한다(소스 에셋 저장 금지, AN-018).

UE 5.8 엔진 동작 두 가지를 여기서 보정한다.
- retain_additive_flags가 사본의 RefPoseSeq를 참조 교체 전 값으로 되돌린다. 다른 시퀀스를 가산 기준으로 쓰는
  사본이 Lyra 원본을 가리키게 되므로, 같은 이름의 사본으로 다시 잇는다.
- FAnimTrack::ReplaceReferredAnimations가 세그먼트의 원본 시퀀스에도 참조 교체를 호출해, 메모리상의 Lyra 원본
  RefPoseSeq가 사본을 가리키게 된다(dirty 표시는 없다). 건드린 원본 패키지를 디스크에서 다시 읽어 되돌린다.
"""
import json, os, sys
from pathlib import Path
import unreal

HERE = Path(__file__).resolve().parent
LIST = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "LyraToUEFN_RetargetList.txt"
TARGET_PATH = sys.argv[2] if len(sys.argv) > 2 else "/Game/Characters/Retargeted/Lyra"
REPORT_DIR = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimRetarget"
REPORT = REPORT_DIR / (LIST.stem + "_report.json")

RETARGETER = "/Game/Characters/Retargeters/RTG_Lyra_to_UEFN_Offline"
SOURCE_MESH = "/Game/Characters/Lyra_Mannequin/Meshes/SKM_Manny"
TARGET_MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
SUFFIX = "_UEFN"


def pkg(obj):
    return obj.get_outermost().get_name() if obj else None


def dst_of(src_pkg):
    return TARGET_PATH + "/" + src_pkg.rsplit("/", 1)[1] + SUFFIX


def gather_sources(assets):
    """배치가 읽거나 건드리는 원본 애니 패키지(몽타주 세그먼트, 블렌드 스페이스 샘플, 가산 기준 포즈)."""
    seen = set()
    queue = list(assets)
    while queue:
        a = queue.pop()
        if a is None or not isinstance(a, unreal.AnimationAsset) or pkg(a) in seen:
            continue
        seen.add(pkg(a))
        if isinstance(a, unreal.AnimMontage):
            for tr in a.get_editor_property("slot_anim_tracks"):
                for s in tr.get_editor_property("anim_track").get_editor_property("anim_segments"):
                    queue.append(s.get_editor_property("anim_reference"))
        if isinstance(a, unreal.BlendSpace):
            for s in a.get_editor_property("sample_data"):
                queue.append(s.get_editor_property("animation"))
        if isinstance(a, unreal.AnimSequence):
            queue.append(a.get_editor_property("ref_pose_seq"))
    return sorted(seen)


res = {"list": str(LIST), "target_path": TARGET_PATH, "retargeter": RETARGETER}
try:
    paths = [l.strip() for l in LIST.read_text(encoding="utf-8").splitlines()
             if l.strip() and not l.strip().startswith("#")]
    datas, objs, missing = [], [], []
    for p in paths:
        ad = unreal.EditorAssetLibrary.find_asset_data(p)
        if ad is None or not ad.is_valid():
            missing.append(p)
        else:
            datas.append(ad)
            objs.append(ad.get_asset())
    res["requested"] = len(paths)
    res["missing"] = missing
    source_pkgs = gather_sources(objs)
    res["source_packages"] = len(source_pkgs)

    dirty_before = {str(p.get_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}

    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property("assets_to_retarget", datas)
    inputs.set_editor_property("source_mesh", unreal.load_asset(SOURCE_MESH))
    inputs.set_editor_property("target_mesh", unreal.load_asset(TARGET_MESH))
    inputs.set_editor_property("ik_retarget_asset", unreal.load_asset(RETARGETER))
    inputs.set_editor_property("target_path", TARGET_PATH)
    inputs.set_editor_property("use_source_path", False)
    inputs.set_editor_property("suffix", SUFFIX)
    inputs.set_editor_property("include_referenced_assets", True)
    inputs.set_editor_property("overwrite_existing_files", True)
    inputs.set_editor_property("retain_additive_flags", True)

    # 반환 목록에는 애니만 있다. 목록에 ABP를 넣으면 ABP 사본도 대상 폴더에 생기지만 여기 잡히지 않는다.
    created = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    res["created"] = sorted(str(a.package_name) for a in created)
    res["created_count"] = len(res["created"])

    # 대상 폴더 밖을 가리키는 가산 기준 포즈를 사본으로 다시 잇는다
    fixed, unresolved = [], []
    for p in res["created"]:
        a = unreal.load_asset(p)
        if not isinstance(a, unreal.AnimSequence):
            continue
        ref = a.get_editor_property("ref_pose_seq")
        if ref is None or pkg(ref).startswith(TARGET_PATH):
            continue
        if not unreal.EditorAssetLibrary.does_asset_exist(dst_of(pkg(ref))):
            unresolved.append([p, pkg(ref)])
            continue
        copy = unreal.load_asset(dst_of(pkg(ref)))
        a.modify()
        a.set_editor_property("ref_pose_seq", copy)
        fixed.append([p, pkg(ref), pkg(copy)])
    res["ref_pose_fixed"] = fixed
    res["ref_pose_unresolved"] = unresolved

    # 엔진이 메모리에서 바꾼 Lyra 원본을 디스크 상태로 되돌린다
    to_reload = [p for p in (unreal.find_package(n) for n in source_pkgs) if p is not None]
    unreal.EditorLoadingAndSavingUtils.reload_packages(to_reload, unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    res["reloaded"] = len(to_reload)
    leaked = []
    for p in source_pkgs:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.AnimSequence):
            ref = a.get_editor_property("ref_pose_seq")
            if ref is not None and pkg(ref).startswith(TARGET_PATH):
                leaked.append([p, pkg(ref)])
    res["source_refs_into_target_after_reload"] = leaked

    dirty_after = [str(p.get_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    res["newly_dirty_outside_target"] = sorted(
        d for d in dirty_after if d not in dirty_before and not d.startswith(TARGET_PATH))
except Exception:
    import traceback
    res["error"] = traceback.format_exc()

REPORT_DIR.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(res, indent=1, ensure_ascii=False), encoding="utf-8")
unreal.log("RetargetLyraToUEFN: report written to " + str(REPORT))
