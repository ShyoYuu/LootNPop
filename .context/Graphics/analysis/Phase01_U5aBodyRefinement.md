# Phase 1-BR U5a-2 몸체 경계 보정과 별도 연결 조사

현재 최신 기준은 사용자 유지 **U5aBody_V006**이며 다음 U5a-6은 새 세션으로 넘겼다. [현재 인수인계](Phase01_U5aBodyHandoff.md)를 우선하고 아래 수치/대기 상태는 당시 시험 이력으로 읽는다.

최신 후속은 [U5a-3 연결 초안 결과](Phase01_U5aConnectionTrial.md)다. V004의 실제 보존/22비교를 마쳤고 사용자 시각 채택 대기다. 아래 내용은 이전 몸체 시험 기록으로 보존한다.

> 상태: V003/생성V007 실제 보존 PASS·동일16카메라 비교 완료 / 외관·경사로 REVIEW·미채택 / 수동 착지점 확인 대기
> 확인: 2026-11-25
> 이전 결과: [U5a-1 몸체 시험](Phase01_U5aBodyTrial.md)

사용자가 몸체 경계/외관 다듬기와 StairRamp3 별도 연결 검토를 승인했다. 시작 에디터는 U5aBody_V002·PIE=false·맵 dirty=false였고, 이전 보존156파일과 최종 시험 기록18파일의 총174개 해시가 같았다. 채택 U4Layout_V011과 U5aBody_V001/V002는 변경하지 않았다.

## 1. 새 형상과 반입 준비

새 맵은 `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V003/LVI_Ridge_EDTerrain_U5aBody_V003`다. 채택 V011을 새로 복제하고 지정 저장했다. 맵 판V003과 오프라인 몸체 판V007을 구별한다. **사용자 Cmd import V003으로 새4개 반입·지정 저장·실제 보존 검사까지 완료했다.** 기존 V002와 새 V003을 별도 보존한다.

`RefineRidgeBodyTrial.py`는 저장된 V006 이진 형상만 읽었다. 원래 배치/몸체 생성기·Cull·베이크를 재실행하지 않았다. 세 ScanBody는 같은 파생 형상을 유지하고, 하부 공동 몸체의 바깥1.25m만 구면 높이 방향으로 낮췄다. R03/R04 중심을 잇는 반경2.25m 안쪽은 유지하고 반경3.5m 바깥쪽 끝은 지면 위15cm로 이어지게 했다. 기존 닫힌 격자 연결은 유지했다.

- 새 메시4개 합계96,344삼각형. 경계 변·비다양체 변 모두0이다.
- 하부 몸체 부피는564.25→468.33m³, 보정 정점814개다. R03/R04 기록점은 계속 같은 하부 몸체 부피 안에 있다. 기록점 검사는 구멍 전체의 시각/물리 차단을 보증하지 않는다.
- 기존51보호 공간·16,552표본을 그대로 검사했다. StairRamp3만32→28표본 겹침, 나머지50공간 겹침0이다. **경사로는 미해결 REVIEW이며 보호 폭을 바꾸지 않았다.**
- 메시 최소 seam 거리는 약95.63m다. native15m/전이55m·시각 seam5m·후속 충돌20m 금지를 유지한다.
- 새 전용 `MI_Ridge_U5a_Body_V001`은 기존 MassifSurface_V003 MI를 새 폴더에 복제했다. 기존 부모/공유 MI는 변경하지 않았다. UV atlas의 cap 평면감 대신 기존 지형의 좌표 투영 재질을 새 몸체에서 비교할 후보이며 아직 외관을 채택하지 않았다.

저작본은 `ClosedBodyTrial_V001/Generation_V003.json`, 형상 근거는 `Saved/Profiling/Graphics/Phase01/U5aBody_V007/Build.json`·`GeometryCheck.json`이다. `RidgeClosedBodyTrial.py import V003`은 새4개 NoCollision 반입·지정 저장·실제 보존 검사를 수행한다. 기존 검사기/Generation_V001/V002·입력 BodyTrial.json은 그대로 보존했다.

## 2. StairRamp3 조사와 수동 착지점 확인

단순 서쪽 우회는 지형 속으로 들어가거나 채택 `ASM_Z3_WestUpperShoulder`와 만났다. 기존 StairRamp2 재사용도 북쪽 고지대를 낮은 선으로 가로질러 바로 채택할 수 없었다. 기존 바위/몸체를 삭제하거나 채택 소유자를 움직이지 않았다.

