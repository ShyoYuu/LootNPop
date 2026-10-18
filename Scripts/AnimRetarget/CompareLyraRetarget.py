"""Lyra 원본 애니와 UEFN 리타깃 사본을 대조한다(애니메이션 개선 단위 2-0a).

LootNPop 에디터 Output Log의 Cmd 창에서 실행한다.
    py "<프로젝트>/Scripts/AnimRetarget/CompareLyraRetarget.py" [목록 파일]
목록의 원본과 그것이 쓰는 세그먼트·샘플·가산 기준 포즈까지 따라가며, /Game/Characters/Retargeted/Lyra/<이름>_UEFN
사본과 노티파이(이름·클래스·시간·길이)·트랙·슬롯·세그먼트·섹션 이름·블렌드·루트모션·가산 설정·커브·싱크 마커·
블렌드 스페이스 샘플을 비교한다. 결과는 Saved/AnimRetarget/<목록 이름>_compare.json이다.
- mismatch: 결함으로 볼 차이. 비어 있어야 한다.
- slot_group_diff: 몽타주 그룹 차이. 그룹은 스켈레톤의 슬롯 그룹 등록에서 오므로 사본이 아니라 스켈레톤 문제다.
섹션 시작 시간과 다음 섹션 연결은 Python에서 읽을 수 없어 이름만 비교한다(배치는 몽타주 객체를 복제하므로 그대로 따라온다).
"""
import json, sys
from pathlib import Path
import unreal

HERE = Path(__file__).resolve().parent
LIST = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "LyraToUEFN_RetargetList.txt"
OUT_DIR = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimRetarget"
OUT = OUT_DIR / (LIST.stem + "_compare.json")
TARGET_PATH = "/Game/Characters/Retargeted/Lyra"
TARGET_SKELETON = "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin"
SUFFIX = "_UEFN"
AL = unreal.AnimationLibrary


def dst_of(src_path):
    return TARGET_PATH + "/" + src_path.rsplit("/", 1)[1] + SUFFIX


def pkg(obj):
    return obj.get_outermost().get_name() if obj else None


def safe(fn):
    try:
        return fn()
    except Exception as e:
        return "ERR " + str(e)


def summary(a):
    d = {"class": a.get_class().get_name(), "skeleton": pkg(a.get_editor_property("skeleton")),
         "length": round(a.get_play_length(), 4)}
    if isinstance(a, unreal.AnimSequenceBase):
        d["notify_names"] = sorted(str(n) for n in AL.get_animation_notify_event_names(a))
        d["notify_tracks"] = sorted(str(n) for n in AL.get_animation_notify_track_names(a))
        d["notifies"] = sorted(
            (str(ev.get_editor_property("notify_name")),
             (ev.get_editor_property("notify").get_class().get_name() if ev.get_editor_property("notify")
              else (ev.get_editor_property("notify_state_class").get_class().get_name()
                    if ev.get_editor_property("notify_state_class") else None)),
             safe(lambda: round(AL.get_anim_notify_event_trigger_time(ev), 4)),
             safe(lambda: round(AL.get_anim_notify_event_duration(ev), 4)))
            for ev in AL.get_animation_notify_events(a))
    if isinstance(a, unreal.AnimSequence):
        d["frames"] = AL.get_num_frames(a)
        d["enable_root_motion"] = a.get_editor_property("enable_root_motion")
        d["force_root_lock"] = a.get_editor_property("force_root_lock")
        d["root_motion_root_lock"] = str(a.get_editor_property("root_motion_root_lock"))
        d["additive_anim_type"] = str(a.get_editor_property("additive_anim_type"))
        d["ref_pose_type"] = str(a.get_editor_property("ref_pose_type"))
        d["ref_pose_seq"] = pkg(a.get_editor_property("ref_pose_seq"))
        d["rate_scale"] = a.get_editor_property("rate_scale")
        d["curves"] = sorted(str(c) for c in AL.get_animation_curve_names(a, unreal.RawCurveTrackTypes.RCT_FLOAT))
        d["sync_markers"] = sorted(str(m.get_editor_property("marker_name")) for m in AL.get_animation_sync_markers(a))
        # 첫 프레임과 끝 프레임 사이 root 본 이동량(root는 로컬 = 컴포넌트 공간)
        p0 = AL.get_bone_pose_for_frame(a, "root", 0, False)
        p1 = AL.get_bone_pose_for_frame(a, "root", max(d["frames"] - 1, 0), False)
        t = p1.translation - p0.translation
        d["root_motion_cm"] = round((t.x ** 2 + t.y ** 2 + t.z ** 2) ** 0.5, 2)
        d["root_motion_yaw"] = round(p1.rotation.rotator().yaw - p0.rotation.rotator().yaw, 2)
    if isinstance(a, unreal.AnimMontage):
        tracks = []
        for tr in a.get_editor_property("slot_anim_tracks"):
            segs = []
            for s in tr.get_editor_property("anim_track").get_editor_property("anim_segments"):
                segs.append([pkg(s.get_editor_property("anim_reference")),
                             round(s.get_editor_property("start_pos"), 4),
                             round(s.get_editor_property("anim_start_time"), 4),
                             round(s.get_editor_property("anim_end_time"), 4),
                             s.get_editor_property("anim_play_rate"),
                             s.get_editor_property("looping_count")])
            tracks.append({"slot": str(tr.get_editor_property("slot_name")), "segments": segs})
        d["slots"] = tracks
        d["sections"] = [str(a.get_section_name(i)) for i in range(a.get_num_sections())]
        d["blend_in"] = round(a.get_editor_property("blend_in").get_editor_property("blend_time"), 4)
        d["blend_out"] = round(a.get_editor_property("blend_out").get_editor_property("blend_time"), 4)
        d["blend_out_trigger"] = a.get_editor_property("blend_out_trigger_time")
        d["auto_blend_out"] = a.get_editor_property("enable_auto_blend_out")
        d["rate_scale"] = a.get_editor_property("rate_scale")
        d["group"] = str(a.get_group_name())
    if isinstance(a, unreal.BlendSpace):
        d["samples"] = sorted([pkg(s.get_editor_property("animation")),
                               round(s.get_editor_property("sample_value").x, 3),
                               round(s.get_editor_property("sample_value").y, 3)]
                              for s in a.get_editor_property("sample_data"))
        d["axes"] = [[str(p.get_editor_property("display_name")), p.get_editor_property("min"),
                      p.get_editor_property("max"), p.get_editor_property("grid_num")]
                     for p in a.get_editor_property("blend_parameters")]
    return d


