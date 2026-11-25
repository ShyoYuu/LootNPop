# U5a V006 유지·새 세션 저작 기준

사용자가 U5aBody_V006의 현재 외관을 유지하고 다음 단위를 새 세션으로 넘겼다(2026-11-25).

- 현재 채택 원본: `Acceptance_V006.json` / 원시 보존 해시412파일: `FrozenInputs_V006.json`.
- 맵/저작: U5aBody_V006 / Generation_V006.json. 몸체 형상은 U5a_5_V001/Geometry_V005이며 맵 판과 다르다.
- Generation_V006.json의 accepted=false와 previous_trial V005는 제작 당시 검사 기록이다. 채택 표시를 위해 원시 파일을 수정하지 않는다.
- BodyTrial.json·README의 미착수/U5a-1 상태는 입력 고정 당시 이력이며 현재 상태가 아니다.
- 실제26자세·기존 Z1/추가21개·연결/재질 보존 PASS, 반입4몸체73,111정점 최대차이0.004367cm·전체 dirty없음이다.
- V006 유지와 자기 교차339후보/기존 StairRamp3 31표본 REVIEW·충돌/PIE 게이트는 구별한다.
- 로컬 U5a_5_V001/AcceptedHandoffHashes_V001.json·AcceptedHandoff/에 채택43파일 복구본, AcceptanceValidation_V001.json에 검증을 남겼다.
- 다음 보존 목록은412+43을 경로/해시 대조로 중복 제거한417파일이다. 이전 채택본은 V006/26자세/몸체4+연결1이며 V005 제작 이력을 복사하지 않는다.
- 다음 U5a-6/U5aBody_V007/Generation_V007은 미착수다. 먼저339후보를 실제 관통/접촉/공면/퇴화 면으로 분류하고 새 사본에서만 최소 보정한다.
- 기존 생성기/반입기 재실행·U4 재시작·원본/공유 에셋 수정은 하지 않는다. Content/Saved는 로컬 보존한다.

상세 인계·필수 검사·새 세션 프롬프트는 [.context/Graphics/analysis/Phase01_U5aBodyHandoff.md](../../../../.context/Graphics/analysis/Phase01_U5aBodyHandoff.md)를 따른다.
