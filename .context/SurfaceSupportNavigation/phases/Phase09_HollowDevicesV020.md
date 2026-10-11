# Hollow 잔도 관성 착지·정상 도착·외곽 복귀 V020

2026-10-11. V019 사용자 PIE에서 호스트·게스트 이동 패널 왕복과 중력 회전은 채택됐다. 훅 정돈은 개선됐으나 일부 잔도에서 종료 관성으로 떨어졌으며, 긴 정상 두 지점과 외곽 추락 다섯 지점의 추가 복귀를 요청했다.

**현재: WestMid 수정본 재생성 lint0·베이크 완료, 실제 저장 조립의 정식2검사 모두 성공(3.393초/오류0/경고0). 25접근/22착지/643관성낙하/1044패널 sweep을 통과했다. 2026-10-11 사용자 마지막 변경 구간 PIE도 “검토 완료, 문제 없음”으로 통과해 V020을 완료했다.** 승인 V017 외피8개 SHA256·패널 BP 일치, 섬·패널·이동 런타임은 유지한다. 복구는 `Saved/SliceOctant/Hollow_00/DevicesV020/Before`다.

## 배치와 근거

- 잔도9개는 각1개를 유지하며 모서리에서 안쪽으로 옮겼다. WestLow→PocketLip→Mid→High→Top, Mid→Panel 분기와 EastLow→Panel→Mid→High→Top을 유지한다. 겹친 잔도 밑면을 피하는 높이와 착지 폭을 함께 조정했다. 서쪽 Low에서는 약2m 왼쪽에서 PocketLip을 당기고, 굴 입구 잔도에서는 약8m 왼쪽(-4200,−2800,H−2500)에서 Mid를 당기면 캡슐이 윗잔도 모서리를 피한다. Mid 착지 후 High는 약4m 오른쪽(-2150,−2650,H1500)에서 당긴다. 잔도 전체 중앙을 가정하지 않고 실제 노출된 착지면을 사용한다.
- 정상 도착은 `WestTopNorth`(4650,−4700,H11000)와 `EastTopSouth`(−8500,5000,H12000)를 추가했다. 사용자 두 정상 좌표의 발밑 H는 각각6000/10000이며, 좌표는 로그와 역투영으로 **slot0**임을 확인했다.
- 외곽 복귀는 `EastOuterRecovery`(−5000,6800,H10000), `WestOuterRecovery`(3500,−8250,H16000), `MountainWestRecovery`(−8500,−11000,H12000), `MountainEastRecovery`(4500,−11000,H8500). 인접한 동쪽 보고2곳은 안부 복귀1개로 통합했다. 전체 수동 훅19→25개, 수직 축3개를 제외한 착지 훅22개다. 추가 시드 훅 제외 정책은 유지한다.
- 벽에 붙은 원 보고점에서 대각선으로 당기면 캡슐이 벽에 먼저 닿는다. 외피를 바꾸지 않고 동쪽 두 번째 지점에서 약2~3m, 서쪽/산 양쪽에서 약3m 물러나 당길 공간을 확보했다. 보고 원점과 `HollowRecovery*Launch`를 모두 검토점으로 남겼다. 원점에서 곧바로 캡슐이 통과한다고 기록하지 않는다.
- 보고 좌표의 slot0 역투영은 정상(4662,−3884,H6099)/(−8206,4184,H10100), 외곽(−4645,7295,H100)/(−5052,7156,H99)/(3116,−7429,H99)/(−8088,−9789,H100)/(4134,−12205,H99)다. Pawn 높이를 포함하므로 바닥 검사는 H0/6000/10000에서 한다.

## 검사와 한계

