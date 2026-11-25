# Phase 1-BR U5a-4 연결로 석재 외형

현재 최신 기준은 사용자 유지 **U5aBody_V006**이며 다음 U5a-6은 새 세션으로 넘겼다. [현재 인수인계](Phase01_U5aBodyHandoff.md)를 우선하고 아래 수치/대기 상태는 당시 시험 이력으로 읽는다.

> 상태: V004 동선 유지 사용자 승인 · V005 석재 외형/26비교·실제 보존/반입858정점 검사 PASS · 연결 외형 유지 사용자 승인
> 확인: 2026-11-25
> 기준: GX-034~036 · [연결 초안](Phase01_U5aConnectionTrial.md)

후속: 사용자가 이 V005 연결 외형 유지를 승인했다. 맵/메시/두 재질/저작본은 동결 보존하고 새 V006의 [U5a-5 몸체 보정](Phase01_U5aBodyCaps.md)으로 이어갔다. 아래 시각 채택 대기 표현은 이 단위 완료 당시 기록이며 몸체·충돌 전체 채택으로 확대하지 않는다.

## 1. 범위와 현재 결과

사용자는 “유지하고 계속 진행하자”라고 승인했다. V004의 연결 동선과 중앙3m 보행면을 고정하고, 새 **U5aBody_V005**에서 연결 메시1개만 석재 외형으로 교체했다. 이 승인은 동선 유지 범위이며 몸체4개·충돌·PIE 전체 채택을 뜻하지 않는다. 기존 V004 맵/연결V011과 U4Layout_V011·Z1 V037/Z1Bake_V010은 보존했다.

- 맵: `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V005/LVI_Ridge_EDTerrain_U5aBody_V005`.
- 액터: 기존 라벨 `CONN_StairRamp3_V011` 유지. 메시만 `U5aBody_V005/ConnectionStone_V001/SM_Ridge_StairRamp3_Stone_V001`로 교체했다. 액터 라벨 V011은 이전 연결 제작 이력이며 현재 석재 형상 판은 V001이다.
- 새 메시6,288삼각형·닫힌 부피/비다양체 변0·자기 교차 후보0. 기존4몸체96,344와 합쳐 시험5액터102,632삼각형이다. Static·월드 identity·NoCollision을 실제 컴포넌트 조회로 확인했다.
- 중앙3m858정점과 상면 삼각형을 보존했다. 저작 단계 최대 차이0.00110cm·실제 상면 경사43.066°다. 실제 UE 반입858정점의 최대 차이0.004367cm로 허용0.05cm 이내 PASS다.
- 바깥 가장자리에 불규칙 폭/모따기·측면 층을 주고 보이는 기존 표면에 닿는 받침4곳을 통합했다. 두께0.90~6.01m, 기존 지형/조립체를 이동하거나 충돌 전용 Floor/Shell을 받침 앵커로 사용하지 않았다.

## 2. 제작과 재질

`RefineRidgeConnectionStone.py`는 동결된 U5a-3 Geometry_V011 이진 형상만 읽었다. 이전 생성기/배치/베이크를 재실행하지 않았다. 원래7열 단면에서 중앙1/2/3열을 새11열 단면에 그대로 복사하고 바깥/아래만 바꿨다. 받침은 NativeCrust·WestUpperShoulder Cliff02·CR LargeAssembly RockFormation06·CR Drape MassiveSandstoneCliff04 표면에 연결한다.

등록된 `StaticMeshTools.import_file`로 새 FBX만 반입했다. 상면 `RidgeWalk_Surface`는 이전 V003 전용 몸체 MI, 측면 `RidgeStone_Sides`는 기존 `MI_Cliff_01`을 참조했다. 공유 MI/부모 재질은 수정하지 않았다. FBX 축/단위 범위 오차0.003663cm·6,288삼각형·두 슬롯 실제 재질 참조가 일치한다. `bUseDefaultCollision=false`, 실제 collisionEnabled/profile=NoCollision, overrideMaterials없음을 확인했다. 새 맵과 새 메시만 지정 저장했다.

형상 생성 후 주석 언어만 한국어로 고쳤으며 실행 AST 불변을 대조했다. Build.json에 실행 당시 소스 해시와 주석 수정 후 저작 소스 해시를 구별했고 BuildSource.py는 현재 저작 소스와 같다. 형상을 재생성해 덮어쓰지 않았다.

## 3. 통로 검사와 남은 REVIEW

원래51보호 공간16,552표본은 유지한다. 새 석재 형상은 StairRamp3에3표본 겹치며 다른50공간에는 새 겹침0이다. 이전 몸체28과 합쳐 **기존 StairRamp3 31표본 REVIEW**다(이전 연결 초안은28+1=29). 기존 길/보호 폭을 지우거나 전체51공간 PASS로 표시하지 않았다.

