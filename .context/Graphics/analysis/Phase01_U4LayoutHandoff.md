# Phase 1-BR U4 확정·현재 U5a 인계 안내

> 상태: U4Layout_V011 동결·외곽 추가 장식 종료 / U5aBody_V006 사용자 유지 / 다음 U5a-6 새 세션·미착수
> 갱신: 2026-11-26
> 현재 시작점: [V006 유지·새 세션 인수인계](Phase01_U5aBodyHandoff.md)

## 1. 현재 세션 인계

현재 맵은 **U5aBody_V006**이며 사용자가 현재 외관 유지를 확정했다. 다음 U5a-6은 새 세션에서 자기 교차 후보339개를 분류하고 새 V007에서 실제 결함만 최소 보정한다. 이 세션에서 V007 맵/Generation_V007을 만들거나 충돌·지각/Support·베이크·PIE를 시작하지 않았다.

현재 채택 원본은 `ClosedBodyTrial_V001/Acceptance_V006.json`이다. 실제26자세·몸체4/연결1·메시/재질/VT 텍스처와 기존 Z1/21소유자를 유지한다. Generation_V006.json의 accepted=false/previous_trial V005는 검사·제작 당시 동결 기록이며 현재 채택 판정은 sidecar를 따른다. 보존412파일과 채택43파일을 중복 제거한417파일이 다음 보존 목록이다.

최신 Verify_20261125_193752_088779.json·CapVerify_20261125_193753_710370.json은 실제 보존/73,111정점 PASS·최대차이0.004367cm·전체 dirty없음이다. 채택 직전 읽기 전용 대조도 V006·PIE=false·26자세/시험5컴포넌트 일치·관련9에셋 dirty없음이다. 같은26시점·최종36파일/검토44파일·채택43파일 복구본은 `Saved/Profiling/Graphics/Phase01/U5a_5_V001/`에 있다.

자기 교차339후보·기존 StairRamp3 31표본·R01 경계점 민감성은 계속 REVIEW다. 시각 기준 유지와 충돌/PIE 통과를 구별한다. 정확한 자산명·보존 목록 생성법·검사/금지 사항·새 세션 프롬프트는 [현재 인수인계](Phase01_U5aBodyHandoff.md)가 소유한다.

## 2. U4에서 확정한 기준

- U4-0~11은 사용자 채택·완료이며 큰 배치 기준은 **U4Layout_V011**이다. 원래 **Z1 V037/Z1Bake_V010**·U4Layout_V010·현재 U5aBody_V006은 서로 다른 맵이다.
- V011 이전 채택본은 V010 맵/저작본·19소유자·누적 이전 저작53파일이다. V010 previous_unit의 V009 제작 이력을 그대로 복사해 Z7 소유자를 중복 추가하지 않았다.
- V011에서 남동/북동 외곽2개를 더해 추가21소유자·LI/로드36개가 됐다. 실제 Export_20261125_120226_919321.json은 PASS·issues/dirty없음·exported=true다. 기존 Z1 185,555인스턴스/데칼12/428키와 이전19개 자세/참조/지문은 불변이다.
- 신규8,723+830인스턴스·NoCollision·각51보호 공간16,552표본 겹침0,30시점 비교와 실제 자세 export를 완료했다. V011 채택 기준과 원시11파일은 U4_InteriorReview_V001의 AcceptanceValidation/AcceptedVisualHashes가 소유한다.
- 사용자 GX-036: 외곽 보라색 전이 띠 추가 장식은 현재 정도에서 종료하고 옥탄트 안쪽을 우선한다. **삼각형 안쪽 보라 구조물과 두 부유섬은 장식 대상**이다.

[U4 계획 §18~20](Phase01_U4LargeLayoutPlan.md)·[U5a 첫 시험](Phase01_U5aBodyTrial.md)·[경계 보정](Phase01_U5aBodyRefinement.md)·[별도 연결](Phase01_U5aConnectionTrial.md)·[석재 연결](Phase01_U5aStoneRefinement.md)·[V006 몸체 보정](Phase01_U5aBodyCaps.md)을 이력으로 보존한다. 과거 문서의 미착수/미채택 표현은 해당 검사 시점의 기록이다.

## 3. 계속 유지할 계약

GX-034~036의 채택 배치·구도/높이 관계를 보존한다. 통로·Pod·동굴 입구/방·석교 위/아래·부유섬 착지/아래 공간·그래플 사선을 유지한다. 의도된 빈 공간을 통째로 메우지 않는다.

구면 현지 Up·native15m/전이55m·시각 seam5m/후속 충돌20m·Floor50㎡/전체 Nav Layer256·산괴 배경A안·빠진 Pod2개 제외·정식 풀/EnemyDensity1을 유지한다. 이전 남서 시각 상자17.12m는 후속 충돌에 그대로 쓰지 않는다.

기존 생성물 준비 재실행·U4 재시작·R01~R06 개별 원본 삭제·공유 에셋 수정은 하지 않는다. FBX 반입은 등록된 StaticMeshTools.import_file을 쓰며 Programmatic은 등록 도구 조합용이다. 임의 Unreal Python 검사는 필요할 때만 사용자 UE Cmd로 요청한다. 현재 V006은 반입/검사 완료이므로 import를 반복하지 않는다. 수동 조작이 빠르면 자동화 구현 전에 요청한다.

Content/Saved는 Git 제외 로컬 자료다. 이번 인계와 함께 문서·저작 JSON·도구를 커밋하며 작업 맵/원본/복구본/비교 화면은 로컬에 보존한다. 다음 세션은 [현재 인수인계](Phase01_U5aBodyHandoff.md)에서 시작한다.