def norm_ref(v):
    # 원본 참조를 기대되는 사본 경로로 바꾼다
    if isinstance(v, str) and v.startswith("/Game/") and not v.startswith(TARGET_PATH):
        return dst_of(v)
    return v


def compare(s, t):
    diffs, group = [], []
    for k in sorted(set(s) | set(t)):
        sv, tv = s.get(k), t.get(k)
        if k == "skeleton":
            if tv != TARGET_SKELETON:
                diffs.append([k, sv, tv])
            continue
        if k == "group":
            if sv != tv:
                group.append([sv, tv])
            continue
        if k == "ref_pose_seq":
            if norm_ref(sv) != tv or (tv is not None and not tv.startswith(TARGET_PATH)):
                diffs.append([k, sv, tv])
            continue
        if k == "slots":
            sv = [{"slot": x["slot"], "segments": [[norm_ref(g[0])] + g[1:] for g in x["segments"]]} for x in (sv or [])]
        if k == "samples":
            sv = sorted([[norm_ref(g[0])] + g[1:] for g in (sv or [])])
        if k == "root_motion_cm":
            # 두 메시의 골반 높이 비(약 0.97)만큼 줄어드는 것은 정상이다
            if abs((sv or 0) - (tv or 0)) > max(2.0, 0.15 * abs(sv or 0)):
                diffs.append([k, sv, tv])
            continue
        if k == "root_motion_yaw":
            if abs((sv or 0) - (tv or 0)) > 2.0:
                diffs.append([k, sv, tv])
            continue
        if sv != tv:
            diffs.append([k, sv, tv])
    return diffs, group


res = {"pairs": {}, "mismatch": {}, "slot_group_diff": {}, "missing_copy": []}
try:
    paths = [l.strip() for l in LIST.read_text(encoding="utf-8").splitlines()
             if l.strip() and not l.strip().startswith("#")]
    queue, seen = list(paths), set()
    while queue:
        sp = queue.pop(0)
        if sp in seen:
            continue
        seen.add(sp)
        src = unreal.load_asset(sp)
        if src is None or not isinstance(src, unreal.AnimationAsset):
            continue
        if not unreal.EditorAssetLibrary.does_asset_exist(dst_of(sp)):
            res["missing_copy"].append(sp)
            continue
        s, t = summary(src), summary(unreal.load_asset(dst_of(sp)))
        res["pairs"][sp] = {"src": s, "dst": t}
        diffs, group = compare(s, t)
        if diffs:
            res["mismatch"][sp] = diffs
        if group:
            res["slot_group_diff"][sp] = group[0]
        for tr in s.get("slots", []):
            queue.extend(g[0] for g in tr["segments"])
        queue.extend(g[0] for g in s.get("samples", []))
        if s.get("ref_pose_seq"):
            queue.append(s["ref_pose_seq"])
    res["compared"] = len(res["pairs"])
except Exception:
    import traceback
    res["error"] = traceback.format_exc()

OUT_DIR.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(res, indent=1, ensure_ascii=False), encoding="utf-8")
unreal.log("CompareLyraRetarget: compared %s pairs, %d mismatches, written to %s"
           % (res.get("compared"), len(res["mismatch"]), OUT))