유지한 대체 통로는 실제 miter 단면 중앙3m·10cm 간격·위100/170/250cm의13,680표본으로 검사했다. 실제 잔존 메시/지각·보존 Floor/Shell·몸체4개·새 연결을 대조해 근접/닫힌 부피 침범0, 최소 거리 하한71.96cm다. 연속 캡슐 이동/실제 보행·점프·그래플·충돌 통과를 뜻하지 않는다. 부유섬/Pod/동굴/석교·그래플 사선은 기존50공간 검사에 포함했다.

최소 seam83.55m로 시각5m/후속 충돌20m 금지를 유지한다. native15m/전이55m·Floor50㎡·Nav Layer256·산괴 배경A안·외곽 장식 종료/내부 보라 구조물·부유섬 후속 범위는 고정했다. 넓은 몸체 캡·직선 경계와 상면/외피 재질 차이는 계속 별도 외관 REVIEW다. 받침/모서리도 사용자 시각 채택 전이다.

## 4. 실제 근거와 마지막 검사

근거 루트: `Saved/Profiling/Graphics/Phase01/U5a_4_V001/`.

- PreservationHashes.json의 이전267파일은 전부 해시 불변이다. 이전200파일+최종 저작16+검토 근거51을 합친 중복 없는 목록이다. V004 manifest/저작 이력을 수정하지 않았다.
- ActualPoseExport.json: 채택21+몸체4+연결1의 실제26자세 export, 이전 V004 대비 위치/방향/크기 오차0. PIE=false·맵 dirty=false다. 실제 Verify_20261125_181219_222557.json은 전체 미저장 패키지없음·issues없음·보존 PASS다. 기존 Z1의185,555인스턴스/데칼12/428키/SHA `853681a3ea1bdb4c002491d0ef4dd484f1723dac18a1915a7d61e8d027527b88`, 채택21개 자세/참조/지문·기존4몸체·새 연결의 identity/NoCollision/삼각형을 대조했다. Pod10·훅6·검토점27·LI/로드36개를 유지한다.
- Comparison.html·ComparisonValidation.json: 이전22+받침4의26시점, 위치/회전0.001 이내·FOV90°·1127×724 전후 일치. 전체/사선 선택2화면을 별도로 남겼다. 같은 카메라 사진은 자세/지문 보존 검사를 대신하지 않는다.
- ImportWarningAudit.json: 새 FBX 탄젠트/바이노멀/스무딩 경고0. 기존 Z1 Shell459,537삼각형 에디터 Nav export1건·북벽 AttachTo1건은 남겨 기록했다. 로그 삭제나 기존 에셋 보정은 하지 않았다.
- ReviewDraftHashes.json·ReviewDraft/: 새 맵/메시·저작/스크립트·실제 자세/재질·형상 근거19파일 복구본이다. 마지막 검사 전 검토 사본으로 구별하며 최종 채택본으로 표시하지 않았다.
- FinalValidation_V001.json·FinalReviewHashes.json·FinalDraft/: 실제 검사 완료 뒤 갱신한 저작 상태/26자세·Verify/StoneVerify를 포함한22파일 최종 검토 복구본이다. 이전 검토19파일 복구 사본은 보존한다. ReviewEvidenceHashes.json은 전후/선택 화면·카메라·로그 근거44파일을 구별해 기록한다. 사용자 시각 채택과 U5a 전체 완료는 아직 아니다.

사용자가 V005에서 실행 완료한 읽기 전용 명령(재실행 불필요):

```text
py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeStoneRefinement.py" verify V005
```

기존 `RidgeClosedBodyTrial.verify`의 전체 지문/21소유자/5액터/미저장 검사와 실제 반입된 중앙858정점·두 재질·범위 대조를 완료했다. StoneVerify_20261125_181219_280794.json: 보존 PASS·858정점 최대0.004367cm·실제3,146정점/6,288삼각형·범위 오차0.003663cm·두 재질 일치·dirty없음이다. 이 명령은 배치·저장을 하지 않았다. 실행 구간 기존 북벽 AttachTo 경고1건을 확인했고 새 Python 오류는 없다. **import V005는 실행하지 않는다.** 기존4몸체와 연결 라벨이 복제된 맵이므로 전체 반입기를 다시 쓰는 판이 아니다.

## 5. 후속 경계

이번 단위의 연결 외형/받침 구현·실제 보존 검사·26비교를 마쳤다. 사용자에게 현재 외형을 유지하며 몸체 캡·직선 경계·재질을 다음 구현 단위로 진행할지 확인한다. 그 뒤 같은 몸체 입력에서 충돌/내부 지각·Support·전체 베이크·수동 PIE를 진행한다. 세부 식생·잔돌과 다른 구역 확대는 아직 실행하지 않았다.
