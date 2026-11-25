# Phase 1-BR V006 유지·U5a-6 새 세션 인수인계

> 상태: U5aBody_V006 사용자 유지·시각 기준 동결 / 실제 보존·반입 검사 PASS / 다음 단위 미착수
> 갱신: 2026-11-26
> 기준: [Current](../Current.md) · [GX-034~036](../Decisions.md) · [U5a-5 결과](Phase01_U5aBodyCaps.md)

## 1. 사용자 결정과 현재 경계

사용자가 **“V006 유지하고 다음 단위는 새 세션으로 넘기자”**라고 지시했다. 현재 V006 외관과26개 액터의 위치·방향·크기·높이 관계·구도를 유지한다. 다음 U5a-6은 이 세션에서 시작하지 않았다. 맵 U5aBody_V007/Generation_V007.json은 아직 없다.

V006 유지는 시각 기준 채택이다. 자기 교차 후보339개·기존 StairRamp3의31표본 겹침과 R01 경계점의 민감성은 남는다. 충돌·내부 지각/Support·전체 베이크·PIE 통과나 U5a 전체 완료로 확대하지 않는다. 원래 R01~R06 제보 수집은 종료됐고 원본 개별 삭제로 먼저 고치지 않는다.

현재 채택 판정 원본은 `Scripts/Profiling/Graphics/ClosedBodyTrial_V001/Acceptance_V006.json`이다. Generation_V006.json·실제 Verify/CapVerify·FinalValidation 안의 accepted=false/visual_review_pending은 검사 당시 기록으로 동결했다. 이를 현재 사용자 유지 결정과 혼동하거나 채택 표시 목적으로 덮어쓰지 않는다. `BodyTrial.json`·같은 폴더 README의 미착수/U5a-1 표현도 과거 입력 고정 이력이다.

## 2. 맵·형상·채택 배치를 구별할 것

