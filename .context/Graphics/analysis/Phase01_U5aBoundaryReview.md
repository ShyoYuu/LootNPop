# Phase 1-BR U5a-7 · R01 내부 국소 보강·V008 검토

> 상태: 구현·실제 보존/형상 검사·동일27카메라 비교 완료 / V008 사용자 외관 채택
> 갱신: 2026-11-27
> 현재 채택: `ClosedBodyTrial_V001/Acceptance_V008.json`·`FrozenInputs_V008.json`

사용자가 V007을 확인하고 유지·다음 단위 진행을 승인했다. V007 채택은 Acceptance_V007에 기록했고 Generation_V007의 accepted=false·이전 V006 제작 이력은 동결했다. FrozenInputs529와 AcceptedHandoff29를 경로·해시로 중복 제거한 이번 보존 목록은534파일이다. V006·U4Layout_V011·Z1Bake_V010과 이전 U5a·공유 원본도 포함해 유지한다.

## 1. 실제 보정 범위

사용자가 R01 화면의 안쪽 열린 빈틈을 확인하고 카메라만 옮겼다. 카메라는 암석 앞 공간이므로 그 위치를 채우지 않았다. 시선이 처음 만나는 기존 `ASM_Z1_PlateauSouthCrown`의 `SM_Cliff_09`(원점21737.11/15022.82/13082.1cm) 내부에만 닫힌 몸체1개를 추가했다. 시선상 hull 진입291.386cm와 ED 표면444.944cm 사이368.165cm 지점이 보강 중심이다.

새 맵은 `U5aBody_V008/LVI_Ridge_EDTerrain_U5aBody_V008`, 활성 새 메시/액터는 `R01Closure_V006/SM_Ridge_U5a_R01RockBackfill_V006`·`BODY_R01RockBackfill_V006`이다. **맵 V008과 형상 V006은 별개다.** 162정점·320삼각형·실제 부피3.617643m³이며 원래 암석 hull 밖 정점0·시선 목표 내부·사용자 카메라 외부다. ED 외피와 기존4몸체/연결·두 재질/중앙858정점·기존26자세는 고정했고 새 identity 액터1개로 총27자세다. 전부 Static/NoCollision이며 충돌 역할을 승격하지 않았다.

전체 암석 폐쇄 V001은 교차 때문에, V003/V004는 자연스러운 바위 사이 공기를 메우고 밖으로 돌출해 제외했다. V005는 저작 winding을 수정한 V006으로 대체했다. 모든 오프라인 판과 반입 초안의 맵/메시 복구본은 남겼고 개별 에셋을 삭제하지 않았다. 일괄 재복셀화·기존 캡 변형은 하지 않았다.

## 2. 실제 검사와 한계

사용자 실행 `Verify_20261126_195007_966169.json`·`NativeVerify_20261126_195009_370108.json`은 전체 보존 PASS·몸체73,273정점/146,126삼각형·최대 반입 오차0.004367cm·전체 dirty없음이다. 기존4몸체의 실제 형상 덤프 해시는 V007과 동일하다. Z1의185,555인스턴스/데칼12/428키와 채택21소유자·Pod10/훅6/검토점27/LI36을 유지했다. 연결6,288삼각형·858보행 정점/두 재질도 유지했다.

실제5몸체 각각 자기 교차0·열린 변0·비다양체 변0이다. UE 실제 덤프의 부호 있는 부피는 기존 V007과 같은 음수 winding이며 Blender 저작본과 부호 관례가 다르다. 이를 결함으로 보고 메시를 뒤집지 않았다. 몸체 간 의도한 겹침과 개별 몸체 자기 교차를 구별한다.

기존51보호 공간·16,552표본을 그대로 검사해 다른50공간 새 겹침0이다. 고정 통로13,221표본의 부피/표면 침범0·최소 거리 하한252.320969cm다. R01의 기존2m×2m/81광선 격자는 미검출9→0이지만 유한 광선 근거이며 R01 전체 폐쇄/접근·PIE 통과를 뜻하지 않는다. R03/R04 중심은 기존 하부 몸체 안이고 통로·Pod·동굴·석교·부유섬·그래플 사선 표본을 유지했다.

