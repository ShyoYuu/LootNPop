# U5a V007 채택·후속 경계 검토 기준

사용자가 에디터에서 변경한 세 몸체를 확인하고 V007 유지 및 다음 단위 진행을 승인했다(2026-11-26).

- 현재 채택 원본은 Acceptance_V007.json과 FrozenInputs_V007.json이다. V006 및 그 이전 채택/제작 기록은 그대로 보존한다.
- Generation_V007/FinalValidation의 accepted=false와 visual_review_pending은 검사 당시 이력이다. 채택 표시 목적으로 덮어쓰지 않는다.
- V007 실제 전체 보존/몸체73,111정점·교차0/닫힌 위상·연결858정점/두 재질 PASS,26자세 오차0·같은26카메라 비교 완료다.
- 몸체4개+연결1개는 NoCollision이며 시각 채택과 충돌/접근/PIE 통과는 별개다. 기존31표본/R01과 RHI 예산 경고는 REVIEW다.
- 다음 U5a-7은 실제 V007을 입력으로 R01 경계와 기존 StairRamp3의31표본을 정밀 검토한다. 고정 연결13,221표본/51공간을 유지하고 경로나 보호 폭을 빼지 않는다.
- 실제 결함에 국소 보정이 필요한 경우에만 새 V008 사본을 만든다. 현재 V008 맵/Generation_V008은 없다. 충돌/지각·Support/전체 베이크/PIE·다른 구역/세부 식생은 시작하지 않는다.
- 다음 보존 목록은 FrozenInputs_V007.files와 AcceptedHandoffHashes_V007.json을 경로/해시 대조로 중복 제거한다. 새 manifest 이전 채택본은 V007/Generation·Acceptance/실제26자세/몸체4+연결1이다.

현재 상태는 .context/Graphics/Current.md와 analysis/Phase01_U5aBoundaryReview.md를 따른다.
