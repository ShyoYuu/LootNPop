# Phase 1-BR V008 채택·다음 단위 새 세션 인수인계

> 상태: U5a-6/V007·U5a-7/V008 구현·실제 검사·사용자 외관 채택 완료 / 다음 구현 단위 미착수
> 갱신: 2026-11-27
> 진입: [Current](../Current.md) · [GX-034~037](../Decisions.md) · [U5a-6 결과](Phase01_U5aSelfIntersectionReview.md) · [U5a-7 결과](Phase01_U5aBoundaryReview.md)

## 1. 사용자 결정과 채택 원본

사용자는 V007 변경을 에디터에서 확인하고 유지를 승인했고, V008 국소 보강도 약간 어색하지만 판타지 지형의 허용 범위로 수용했다. **현재 채택은 V008이다.** 다음 작업은 새 세션으로 넘긴다. 다음 구현 단위의 범위·판은 아직 정하지 않았고 새 V009 맵/Generation_V009은 만들지 않았다.

GX-037에 따라 Ridge를 포함한 LootNPop 전체 맵에서 부유섬·부유석·공중 암석과 그 사이 빈 공간을 적극적인 판타지 표현으로 활용한다. 의도된 부유 덩어리에 지면 받침을 강제하거나 공중 분리를 메우지 않는다. 기존 Cull A~D와 채택 배치는 재현 이력으로 보존하고 재실행/복원하지 않는다. 열린 내부·일방 뒷면 노출·실질적인 통행 막힘과 부유 연출은 구별한다.

채택 판정은 `Scripts/Profiling/Graphics/ClosedBodyTrial_V001/Acceptance_V008.json`·`FrozenInputs_V008.json`이 소유한다. Generation_V008/FinalValidation의 accepted=false·visual_review_pending과 previous_accepted V007은 **제작 당시 이력**이다. 채택 표시나 다음 manifest 준비 목적으로 덮어쓰지 않는다. V007/V006의 Acceptance·제작 기록도 보존한다. 다음 manifest의 이전 채택본은 **V008 맵/Generation·Acceptance/실제27자세/몸체5+연결1**로 새로 구성하고 기존21소유자를 중복 추가하지 않는다.

## 2. 맵·형상·배치 기준