| 대상 | 실제 기준 |
|:---|:---|
| 현재 유지 맵 | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V006/LVI_Ridge_EDTerrain_U5aBody_V006` |
| 현재 몸체 저작 | `ClosedBodyTrial_V001/Generation_V006.json`, 새 몸체 형상은 `U5a_5_V001/Geometry_V005/`의 V005다. 맵 판과 형상 판이 다르다 |
| 유지한 연결 | 액터 `CONN_StairRamp3_V011` / 메시 `U5aBody_V005/ConnectionStone_V001/SM_Ridge_StairRamp3_Stone_V001`,6,288삼각형 |
| 현재 큰 배치 원본 | **U4Layout_V011**, 추가21소유자·LI36개. U4-0~11 완료·사용자 채택·동결 |
| 이전 기술 검증 | Z1 V037·**Z1Bake_V010**. U4Layout_V010/V011·현재 U5aBody_V006과 별개이며 보존한다 |
| 다음 예약 판 | U5a-6 / **새 U5aBody_V007**, 아직 생성하지 않았다. 기존 오프라인 U5aBody_V007 형상 폴더와 혼동하지 않는다 |

현재 몸체 액터 라벨은 `BODY_ScanBody_35430_V007`, `BODY_ScanBody_116462_V007`, `BODY_ScanBody_116490_V007`, `BODY_UnderSurfaceBody_V007`이다. 라벨 V007은 과거 제작 이력이며 현재 맵/메시 판이 아니다. 새 메시4개는 V006/BodyCaps_V005 안의 FacetedBody35430/116462/116490·UnderFacetedBody다.

- 몸체4개:145,806삼각형·73,111정점, 유지한 연결을 더한 시험5액터152,094삼각형. 모두 Static·월드 identity·NoCollision이다. 월드 자세는 메시 정점에 들어 있다.
- 실제26자세는 채택21소유자+몸체4+연결1이다. 네 몸체/연결 액터의 원점 자세에 구면 역변환을 적용하지 않는다. body component는 `bUseDefaultCollision=false`와 실제 collisionEnabled/profile NoCollision까지 확인한다.
- 몸체 재질은 V006의 `MI_Ridge_U5a_CapRock_V001`, 전용 `CapTextures_V001/T_Ridge_CapNeutralNormal_V001`·`T_Ridge_CapNeutralMask_V001`은 VT=true다. 부모 MI_Cliff_01·엔진 텍스처는 원본을 보존했다.
- 연결 상면은 V003의 전용 MI_Ridge_U5a_Body_V001, 측면은 공유 MI_Cliff_01이다. 연결의 메시·두 재질·중앙3m 보행면858정점·6,288삼각형·컴포넌트는 고정한다.

## 3. 실제 검증과 채택 후 동결

근거 루트: `Saved/Profiling/Graphics/Phase01/U5a_5_V001/`.

- `Verify_20261125_193752_088779.json`: 보존 PASS·issues/전체 dirty없음. 기존 Z1 185,555인스턴스/데칼12/428키/SHA `853681a3ea1bdb4c002491d0ef4dd484f1723dac18a1915a7d61e8d027527b88`, 채택21개 자세/참조/지문·Pod10/훅6/검토점27/LI36 유지.
- `CapVerify_20261125_193753_710370.json`: 실제73,111정점이 저작 정점 수와 각각 일치, 최대차이0.004367cm(허용0.05cm)·범위 최대0.003357cm·145,806삼각형·재질/유지 연결 일치. 최신 Cmd 구간 새 경고/오류0이다.
- `ActualPoseExport.json`·`ComparisonValidation.json`: 실제26자세 오차0·같은26카메라/FOV90°/1127×724 전후 비교. Comparison.html과 몸체 선택2화면을 보존한다.
- 채택 직전 `AcceptanceLiveState_V001.json`: 실제 열린 V006·PIE=false·26자세 오차0/시험5개 컴포넌트 동일·관련9에셋 dirty없음. 선택된 몸체4개와 사용자 카메라를 읽기만 했으며 바꾸지 않았다. 전체 미저장 기준은 위 최종 Verify이고, 채택 직전 검사는 관련9에셋에 한정했다.
- `FinalReviewHashes.json`·FinalDraft/: 검사 완료 검토36파일, `ReviewEvidenceHashes.json`: 근거44파일. 이전333파일도 불변이다.
- `FrozenInputs_V006.json`: 위333+36+44를 중복 제거한 **412파일**. `AcceptedHandoffHashes_V001.json`·AcceptedHandoff/: 채택 sidecar·현재 맵/메시/저작/실제 근거 **43파일** 복구본. `AcceptanceValidation_V001.json`: 실제26자세/9에셋과 모든 해시 대조 PASS다.

다음 단위 보존 목록은 `FrozenInputs_V006.json.files`와 로컬 `AcceptedHandoffHashes_V001.json`을 **경로 대소문자 무시·해시 일치 확인 후 중복 제거**해417파일로 만든다. 합계를 단순 더하거나 old previous_trial(V005 제작 이력)을 다음 채택본으로 사용하지 않는다. 다음 manifest의 이전 채택본은 **V006 맵/Generation_V006·Acceptance_V006/26자세/몸체4+연결1**이다.

Content/Saved는 Git 제외 로컬 자료다. 문서·도구·저작 JSON·Acceptance/FrozenInputs는 Git으로 보존하지만 다른 컴퓨터의 새 체크아웃에 맵/형상/검사 화면이 자동 복원되지는 않는다. 로컬 자료가 없으면 생성기 재실행으로 대신하지 말고 필요한 원본/복구 사본부터 확인한다. 동결 저작본은 .gitattributes의 실제 LF/CRLF 속성을 유지한다.

## 4. 새 세션 첫 단위: U5a-6

1. 실제 에디터 맵/PIE·사용자 미저장 변경과 로컬417파일·실제26자세·메시/재질/컴포넌트를 먼저 대조한다. 원본/공유 에셋은 저장하거나 변경하지 않는다. 현재 사용자가 다른 맵을 열었으면 기존 미저장 변경을 먼저 확인한다.
2. 유지한4몸체의 이진 형상 또는 UE 실제 형상을 읽어 자기 교차 후보 **12/16/311/0**을 분류한다. 공유 정점/접선 접촉·공면/거의 퇴화 면·실제 관통을 구별하고, BVH 후보 수를 실제 결함 수로 취급하지 않는다. 원인/면 인덱스/시점을 결과로 남긴다.
3. 실제 결함의 최소 보정 범위를 정한 뒤 V006을 새 **U5aBody_V007**으로 복제한다. 채택 외관·원래 ED 외피와 맞닿는 관계·실제26자세·연결 메시/두 재질·보행면을 유지한다. 새 몸체 파생본에만 국소 수정하며 일괄 재복셀화·캡 대규모 변형을 준비 작업으로 하지 않는다. 수동 Blender/GUI 조형이 빠르면 자동화 구현 전에 즉시 사용자에게 요청한다.
4. 기존51보호 공간16,552표본·31표본 legacy REVIEW와 유지 연결의 실제 중앙3m/10cm 이하·위100/170/250cm 통로 검사를 함께 유지한다. 현재 새 몸체와 연결 통로13,221표본 침범0·최소 거리 하한252.32cm, 다른50공간 새 겹침0이다. 입력 폭을 줄이거나 기존 경로를 검사에서 빼서 PASS로 만들지 않는다.
5. 새4몸체의 닫힌 위상/남은 실제 교차·원래 R03/R04 중심·R01 경계 민감성·기존 통로/Pod/동굴·석교·부유섬/그래플 사선을 검사한다. native15m/전이55m, 시각 seam5m/후속 충돌20m·Floor50㎡/전체 Nav Layer256을 유지한다.
6. 새 패키지만 지정 저장·실제 자세 export·이전417파일 불변·같은26카메라 비교·새 경고와 기존 경고 구별·복구본을 마친 뒤 결과를 보고하고 다음 단위를 확인한다. 전체 지문/실제 정점 검사는 필요할 때 UE Cmd로 요청한다. 현재 V006의 import/생성/검사를 준비 목적으로 반복하지 않는다.

충돌 역할 변경·지각/Support 재빌드·전체 베이크/PIE·다른 구역 확대·세부 식생/잔돌은 이 첫 단위에 넣지 않는다. 외곽 보라색 전이 띠 장식은 종료했고 삼각형 안쪽 보라 구조물·두 부유섬은 후속 장식 대상이다. 기존 모든 U4/U5a/Z1 판과 원본/공유 에셋을 보존한다.

## 5. 새 세션 시작 프롬프트

```text
@D:/UnrealProjects/LootNPop/.context/Graphics/README.md
@D:/UnrealProjects/LootNPop/.context/Graphics/Current.md
@D:/UnrealProjects/LootNPop/.context/Graphics/analysis/Phase01_U5aBodyHandoff.md