읽기 전용 조사는 보존한 fallback 삼각형과 채택 추가 소유자의 원본 조립체를 채택 자세로 옮긴 형상을 사용했다. 해당 구역의 구조 메시714개를 대상으로 폭3m, 종방향50cm, 지면 위100/170/250cm 표본을 조사했다. 메시 로컬 BVH의 보수적인 거리 하한이50cm 미만인 후보는 실제 월드 삼각형 거리로 다시 확인했다. 이 검사는 열린 원본 바위의 내부·연속 캡슐 이동·물리 통행을 검증하지 않는다.

최종 비교3안은 `U5a_2_V001/ConnectionSurvey.json`에 있다. 북쪽 착지점 `(-7400,2900)`/`(-7500,3000)`은 기존 표면과106/134표본 근접이다. `NorthStepLanding_7600`은2,079표본 근접0·최소 거리 하한 약63.36cm·구면 기준 최대 경사37.05°다. 다만 길이가 긴 고가 우회이고 실제 발판 접속·자체 교차/전체 기존 보호 공간·구도는 미검증이다. **이를 확정 연결이나 보호 검사 PASS로 표시하지 않는다.** 새 길 메시도 배치하지 않았다.

북쪽 착지점은 수동으로 에디터에서 정하는 편이 더 빠르고 정확하다고 판단했다. 추가 경로 자동 생성은 중단했다. 우선 몸체 보정본을 반입한 다음 같은 카메라로 비교하고, 사용자가 원하는 북쪽 착지부를 뷰포트에서 확인한 뒤 별도 연결을 구체화한다. 현재 NoCollision 시험본으로 통행 PIE를 대신하지 않는다.

## 3. 실제 검사·비교 결과와 남은 수동 단계

근거 루트는 `Saved/Profiling/Graphics/Phase01/U5a_2_V001/`다. Preparation.json/PreparationValidation.json은 반입 전 기록으로 남기고, 실제 결과는 FinalValidation_V001.json·ComparisonValidation.json·ActualPoseExport.json·FinalReviewHashes.json으로 구별한다.

- 실제 `Verify_20261125_131822_798705.json`: preservation_pass=true·issues/dirty없음. 기존 Z1 지문185,555개/데칼12/428키와 SHA `853681a3ea1bdb4c002491d0ef4dd484f1723dac18a1915a7d61e8d027527b88`가 같고, 기존8메시/Cull15개·채택21개 자세/참조/재질/지문을 유지한다. Pod10·훅6·검토점27·LI/로드36개다.
- 새4개96,344삼각형·world identity·NoCollision·전용 재질이다. 실제 FBX 범위 오차는 최대 약0.0033cm다. Import_20261125_131814_440692.json에 메시/재질/액터 경로를 기록했다.
- 반입 명령부터 실제 Verify 종료까지 새 메시 경고0·기존 북벽 AttachTo1건이다. 탄젠트/바이노멀·ComparisonCameras 파일 누락 오류는 이번 구간에 없다. 로그를 지우지 않았다.
- 기존174파일 해시 불변과 새20파일 복구본/해시를 확인했다. 실제25자세(채택21+몸체4)는 MCP ActorTools.get_actor_transform을 읽어 ActualPoseExport.json으로 저장했다. 원점 자세인 몸체에는 구면 역변환을 적용하지 않고 월드 정점 자세와 구별했다.
- V002 대비16시점 모두 카메라 위치/회전0.001 이내·FOV90°·1127×724·Unlit 비교 PASS다. Comparison.html에 같은 화면을 나란히 배치했다. 이미지 픽셀 동일성을 배치 보존 검사로 삼지 않았다.
- 육안에서는 넓은 캡 면의 평면감과 기존 외피/새 투영 재질의 색차가 남는다. R03 입구/아래면의 직선 경계도 보인다. **보존 PASS를 외관 채택으로 바꾸지 않았다.** 외관과 StairRamp3는 계속 REVIEW다.

ManualReview.html에는 같은16비교와 LandingPlan.svg·착지부 전체/근접2시점을 함께 넣었다. 카메라 중심은 조사 후보 `(-7600,3000,H1006)`이다. 에디터는 V003·PIE=false·맵 dirty=false·선택없음·LandingClose에 두었고 LiveHandoff.json/ManualReviewHashes.json을 남겼다. 원래 UnderVoidOverview 카메라는 PostImportLiveState.json에 보존했다.

사용자에게 현재 화면 중앙을 착지 기준으로 쓸지, 원하는 착지면이 화면 중앙에 보이도록 뷰포트를 옮겨 직접 지정할지 요청했다. 수동 응답을 기다리는 동안 연결 메시를 배치하거나 다른 자동 경로를 만들지 않는다. 연결/외관이 정리된 뒤 충돌·지각/Support·베이크·수동 PIE를 진행한다. 외곽 장식 종료와 내부 보라 구조물/부유섬 후속 범위는 유지한다.