| 구분 | 현재 기준 |
|:---|:---|
| 채택 시험 맵 | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V008/LVI_Ridge_EDTerrain_U5aBody_V008` |
| 몸체 저작 | `ClosedBodyTrial_V001/Generation_V008.json`, 기존4몸체와 새 R01 보강1개 |
| 새 보강 | `U5aBody_V008/R01Closure_V006/SM_Ridge_U5a_R01RockBackfill_V006`, 액터 `BODY_R01RockBackfill_V006`(StaticMeshActor_13) |
| 유지한 연결 | `CONN_StairRamp3_V011` / `U5aBody_V005/ConnectionStone_V001/SM_Ridge_StairRamp3_Stone_V001` |
| 큰 배치 원본 | U4Layout_V011·추가21소유자·LI36개, U4-0~11 사용자 채택·동결 |
| 이전 기술 검증 | Z1 V037/Z1Bake_V010. U4Layout 판·현재 U5aBody 판과 구별하고 보존 |
| 실제 자세/역할 | 기존26자세 그대로+새 identity1개=27자세, 몸체5+연결1 모두 Static/NoCollision |

맵 V008과 보강 형상 V006은 서로 다른 버전 축이다. 몸체의 기존 V007 라벨이나 Saved의 과거 오프라인 U5aBody_V007 형상을 새 맵 판과 혼동하지 않는다. 기존 세 수리 몸체는 V007/IntersectionRepair_V001, 하부 몸체는 V006/BodyCaps_V005, 연결은 V005를 참조한다. ED 외피·연결 메시/두 재질·중앙3m 보행면858정점은 고정했다.

마지막 에디터 확인은 `Saved/Profiling/Graphics/Phase01/U5a_7_V001/HandoffLiveState_20261127.json`이다. 활성 V008·PIE=false·새 보강 선택 상태이고 V008 맵/최종 새 메시 dirty=false다. 사용자가 육안 확인하며 옮긴 카메라를 그대로 두었다. **이번 확인은 관련2패키지에 한정한다.** 전체 dirty=[]는 11월26일 사용자 실행 Verify/NativeVerify 당시 근거이며 현재 전체 미저장 상태로 확대하지 않는다. 새 세션에서 사용자 미저장 변경을 다시 확인하고 임의 저장/폐기하지 않는다.

## 3. 완료한 검사와 남은 REVIEW

- **U5a-6/V007:** 최초 BVH 후보339쌍(12/16/311/0)을 실제 관통으로 분류하고 공유 정점 바깥에서 관통하는126쌍을 추가 검출했다. 합465는 면 쌍 수이며 독립 결함 위치 수가 아니다. 세 몸체76/68/2,010정점만 최대7.258cm 이동했고 UV/면 연결/축별 외곽 경계·하부/연결을 유지했다. 실제73,111정점/145,806삼각형·개별 자기 교차0·26자세/같은26카메라·이전417파일 보존 뒤 사용자 채택했다.
- **U5a-7/V008:** 사용자 확인 시선이 가리키는 정확한 기존 Cliff09 안쪽에162정점/320삼각형·3.617643m³ 보강1개를 추가했다. 사용자 카메라는 암석 앞 공간이므로 그 위치를 채우지 않았다. 원래 ED hull 밖 정점0·목표 내부/카메라 외부이며 돌출 중간안은 제외/보존했다. 실제73,273정점/몸체146,126삼각형·기존4몸체 덤프 해시 동일·5몸체 각각 교차0/열린 변0/비다양체 변0이다. UE 덤프 부피 부호는 기존 V007과 같은 winding으로 대조했다.
- **전체 보존/자세:** 실제 Z1의185,555인스턴스/데칼12/428키·SHA `853681a3ea1bdb4c002491d0ef4dd484f1723dac18a1915a7d61e8d027527b88`, 채택21소유자·Pod10/훅6/검토점27/LI36 보존 PASS다. 기존26자세 오차0·새 identity1개다. 연결6,288삼각형/두 재질·858보행 정점 유지·반입 최대차이0.004367cm다.
- **보호 공간/통로:** 기존51공간16,552표본·다른50공간 새 겹침0이다. 고정 중앙3m 통로13,221표본 부피/표면 침범0·최소 거리 하한252.320969cm다. R03/R04 중심은 기존 하부 몸체 안이며 통로·Pod·동굴·석교·부유섬·그래플 사선 표본을 유지했다.
- **기존 StairRamp3:** 하부28+연결3=31표본을 REVIEW로 유지한다. 중앙 station19~23의 위100/250cm도 몸체 안이므로 가장자리 접촉만으로 축소하지 않는다. 기존 경로/보호 폭을 빼서 PASS로 만들지 않는다. 채택한 별도 연결과 구별한다.
- **R01:** 원래2m×2m/81광선의 미검출9→0은 유한한 방향 표본 근거다. R01 전체 폐쇄·접근/보행·PIE 통과로 확대하지 않는다. GX-037에 따라 공중 분리 자체를 결함으로 다루지 않는다.
- **화면/경고:** V008 전후27카메라(기존26+R01GapApproach)/54화면·FOV90°/1127×724·위치/회전 오차0을 대조했다. 검사 구간 Error0, 기존 북벽 AttachTo와 PASS 뒤 Epic 통계 업로드 libcurl65는 별도 기록이다. 준비/정리 도구 입력 오류도 수정 내역을 보존했다. RHI 가상 예약258GB/예산256GB·Lumen atlas 초과는 렌더링 REVIEW이며 조정하지 않았다.

시각 채택·개별 몸체 자기 교차 해결과 충돌/Support·PIE 통과는 별개다. 충돌 역할 변경·지각/Support 재빌드·전체 베이크/PIE·다른 구역 확대·세부 식생/잔돌은 U5a-6/7에서 시작하지 않았다.

## 4. 보존 목록·로컬 근거·복구

현재 저작 폴더는 `Scripts/Profiling/Graphics/ClosedBodyTrial_V001/`이다. `Handoff_V008.md`는 채택 당시 동결 sidecar이며 최신 세션 경계는 이 문서를 따른다.

다음 단위는 `FrozenInputs_V008.json.files`의696파일과 `Saved/Profiling/Graphics/Phase01/U5a_7_V001/AcceptedHandoffHashes_V008.json`의25파일을 **경로 대소문자 무시·중복 해시 일치 확인 후701파일**로 합친다. backup 경로도 대조한다. 문서와 `.gitattributes`는 후속 갱신을 위해 동결 목록에서 제외했고, 제작 당시 문서의 복구 사본 해시는 유지한다. 합계를 단순 더하거나 구 V005/V006 제작 이력을 이전 채택본으로 복사하지 않는다.

| 로컬 근거 루트 | 핵심 자료 |
|:---|:---|
| `Saved/Profiling/Graphics/Phase01/U5a_6_V001/` | 후보 분류·국소 보정·실제 Verify/NativeVerify·NativeGeometryCheck_V007·26비교·최종118파일/채택29파일 복구 |
| `Saved/Profiling/Graphics/Phase01/U5a_7_V001/` | BoundarySurvey·UserGapCamera/UserViewRay·Geometry_V006·실제 Verify/NativeVerify·NativeGeometryCheck_V008·ActualPoseExport_V008·Comparison_V008.html/27비교·WarningReview_V008 |
| 같은 U5a-7 루트 복구 | FinalReviewHashes_V008/FinalReview_V008의168파일, AcceptedHandoffHashes_V008/AcceptedHandoff_V008의25파일, AdoptionDecisionHashes_V008의결정 문서9파일 |
| 같은 U5a-7 루트 초안 | Geometry_V001~006 및 NativeDraft_V003/V004·각 해시 목록. 제외된 초안도 보존하며 개별 삭제하지 않음 |

복구는 manifest의 정확한 경로/해시로 새 판의 지정 파일만 복원한다. V008 현재 채택과 V007/V006 되돌리기는 해당 Acceptance의 맵과 복구본을 구별한다. 기존 Z1/U4/이전 U5a·원본/공유 에셋은 덮어쓰지 않는다.

Content 작업 맵/형상과 Saved 검사·화면·복구본은 기존 Git ignore 정책대로 **로컬 보존**한다. 커밋은 문서·도구·저작 JSON·Acceptance/FrozenInputs를 보존한다. 새 컴퓨터의 체크아웃에 로컬 근거가 자동 복원되지는 않는다. 자료가 없으면 기존 생성 준비를 재실행하지 말고 원본/복구 자료부터 확보한다. `.gitattributes`의 실제 LF/CRLF/혼합 원시 바이트를 유지한다.

## 5. 새 세션 첫 작업과 경계

1. README·Current·GX-034~037·이 문서/U5a-7 결과를 읽고 실제 에디터 맵/PIE·사용자 전체 미저장 상태를 먼저 확인한다. TestMap03이면 미저장 변경을 보존한 채 V008 맵을 열고 보강을 선택할 수 있다. 전체 dirty 확인용 수동 UE Cmd가 빠르면 즉시 요청한다.
2. 로컬701파일/채택 복구본·실제27자세·몸체5+연결1 메시/재질/컴포넌트를 대조한다. 완료한 생성/반입 준비를 반복하지 않는다. 과거 전체 검사와 현재 에디터 상태를 구별한다.
3. 남은 R01 전체 접근·기존 StairRamp3의31표본 REVIEW에서 **다음 한 단위의 범위를 먼저 제안하고 사용자와 확정**한다. 권장 첫 범위는 기존 근거의 읽기 전용 정리와 필요한 수동 시각/동선 확인 범위 지정이다. 의도된 부유 표현은 적극 유지한다. 새 형상/맵 판·충돌 단계는 이 인계만으로 착수 승인된 것이 아니다.
4. 수정이 합의되면 V008 원본·현재 외관/구도·21소유자/27자세·ED 외피·연결/두 재질/858정점을 유지하고 새 사본에서 최소 보정한다. 실질적 막힘은 새 보정본이나 별도 연결로 다루되 기존 경로·보호 폭은 검사의 분모에서 유지한다. 수동 조형이 더 효율적이면 자동화 구현 전에 요청한다.
5. 기존51공간/16,552표본·다른50공간 새 겹침0·고정13,221표본/원래31표본 REVIEW와 R03/R04·통로/Pod/동굴/석교/부유섬/그래플 사선을 대조한다. 실제 자세 export·이전 파일 보존·같은 카메라 비교·경고 구별·복구를 마친 뒤 단위마다 결과를 보고하고 다음 진행을 확인한다.

구면 Up·지각 native15m/전이55m·시각 seam5m/후속 충돌20m 금지·Floor50㎡/전체 Nav Layer256 계약을 유지한다. U4 재시작·R01~R06 제보 재수집/개별 원본 삭제·일괄 재복셀화·캡 대규모 변형·기존 생성 준비 재실행은 하지 않는다. 충돌 역할 변경·지각/Support 재빌드·전체 베이크/PIE·다른 구역 확대·세부 식생/잔돌은 별도 후속 단위다. 외곽 보라 전이 띠 장식은 종료했고 안쪽 보라 구조물·두 부유섬은 후속 마감 대상이다.

## 6. 새 세션 시작 프롬프트

```text
@D:/UnrealProjects/LootNPop/.context/Graphics/README.md
@D:/UnrealProjects/LootNPop/.context/Graphics/Current.md
@D:/UnrealProjects/LootNPop/.context/Graphics/analysis/Phase01_U5aBodyHandoff.md
@D:/UnrealProjects/LootNPop/.context/Graphics/analysis/Phase01_U5aBoundaryReview.md

