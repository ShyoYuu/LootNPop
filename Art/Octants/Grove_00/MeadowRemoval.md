# Meadow 정식 풀 제외

2026-11-10. 사용자가 Grove 포함을 확인하고 Meadow를 기존 플레이 풀에서 제외하도록 지시했다. `/Game/Maps/OctantPoolData`의 OctantDefinitions에서 Meadow1개, legacy OctantPool에서 Meadow1개를 제거했다. 남은 정의는 Bastion·Hollow·Acropolis·Pyramid·Grove5종이며 상대 순서·LVI/DA·회전255·seam signature를 그대로 보존했다. legacy 목록은 비었다.

Meadow의 원본·LVI·SurfaceData·기준 seam·테스트 fixture는 보존한다. 기존326개 에셋과 정식 Grove7개 에셋/설정의 SHA256는 변경되지 않았다. 풀 복구본은 Saved/SliceOctant/Grove_00/MeadowRemoval/OctantPoolData_Before.uasset에 있다. 지형·마커·DA가 바뀌지 않아 재베이크나 전체135개 검사를 반복하지 않았다. 런타임 C++/Live Coding은 변경·사용하지 않았다.

초기 검사4개 중2개 통과·2개 실패였다. 스폰 편성 검사는 과거 Package1P 회귀 배치에 Meadow를 넣으면서 현재 실제 풀에서 반드시 찾아야 한다고 가정했다. `LNPProductionSpawnCompositionTest.cpp`1곳(10줄 추가·1줄 변경)을 보정해, 현재 풀에서 빠진 Meadow는 보존된 원본 LVI/DA를 해당 과거 회귀 배치에만 사용한다. 실제 풀과 뒤의32개 seed별 배치는 Meadow 없는5종을 사용한다. 사용자에게 공유 에디터 종료·빌드 시점을 확인해 승인받은 뒤 `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 빌드 성공과 해당 검사 재통과를 확인하고 에디터를 재시작했다.

실제5종 풀에서 정의·Spawn Nav 투영은 최초 통과했고, 스폰 편성은 고정 회귀 배치의 Meadow 의존 조건 보정·Editor 빌드 후 재검사를 통과해 관련3개 항목을 검증했다. PodOverlay는 이번5종 구성에서도 동일 샘플 선택 assertion으로 실패했다. 기존 Pyramid 단독에서도 이미 재현된 항목으로 남기며 해당 검사 입력 검토가 후속 과제다. 검사별 오류/경고와 원본 이벤트는 `MeadowRemovalValidation.json`·Saved/SliceOctant/Grove_00/MeadowRemoval/Tests가 소유한다. 이전 정식6종 풀의 전체135개 검사 기록은 역사적 결과로 보존한다.

TestMap03 혼합 PIE에서 Meadow 제외·8slot/274bindings·seam 높이 차0cm·blocked seam nodes/edges0·Pod120/120·적2,087/2,087(밀도1)을 확인했다. Grove는 slot1·6에 배치됐다. 직전 사용자의 Grove 경계 왕복·Pod 교전 승인과 이번 실제 포함 확인은 유지한다. 이번 변경에서 추가 지형 전체 수동 검토는 요청하지 않는다.

현재 플레이 풀 확인 범위에서 추가 수동 확인은 없다. 남은 PodOverlay 샘플 선정 검토와 정식 아트·밸런스·이후 Ridge/경로 성능 재측정은 별도 후속 범위다.

Editor 빌드 후 스폰 편성 재검사는33개 구성에서 적67,495/67,495기·shortArrangements0으로 통과했다(오류/경고0). 재시작 뒤 정식5종 풀 PIE도8slot·293bindings·seam 높이 차/blocked seam0·Pod120/120·적2,280/2,280으로 초기화했다. 현재 Grove는 slot1·5다. 로그는 `CompositionRecheck`와 `pie-after-build.json`에 보존한다.