문서를 확인하고 Phase 1-BR U5a-6 몸체 자기 교차 후보 검토·정리를 진행하자.
Decisions GX-034~036과 Phase01_U5aBodyCaps.md도 확인해줘.
U4Layout_V011은 추가21소유자 채택·동결 상태이고 U5aBody_V006도 현재 외관 유지로 채택했어.
V006은 실제 보존/73,111정점 PASS·26자세 export·26시점 비교 완료야.
U5a-6/U5aBody_V007/Generation_V007은 아직 시작하지 않았어.

먼저 실제 에디터·PIE·미저장 변경·로컬417파일/26자세/메시·재질을 대조해줘.
Acceptance_V006과 FrozenInputs_V006이 현재 채택 기준이며,
Generation_V006의 accepted=false/previous_trial V005는 제작 당시 이력이야.
다음 manifest의 이전 채택본은 V006 맵/저작본·26자세·몸체4+연결1로 갱신해줘.
417목록은 FrozenInputs412와 AcceptedHandoff43을 중복 제거해서 만들고 중복 추가하지 마.

먼저12/16/311/0의 자기 교차 후보를 실제 관통/접촉·공면/퇴화 면으로 분류해줘.
실제 결함의 최소 보정만 새 V007 사본에서 하고 V006 원본은 보존해줘.
채택 외관·21소유자/26자세·연결 메시/두 재질·중앙3m 보행면은 유지해줘.
기존51보호 공간·다른50공간 겹침0·새 통로13,221표본을 대조하고 기존길31표본은 REVIEW로 유지해줘.
R01 경계점·R03/R04 중심·통로/Pod/동굴/석교/부유섬/그래플 사선도 확인해줘.

기존 생성물 준비 재실행·U4 재시작·원본/공유 에셋 수정·R01~R06 개별 삭제는 하지 마.
비주얼 우선과 seam/구면/native15m·전이55m·시각5m·후속충돌20m 계약을 유지해줘.
충돌/지각·Support/전체 베이크·PIE·세부 식생/잔돌은 후속으로 둬.
실제 자세 export·기존 보존 검사·동일 카메라 비교까지 보고하고 구현 단위마다 다음 진행을 확인해줘.
수동 조작이 더 효율적이면 자동화 구현 전에 즉시 요청해줘.
```