문서를 확인하고 Phase 1-BR의 다음 한 단위를 정하자.
Decisions GX-034~037과 Phase01_RidgeReferencePatch.md §2.1·U5a도 확인해줘.
U4Layout_V011은 동결이고 U5a-6/V007·U5a-7/V008은 실제 검사·사용자 외관 채택 완료야.
현재 채택은 Acceptance_V008/FrozenInputs_V008, 실제27자세·몸체5+연결1이야.
Generation_V008/FinalValidation의 accepted=false·이전 V007은 제작 이력으로 보존해.
새 V009 맵/Generation과 다음 구현 단위는 아직 시작하지 않았어.

먼저 실제 에디터/PIE·전체 미저장 변경과 로컬701파일·실제27자세·메시/재질/컴포넌트를 대조해줘.
701은 Frozen696과 로컬 AcceptedHandoff25를 경로/해시 대조해 중복 제거한 목록이야.
새 manifest 이전 채택본은 V008 맵/저작본·27자세·몸체5+연결1로 갱신하고21소유자를 중복 추가하지 마.
Content/Saved가 없으면 생성기로 대신하지 말고 원본/복구 자료부터 확인해줘.

GX-037에 따라 Ridge를 포함한 LootNPop 전체 맵의 부유석/공중 암석 표현을 적극 활용해줘.
의도된 부유 덩어리를 지면 받침이 없다는 이유로 메우거나 제거하지 마.
열린 내부/뒷면 노출·실제 통행 막힘은 별도로 구별해줘.
다음 첫 범위는 남은 R01 전체 접근·기존 StairRamp3 31표본 REVIEW 근거를 읽고
다음 한 단위의 범위와 필요한 수동 확인을 먼저 제안해줘. 새 형상/충돌 작업은 범위 확인 뒤 진행하자.

현재 외관/바깥 윤곽·높이 관계·구도·21소유자/27자세·ED 외피·연결 메시/두 재질/858정점은 유지해줘.
51보호 공간16,552표본·다른50공간 새 겹침0·고정 통로13,221표본과 기존길31표본 REVIEW를 유지해줘.
기존 경로/보호 폭을 빼서 PASS로 만들지 말고 R03/R04·통로/Pod/동굴/석교/부유섬/그래플 사선도 확인해줘.
U4/Z1/이전U5a·원본/공유 에셋 보존, 기존 생성물 준비 재실행·U4 재시작·R01~R06 재수집/개별 삭제는 하지 마.
구면 Up·native15m/전이55m·시각5m/후속충돌20m 금지·Floor50㎡/전체Nav Layer256 계약을 유지해줘.
충돌 역할 변경·지각/Support 재빌드·전체 베이크/PIE·다른 구역 확대·세부 식생/잔돌은 이번 첫 범위에서 시작하지 마.
구현 단위마다 실제 자세 export·이전 파일 보존·같은 카메라 비교·복구를 보고하고 다음 진행을 확인해줘.
수동 조작이 더 효율적이면 자동화 구현 전에 즉시 요청해줘.
```