- V019의 방사 광선·±60cm 바닥만으로는 종료 관성을 검증하지 못했다. BP 실제 설정의 정지 거리150cm·종료 속도600cm/s, 중력2000cm/s², 무입력 측방 감속200cm/s²·종단2000cm/s를 기준으로 60Hz 낙하를 예측한다. 게임 이동 설정은 바꾸지 않았다.
- 승인 V017 blend와 코드4개 섬을 동일 투영한 BVH에서 수정/추가17훅 접근선 가림0·사거리 초과0, 첫 착지면 오차 최대2.7cm, 주요 잔도/정상13개의 반경400cm×16방향 바닥을 확인했다. 외곽 복귀는 반경100cm이며 관성이 지면 안쪽으로 진행하는 경로를 별도로 확인한다.
- 최초 원본 검사는 캡슐 축5점 근사 낙하547사례 실패0이었으나, 근사 접근 판정이 실제 Chaos보다 많은 합성 경로를 제외했다. 따라서 해당 통과 수치를 최종 착지 증거로 사용하지 않는다. 실제 첫 Chaos652낙하에서 WestMid5방향이 H−7498 협곡 바닥으로 떨어지는 것을 발견했다. 진단 코드는 `DevicesV020/preflight.py`, `capsule-preflight.py`, `pull-preflight.py`와 JSON 결과다.
- 정식 `HollowContent`에 수정17훅의 캡슐 접근·종료 관성 낙하·대표 출발점/주변의 착지100cm 여유, 주요13훅의400cm 바닥·여러 진입 방향을 추가했다. 기존 외피/접합/축/Nav/Pod·패널 검사는 유지한다. 원본 검사에서 통과해도 실제 조립 검사 결과를 먼저 확인한다.
- 첫 생성 lint0/8반입/25훅/2패널/8Pod/40검토점, 베이크12Support/24Layer/2154508샘플/185568Nav셀/165086tris를 확인했다. `ProductionSurfaceDataDefinitions` 성공1.092초, `HollowContent` 실패3.303초/WestMid5오류/경고0. 결과는 `DevicesV020/first-formal-results.json`에 보존했다.
- WestMid를(-2150,−2650,H5700)에서(-2500,−2850,H3800)으로 낮추고 안쪽으로 옮겼다. 원본 후보 검색은 접근 캡슐 근사로 합성 경로를 제외하지 않고 광선 접근이 가능한 방향을 검사했다. 실제 저장 전 후보는 테스트 중에만 마커 위치를 바꾸고 원래 transform으로 복구하여 기존 지형의 Chaos를 검사했다. 25접근/22착지/643관성낙하/1044패널 sweep 성공3.248초/오류0/경고0이다. 이 결과는 수정된 저장 LVI·베이크 검사 완료를 뜻하지 않는다. 증거는 `DevicesV020/west-mid-candidate-chaos.json`이다.
- 후보 임시 적용 코드를 완전히 제거하고 Live Coding patch_5 성공, 링크 오류 없음. 재생성·베이크 뒤 저장 조립에서 `LootNPop.SurfaceNavigation.Bake.HollowContent`, `LootNPop.SurfaceNavigation.Runtime.ProductionSurfaceDataDefinitions`를 실행한다. 패널 런타임 변경은 없으므로 승인된 패널 PIE를 반복 요청하지 않는다.
- 수정본 사용자 Build/Bake 완료 후 저장 조립의 `HollowContent` 성공3.233초·`ProductionSurfaceDataDefinitions` 성공0.160초, 총3.393초/오류0/경고0. 외피6455바닥/12730왕복5트랙 캡슐 sweep·접합2592바닥, 훅25접근/22착지/643관성낙하, 회전 패널1044 sweep과 기존 Nav/Pod 검사를 통과했다. 생성 lint0/25훅/2패널/8Pod/40검토점, 베이크12Support/24Layer/2154508샘플/185568Nav셀/165086tris다. 최종 증거는 `DevicesV020/final-formal-results.json`, `repaired-build-bake.log`, `interior-sha256.json`이다. 원본 근사나 임시 후보 검사가 아닌 저장 조립 검사이며, 실제 Mover의 입력/착지감은 아래 PIE로 확인한다.

## 마지막 변경 구간 PIE — 2026-10-11 사용자 통과

사용자 응답은 “변경 구간 검토 완료, 문제 없음”이다. 아래 요청 묶음의 통과로 잔도 착지·정상 도착·외곽 복귀 조정을 채택했다. V017 전체 외피 검토와 V019 호스트·게스트 패널 검토를 합쳐 Hollow 전체 단순화 재제작·장치·훅 조정은 완료다. NPC LoS·풀 통합·부하 측정은 이번 완료 범위에 포함하지 않는다.

1. `HollowBasin`에서 양쪽 잔도 체인/Panel 분기. 서로 다른 방향에서 당긴 뒤 무입력 착지와 다음 목표 식별을 확인한다. 서쪽 굴 입구에서 Mid로 갈 때 약8m 왼쪽, Mid에서 High로 갈 때 약4m 오른쪽으로 걸어 밑면을 피한다.
2. `HollowTopWestNorth`, `HollowTopEastSouth` 주변에서 새 정상 훅의 착지·전장 연결을 확인한다.
3. `HollowRecoveryEast01/East02/West/MountainWest/MountainEast`에서 가까운 복귀를 확인한다. 벽에 붙은 곳은 대응 `HollowRecoveryEastLaunch/WestLaunch/MountainWestLaunch/MountainEastLaunch`까지 몇 걸음 물러나 당긴다. 두 동쪽 지점은 같은 안부 훅을 쓴다. 긴 우회 없이 복귀되는지를 검토한다.

실패는 현재 Hollow slot과 `LNP.Debug.WhereAmI`, 이동 방향을 남긴다. 이번 완료 범위는 훅 조정이며 NPC LoS는 별도 세션이다.
