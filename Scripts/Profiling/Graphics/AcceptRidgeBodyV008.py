"""V008 외관 채택을 별도 원본에 기록하고 제작 당시 검사/형상은 동결한다."""
import hashlib
import json
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[3]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_7_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def record(p):return {'path':p.relative_to(ROOT).as_posix(),'sha256':sha(p)}
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')


def main():
    assert not (AUTHOR/'Acceptance_V008.json').exists(),'Acceptance already recorded'
    prior=read(UNIT/'PreservationHashes.json');recovery=read(UNIT/'FinalReviewHashes_V008.json')
    assert len(prior)==534 and len(recovery)==168
    for row in prior+recovery:assert sha(ROOT/row['path'])==row['sha256'],row['path']
    for row in recovery:assert sha(ROOT/row['backup'])==row['sha256'],row['backup']
    config=read(AUTHOR/'Generation_V008.json');summary=read(UNIT/'FinalValidation_V008.json')
    poses=read(UNIT/'ActualPoseExport_V008.json');live=read(UNIT/'AcceptanceLiveState_V008.json')
    assert live['level']==config['target_level'] and not any(live[k] for k in ['pie','map_dirty','mesh_dirty'])
    assert summary['native_body_vertices']==73273 and summary['actual_body_self_crossing_pairs']==[0]*5
    assert summary['same_camera_count']==27 and poses['actor_count']==27 and summary['accepted'] is False
    merged={}
    for row in prior+recovery:
        path=row['path']
        if path.startswith('.context/') or path=='.gitattributes':continue
        key=path.casefold();assert key not in merged or merged[key]['sha256']==row['sha256']
        merged[key]={'path':path,'sha256':row['sha256']}
    for path in [UNIT/'FinalReviewHashes_V008.json',Path(__file__)]:
        merged[path.relative_to(ROOT).as_posix().casefold()]=record(path)
    frozen=sorted(merged.values(),key=lambda r:r['path'].casefold());freeze_path=AUTHOR/'FrozenInputs_V008.json'
    write(freeze_path,{'unit':'U5a-7','accepted_map':config['target_level'],'file_count':len(frozen),'files':frozen,
        'method':'deduplicate previous534 and immutable final168 sources; freeze recovery index and acceptance tool; mutable .context documents and .gitattributes excluded',
        'historical_recovery_note':'document sources may change after adoption; original recovery backup hashes remain authoritative'})
    acceptance={'unit':'U5a-7','accepted_visual':True,'decision_date':'2026-11-27',
        'user_instruction':'약간 어색하지만 판타지 부유섬/부유석 지형의 허용 범위로 V008 외관을 수용; 앞으로 부유석 느낌을 적극 활용',
        'decision':'GX-037','scope':'Keep V008 appearance; held4 bodies and connection plus one local Cliff09 backfill;27 poses/21 adopted owners',
        'level':config['target_level'],'frozen_generation':record(AUTHOR/'Generation_V008.json'),
        'frozen_inputs':dict(record(freeze_path),files=len(frozen)),'actual_pose_export':record(UNIT/'ActualPoseExport_V008.json'),
        'poses':[{'label':r['label'],'world_transform':r['actual_world_transform']} for r in poses['actors']],
        'outputs':config['outputs'],'body_outputs':config['body_outputs'],'kept_connection':config['kept_connection'],
        'actual_validation':record(UNIT/'FinalValidation_V008.json'),'actual_native_geometry':record(UNIT/'NativeGeometryCheck_V008.json'),
        'acceptance_live_state':record(UNIT/'AcceptanceLiveState_V008.json'),'preservation_pass':True,'collision_or_PIE_pass':False,
        'remaining_review':{'legacy_StairRamp3_samples':31,'R01_whole_access':'REVIEW; finite81ray union misses0 is bounded evidence',
            'rendering':'existing RHI reservation and Lumen atlas budget reviews'},
        'next_unit':'scope confirmation required; no new geometry/map/collision/crust/Support/bake/PIE started',
        'history_policy':'Generation_V008 and FinalValidation accepted=false remain production-time records; this sidecar owns current adoption',
        'previous_acceptance':record(AUTHOR/'Acceptance_V007.json')}
    write(AUTHOR/'Acceptance_V008.json',acceptance)
    handoff=AUTHOR/'Handoff_V008.md'
    handoff.write_text('''# U5a V008 채택·판타지 부유석 표현 기준

사용자가 V008 외관을 판타지 지형의 허용 범위로 수용했다(2026-11-27, GX-037).

- 현재 채택 원본은 Acceptance_V008.json과 FrozenInputs_V008.json이다. Generation_V008/FinalValidation의 accepted=false는 제작 이력으로 보존한다.
- 기존4몸체/연결/26자세에 국소 Cliff09 보강1개를 더한27자세·73,273정점·실제 자기 교차0·동일27카메라 비교 결과를 유지한다.
- 부유석·공중 암석은 LootNPop 전체 맵에서 의도적인 판타지 표현으로 적극 활용한다. 시각적 받침이 없다는 이유만으로 메우거나 삭제하지 않는다.
- 열린 내부/뒷면 노출·실제 통행 막힘·충돌/Support의 불일치는 부유 연출과 구별한다. 기존31표본과 전체R01 접근은 REVIEW이며 충돌/PIE 통과가 아니다.
- 기존 Cull A~D·채택 배치는 과거 재현 근거로 유지하고 자동 재실행/복원하지 않는다. 다음 단위는 범위를 확인한 뒤 새 사본에서 진행한다.
- 다음 보존 목록은 FrozenInputs_V008.files와 AcceptedHandoffHashes_V008.json을 경로/해시로 대조하고 중복 제거한다. 다음 manifest 이전 채택본은 V008/실제27자세/몸체5+연결1이다.

최신 상태는 .context/Graphics/Current.md와 analysis/Phase01_U5aBoundaryReview.md를 따른다.
''',encoding='utf-8')
    validation=UNIT/'AcceptanceValidation_V008.json'
    write(validation,{'accepted_visual':True,'decision_date':'2026-11-27','level':config['target_level'],
        'frozen_file_count':len(frozen),'previous534_and_final168_hashes_pass':True,'live_map_and_new_mesh_dirty':False,
        'actual_pose_count':27,'native_body_vertices':73273,'collision_or_PIE_pass':False,'live_state':record(UNIT/'AcceptanceLiveState_V008.json')})
    paths=[ROOT/r['path'] for r in recovery if r['path'].startswith('Content/') or r['path'].startswith('Scripts/')]
    paths += [AUTHOR/'Acceptance_V008.json',freeze_path,handoff,UNIT/'AcceptanceLiveState_V008.json',validation,
        UNIT/'ActualPoseExport_V008.json',UNIT/'FinalValidation_V008.json',UNIT/'NativeGeometryCheck_V008.json',
        UNIT/'NativeImport_V008.json',sorted(UNIT.glob('NativeVerify_*.json'))[-1],sorted(UNIT.glob('Verify_*.json'))[-1]]
    backup=UNIT/'AcceptedHandoff_V008';rows=[]
    for path in dict.fromkeys(paths):
        dest=backup/path.relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
        assert sha(dest)==sha(path);rows.append(dict(record(path),backup=dest.relative_to(ROOT).as_posix()))
    write(UNIT/'AcceptedHandoffHashes_V008.json',rows)
    combined={r['path'].casefold():r['sha256'] for r in frozen}
    for row in rows:
        key=row['path'].casefold();assert key not in combined or combined[key]==row['sha256'];combined[key]=row['sha256']
    print(json.dumps({'accepted_visual':True,'frozen_files':len(frozen),'handoff_files':len(rows),'next_unique_preservation_files':len(combined)}))


if __name__=='__main__':main()
