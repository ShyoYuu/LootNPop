"""사용자 V007 유지 결정을 별도 sidecar로 기록하고 이전 제작 기록/복구본을 동결한다."""
import hashlib
import json
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[3]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_6_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
def record(p):return {'path':p.relative_to(ROOT).as_posix(),'sha256':sha(p)}


def main():
    assert not (AUTHOR/'Acceptance_V007.json').exists(),'Acceptance already recorded'
    prior=read(UNIT/'PreservationHashes.json');recovery=read(UNIT/'FinalReviewHashes_V007.json')
    for row in prior+recovery:assert sha(ROOT/row['path'])==row['sha256'],row['path']
    for row in recovery:assert sha(ROOT/row['backup'])==row['sha256'],row['backup']
    merged={}
    for row in prior+recovery:
        path=row['path']
        if path.startswith('.context/') or path=='.gitattributes':continue
        key=path.casefold()
        assert key not in merged or merged[key]['sha256']==row['sha256'],path
        merged[key]={'path':path,'sha256':row['sha256']}
    for path in [UNIT/'FinalReviewHashes_V007.json',Path(__file__)]:
        merged[path.relative_to(ROOT).as_posix().casefold()]=record(path)
    frozen=list(sorted(merged.values(),key=lambda r:r['path'].casefold()))
    config=read(AUTHOR/'Generation_V007.json');summary=read(UNIT/'FinalValidation_V007.json')
    live=read(UNIT/'AcceptanceLiveState_V007.json');poses=read(UNIT/'ActualPoseExport_V007.json')
    assert live['current_level']==config['target_level'] and not live['pie'] and not live['dirty_assets']
    assert len(live['verified_poses'])==26 and summary['native_remaining_crossing_pairs']==[0,0,0,0]
    freeze_path=AUTHOR/'FrozenInputs_V007.json'
    write(freeze_path,{'unit':'U5a-6','accepted_map':config['target_level'],'file_count':len(frozen),'files':frozen,
        'method':'deduplicate previous417 and immutable final review sources; freeze recovery index and acceptance tool; mutable .context documents and .gitattributes excluded',
        'historical_recovery_note':'FinalReviewHashes contains mutable document source paths at review time; stored backup hashes remain valid; mutable sources may change after acceptance'})
    acceptance={'unit':'U5a-6','accepted_visual':True,'decision_date':'2026-11-26',
        'user_instruction':'V007 에서 변경한 세 몸체를 에디터에서 확인한 뒤 유지하고 다음 단위 진행',
        'scope':'Keep V007 current appearance/26 world poses/21 adopted owners; three repaired bodies and held underbody/connection; original UV/materials/core858 vertices',
        'level':config['target_level'],'frozen_generation':record(AUTHOR/'Generation_V007.json'),
        'frozen_inputs':dict(record(freeze_path),files=len(frozen)),
        'actual_pose_export':record(UNIT/'ActualPoseExport_V007.json'),
        'poses':[{'label':r['label'],'world_transform':r['actual_world_transform']} for r in poses['actors']],
        'outputs':config['outputs'],'kept_connection':config['kept_connection'],'kept_body':config['kept_body'],
        'actual_validation':record(UNIT/'FinalValidation_V007.json'),'actual_native_geometry':record(UNIT/'NativeGeometryCheck_V007.json'),
        'preservation_pass':True,'collision_or_PIE_pass':False,
        'remaining_review':{'legacy_StairRamp3_samples':31,'R01_boundary_sensitivity':True,'RHI_virtual_budget_warning':1},
        'next_unit':{'unit':'U5a-7','scope':'read-only R01 boundary and legacy StairRamp3 review; keep all51 spaces and actual fixed connection; new V008 only if actual geometric defect warrants local repair',
            'collision_crust_Support_bake_PIE':'not started in this review','new_V008_map_created':False},
        'history_policy':'Generation_V007/FinalValidation accepted=false and visual_review_pending remain production-time records; this sidecar owns current adoption',
        'previous_acceptance':record(AUTHOR/'Acceptance_V006.json')}
    write(AUTHOR/'Acceptance_V007.json',acceptance)
    handoff=AUTHOR/'Handoff_V007.md'
    handoff.write_text('''# U5a V007 채택·후속 경계 검토 기준

사용자가 에디터에서 변경한 세 몸체를 확인하고 V007 유지 및 다음 단위 진행을 승인했다(2026-11-26).

- 현재 채택 원본은 Acceptance_V007.json과 FrozenInputs_V007.json이다. V006 및 그 이전 채택/제작 기록은 그대로 보존한다.
- Generation_V007/FinalValidation의 accepted=false와 visual_review_pending은 검사 당시 이력이다. 채택 표시 목적으로 덮어쓰지 않는다.
- V007 실제 전체 보존/몸체73,111정점·교차0/닫힌 위상·연결858정점/두 재질 PASS,26자세 오차0·같은26카메라 비교 완료다.
- 몸체4개+연결1개는 NoCollision이며 시각 채택과 충돌/접근/PIE 통과는 별개다. 기존31표본/R01과 RHI 예산 경고는 REVIEW다.
- 다음 U5a-7은 실제 V007을 입력으로 R01 경계와 기존 StairRamp3의31표본을 정밀 검토한다. 고정 연결13,221표본/51공간을 유지하고 경로나 보호 폭을 빼지 않는다.
- 실제 결함에 국소 보정이 필요한 경우에만 새 V008 사본을 만든다. 현재 V008 맵/Generation_V008은 없다. 충돌/지각·Support/전체 베이크/PIE·다른 구역/세부 식생은 시작하지 않는다.
- 다음 보존 목록은 FrozenInputs_V007.files와 AcceptedHandoffHashes_V007.json을 경로/해시 대조로 중복 제거한다. 새 manifest 이전 채택본은 V007/Generation·Acceptance/실제26자세/몸체4+연결1이다.

현재 상태는 .context/Graphics/Current.md와 analysis/Phase01_U5aBoundaryReview.md를 따른다.
''',encoding='utf-8')
    validation=UNIT/'AcceptanceValidation_V007.json'
    write(validation,{'accepted_visual':True,'level':config['target_level'],'frozen_file_count':len(frozen),
        'actual_poses':26,'pose_error':0,'component_count':5,'relevant_dirty':[],
        'previous417_pass':True,'final118_source_and_backup_pass':True,'collision_or_PIE_pass':False,
        'live_state':record(UNIT/'AcceptanceLiveState_V007.json')})
    paths=[ROOT/r['path'] for r in recovery if (r['path'].startswith('Content/') or r['path'].startswith('Scripts/'))]
    paths += [AUTHOR/'Acceptance_V007.json',freeze_path,handoff,UNIT/'AcceptanceLiveState_V007.json',validation,
        UNIT/'ActualPoseExport_V007.json',UNIT/'NativeComponentState_V007.json',UNIT/'FinalValidation_V007.json',UNIT/'NativeGeometryCheck_V007.json',
        sorted(UNIT.glob('NativeVerify_*.json'))[-1],sorted(UNIT.glob('Verify_*.json'))[-1]]
    backup=UNIT/'AcceptedHandoff_V007';rows=[]
    for path in dict.fromkeys(paths):
        destination=backup/path.relative_to(ROOT);destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,destination);assert sha(path)==sha(destination)
        rows.append(dict(record(path),backup=destination.relative_to(ROOT).as_posix()))
    write(UNIT/'AcceptedHandoffHashes_V007.json',rows)
    combined={r['path'].casefold():r['sha256'] for r in frozen}
    for row in rows:
        key=row['path'].casefold();assert key not in combined or combined[key]==row['sha256']
        combined[key]=row['sha256']
    print(json.dumps({'frozen':len(frozen),'accepted_handoff':len(rows),'next_unique_preservation_files':len(combined),'accepted_visual':True}))


if __name__=='__main__':main()