기존 StairRamp3의31표본(하부28+연결3)은 계속 **REVIEW**다. 중앙선 station19~23의 위100/250cm도 몸체 내부여서 가장자리 접촉만으로 축소하지 않는다. 기존 경로/폭을 검사에서 빼지 않았으며 별도 채택 연결과 구별한다.

실제 전후27카메라(기존26+R01GapApproach)·54화면·FOV90°/1127×724·위치/회전 오차0을 대조했다. 모음4장도 검토했다. 식생 바람·스트리밍·Lumen 화면 경고 차이는 배치 변경의 증거로 쓰지 않는다. 2026-11-27 사용자가 약간 어색한 외관을 부유섬/부유석 판타지 지형의 허용 범위로 수용했다. Acceptance_V008이 현재 시각 채택을 소유하며 Generation_V008/FinalValidation의 accepted=false는 제작 당시 이력으로 동결한다.

## 3. 경고·근거·복구

읽기 전용 실제 검사 구간에 Error0, 과거부터 존재한 Z1 북벽 AttachTo1건, PASS 직후 Epic 통계 업로드 libcurl65 경고가 있다. 메시/보존 검사는 PASS다. 준비 중 도구 assertion/입력 문법 오류는 수정했고 반입 중간판과 함께 기록한다. 기존 RHI 가상 예약258GB/예산256GB·Lumen surface cache atlas 초과는 별도 렌더링 REVIEW이며 이번에 조정하지 않았다.

근거 루트는 `Saved/Profiling/Graphics/Phase01/U5a_7_V001/`이다. `BoundarySurvey.json`·`BoundaryReview.html`은 수정 전 조사, `UserGapCamera.json`·`UserViewRay.json`은 사용자 시점, `Geometry_V006/`은 최종 저작, `NativeGeometryCheck_V008.json`은 실제 형상/보호 검사, `ActualPoseExport_V008.json`은 실제27자세다. `Comparison_V008.html`·`ComparisonValidation_V008.json`, `WarningReview_V008.json`, `FinalValidation_V008.json`·`FinalReviewHashes_V008.json`·`FinalReview_V008/`를 함께 본다.

복구는 FinalReviewHashes의 경로/해시로 새 V008 맵과 최종 새 메시를 대조하고 지정 파일만 복원한다. V007 복귀는 Acceptance_V007의 맵을 열면 된다. 이전534파일을 덮어쓰거나 생성 준비를 재실행하지 않는다. 충돌 역할 변경·지각/Support 재빌드·전체 베이크/PIE·다른 구역 확대·세부 식생/잔돌은 미착수다. 구면 Up·native15m/전이55m·시각 seam5m/후속 충돌20m 금지·Floor50㎡/Nav Layer256 계약을 유지한다.

## 4. 사용자 채택과 이후 시각 방향(2026-11-27)

사용자는 앞으로 Ridge를 포함한 LootNPop 맵의 공중에 뜬 부유석 느낌을 허용하는 수준을 넘어 적극 활용하도록 지시했다([GX-037](../Decisions.md)). 의도적인 공중 분리를 받침 부족으로 보고 메우거나 삭제하지 않는다. 열린 내부/뒷면 노출·실질적 통행 막힘은 따로 검사한다. 기존 Cull A~D/채택 배치는 재실행하거나 복원하지 않는다. 이번에는 지형을 다시 만들지 않았다.

V008 맵과 최종 보강 메시가 저장됨·PIE=false를 현재 에디터에서 확인하고 기존534파일/최종168복구본의 해시를 대조했다. FrozenInputs_V008은 가변 문서를 제외한696파일, AcceptedHandoff_V008은25파일이며 중복 제거한 다음 입력은701파일이다. 이전 V007/V006 채택 원본과 모든 제작 이력도 보존한다. 다음 manifest의 이전 채택본은 V008/실제27자세/몸체5+연결1이며 기존21소유자를 중복 추가하지 않는다. 다음 구현 단위는 범위 확인 전이며 사용자 지시로 새 세션에 넘겼다. [최신 인수인계·시작 프롬프트](Phase01_U5aBodyHandoff.md)를 따른다. 마지막 에디터 재확인은 HandoffLiveState_20261127.json의 관련2패키지 dirty=false·PIE=false이며 전체 dirty 재검사는 새 세션 첫 확인에 포함한다.
