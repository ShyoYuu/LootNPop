# Ridge U4-4 V004 큰 배치 초안

V003을 복제한 새 검토 맵에 메사 1단 남쪽 끝 암괴와 남쪽 외곽 절벽을 추가한다. 사용자가 2026-11-24 채택했다. 이후 동결한다.

- 이전 채택본은 V003 맵·저작 파일 해시이며 Z2 세 소유자와 Z4 두 소유자 모두 고정한다. Z2/Z4 JSON도 이 폴더의 독립 사본이다.
- 신규 `ASM_Z4_MesaRampToe`: PL_LargeCliff_01c 전용 사본, 로컬 `(-9900,-15100,H-1400)`·yaw -90°·크기 1. 830인스턴스.
- 신규 `ASM_Z4_MesaSouthWall`: PL_LargeCliff_02d 전용 사본, 로컬 `(-6500,-14500,H-1200)`·yaw -90°·크기 1. 8,206인스턴스.
- 두 조립체의 원래 내부 배치·공유 메시/재질·식생 층을 유지한다. 기존 남 테두리나 기반면은 늘리지 않는다.
- 원본 상자 대조에서 신규 각각 여덟 보호 공간 겹침 0. 전체 상자의 seam 최소 거리는 약64.36m/61.46m다. 실제 에디터 검사·export도 PASS다.
- 실제 충돌은 기존 V010만 계승한다. 몸체·충돌·지각·베이크·세부 식생·잔돌 마감은 후속이다. `body_handoff`에 채울 부피와 유지할 통로를 기록한다.
- 검사: UE Cmd `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeLargeLayout.py" export V004`.
- 근거: `Saved/Profiling/Graphics/Phase01/U4_4_V004/`. `Views_Before`는 V003의 신규 네 비교 시점이다. 기존 8시점도 같은 FOV90°·1305×724·Unlit로 대조한다.

실제 결과 `Export_20261124_232358_333279.json`: PASS·issues없음·exported=true·dirty없음. 기존 Z1/채택5소유자/보존51파일 불변, LI/로드22개, 신규 두 소유자 여덟 보호 공간 겹침0이다. 경고1건은 기존 북벽 AttachTo다. 사용자가 구도를 채택하고 다음 U4-5 진행을 승인했다.
