"""Phase 5 §37: 검증된 Lyra 프레임 후보를 시험 몽타주/Chooser/BP에 연결한다.

PIE 종료 후 BuildUAFLyraWeaponFrame.py의 보고서를 검토하고 실행한다.
시험 폴더의 몽타주6개/Chooser 사본과 시험 BP만 저장한다. 원본/공유/맵은 저장하지 않는다.
시퀀스는 생성·교체·삭제하지 않으며 AG의 Idle/AO 연결은 수동으로 변경한다.
"""
import json
from pathlib import Path
import unreal

if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
    raise RuntimeError("Stop PIE before connecting candidate montages.")

AL = unreal.EditorAssetLibrary
ROOT = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimationOverhaul/2026-10-29_UAF52RifleOverlay"
candidate = json.loads((ROOT / "LyraWeaponFrameCandidate_2026-11-01.json").read_text(encoding="utf-8"))
if not candidate["checks"] or any(row["maxSourceBarrelDifferenceDeg"] > 0.5
                                  or row["maxRightGripChangeVsPreviousCm"] > 0.2
                                  or row["maxRightHandPositionChangeCm"] > 0.2 for row in candidate["checks"]):
    raise RuntimeError("Candidate offline direction/grip checks must pass before connecting it.")
FOLDER = "/Game/Characters/UAFSpike/LyraWeaponContract"
GENERATED = "/Game/Characters/Retargeted/UAFLyraFrame"
BASE_CHOOSER = "/Game/Characters/CHT_Montage"
CHOOSER = FOLDER + "/CHT_MontageLyraFrame"
BP_PATH = "/Game/Characters/UAFSpike/BP_PlayerUAFSpike"
NAMES = ("AM_MM_Rifle_Reload", "AM_MM_Rifle_Fire", "AM_MM_Dash_Forward", "AM_MM_Dash_Backward", "AM_MM_Dash_Left", "AM_MM_Dash_Right")


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing required asset: " + path)
    return asset


def own_copy(path, original):
    asset = load(path) if AL.does_asset_exist(path) else AL.duplicate_asset(original, path)
    if asset is None:
        raise RuntimeError("Could not create trial asset: " + path)
    return asset


registry = unreal.AssetRegistryHelpers.get_asset_registry()
dependency_options = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True)
base_dependencies = {str(path) for path in (registry.get_dependencies(BASE_CHOOSER, dependency_options) or [])}
mapping = {"/Game/Characters/Retargeted/Lyra/"+name+"_UEFN": FOLDER+"/"+name+"_LyraFrame" for name in NAMES}
if not set(mapping).issubset(base_dependencies):
    raise RuntimeError("Shared chooser does not contain the expected six original montages.")
assets = []
rows = []
for original_path, destination in mapping.items():
    original = load(original_path)
    montage = own_copy(destination, original_path)
    montage.modify()
    tracks = montage.get_editor_property("slot_anim_tracks")
    segments_report = []
    for index, track in enumerate(tracks):
        anim_track = track.get_editor_property("anim_track")
        segments = anim_track.get_editor_property("anim_segments")
        for segment_index, segment in enumerate(segments):
            previous = segment.get_editor_property("anim_reference")
            if not isinstance(previous, unreal.AnimSequence):
                raise RuntimeError("Expected sequence segments in " + destination)
            name = previous.get_name().removesuffix("_UEFN").removesuffix("_LyraFrame")
            replacement = load(GENERATED+"/"+name+"_LyraFrame")
            if abs(previous.get_play_length()-replacement.get_play_length()) > 0.0001:
                raise RuntimeError("Candidate sequence length differs in " + destination)
            segment.set_editor_property("anim_reference", replacement)
            segments[segment_index] = segment
            segments_report.append({"slot": str(track.get_editor_property("slot_name")),
                                    "sequence": replacement.get_path_name(),
                                    "start": segment.get_editor_property("anim_start_time"),
                                    "end": segment.get_editor_property("anim_end_time")})
        anim_track.set_editor_property("anim_segments", segments)
        track.set_editor_property("anim_track", anim_track)
        tracks[index] = track
    montage.set_editor_property("slot_anim_tracks", tracks)
    if abs(original.get_play_length()-montage.get_play_length()) > 0.0001:
        raise RuntimeError("Candidate montage length changed unexpectedly.")
    assets.append(montage)
    rows.append({"original": original_path, "candidate": destination, "segments": segments_report})

chooser = own_copy(CHOOSER, BASE_CHOOSER)
command = "LNP.Editor.ReplaceAssetRefs " + CHOOSER + " " + " ".join(old+"="+new for old, new in mapping.items())
unreal.SystemLibrary.execute_console_command(None, command)
for asset in assets + [chooser]:
    if not AL.save_loaded_asset(asset):
        raise RuntimeError("Could not save trial montage/chooser: " + asset.get_path_name())
chooser_file = Path(unreal.Paths.project_content_dir()).resolve() / (CHOOSER.removeprefix("/Game/")+".uasset")
registry.scan_files_synchronous([str(chooser_file)], True)
dependencies = {str(path) for path in (registry.get_dependencies(CHOOSER, dependency_options) or [])}
expected = (base_dependencies-set(mapping)) | set(mapping.values())
if dependencies != expected:
    raise RuntimeError("Candidate chooser dependency replacement did not match; BP was not changed.")

bp = load(BP_PATH)
cdo = unreal.get_default_object(unreal.load_class(None, BP_PATH+".BP_PlayerUAFSpike_C"))
cdo.modify()
cdo.set_editor_property("montage_chooser", chooser)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(unreal.load_class(None, BP_PATH+".BP_PlayerUAFSpike_C"))
if cdo.get_editor_property("montage_chooser") != chooser:
    raise RuntimeError("Spike BP chooser connection did not persist through compilation.")
if not AL.save_loaded_asset(bp):
    raise RuntimeError("Could not save the spike BP.")
report = {"scope": "Trial six montage/chooser copies and spike BP only; sequence objects and source/shared/map assets unchanged",
          "saved": [asset.get_path_name() for asset in assets+[chooser, bp]], "montages": rows,
          "chooserDependencies": sorted(dependencies),
          "pending": "Set AG Sequence Player to MM_Rifle_Idle_ADS_LyraFrame and Blend Space Player to AO_MM_Rifle_Idle_ADS_LyraFrame; keep Alpha=1, Compile/Save AG before PIE"}
output = ROOT / "LyraFrameMontages_2026-11-01.json"
output.write_text(json.dumps(report, indent=2), encoding="utf-8")
print("[UAFLyraFrameMontages] Saved " + str(output))
