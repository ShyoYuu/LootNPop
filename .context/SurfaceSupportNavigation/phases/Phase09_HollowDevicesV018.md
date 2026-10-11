# Hollow V017 확정 후 장치 조정 V018

**후속 PIE 판정:** 패널 크기는 사용자 채택. 협곡 훅의 정돈/잔도 착지는 실패했으며 Shuttle은 마주 보는 서쪽 절벽으로 연결하도록 요청받았다. 최신 실행 기준은 [V019](Phase09_HollowDevicesV019.md)다. 아래 자동 통과 기록은 당시 검사 범위이며 장치 완료로 해석하지 않는다.

2026-10-10. 사용자는 V017 전체 맵을 면밀하게 플레이하며 메시 틈·한쪽 면·깨진 형상을 발견하지 못했고, 외피를 확정해도 좋다고 확인했다. CaveLift 지하 착지/지상 복귀, Basin↔절벽 정상 왕복, Shuttle 패널 왕복도 성공했다. 추가 요청은 Rim 복귀 훅, 잔도별 훅 정돈/중복 통합, Shuttle 섬 거리, 패널2개의 3×3 확대, CaveMesa 검토점 수정이다. 실제 NPC 교전 성공으로 확대 기록하지 않는다.

**승인된 내부 외피는 LowPolyV017 그대로다. V018은 코드 소유 섬 위치·장치·검토점 조정의 식별자다. Live Coding patch_11/생성 lint0/베이크 성공. 정식 검사2개 성공, 오류/경고0, 합계3.660초. 수정 구간 사용자 PIE만 남았다.**

## 수정 내용

- 서쪽 훅은 Basin→Low→PocketLip→Mid→High→정상, Mid→Panel 잔도 분기로 정리한다. 동쪽은 Basin→Low→Panel→Mid→High→정상이다. 잔도마다 하나를 두고, 위 잔도에 가리지 않도록 전면 돌출을 조정한다. 서쪽 정상 훅은 High 바로 위, 동쪽 정상 훅은 EastHigh 바로 위로 이동했다.
- 잔도 훅 사거리는 서쪽60/60/70/60/60m, Panel60m, 동쪽65/105/70/80/60m다. 실제 이전 착지점에서 다음 훅까지44.6~88.3m를 확인했다. 지형은 바꾸지 않고 각 단계 거리와 여유로 설정한다. Ascent140m/Arena90m, Dock↔Shuttle130m, 굴/수직 축도 역할별로 조정했다.
- 중앙 공동의 Pod 전용 훅과 중앙 수직 축 아래 훅은 가까우며 접근 역할이 겹친다. 축 아래 훅(0,0,H−10000)이 Pod 접근도 겸한다. 기존 Anchor_021을 Rim 복귀 훅(4000,11000,H4500)/65m로 바꿔 전체26개를 유지한다. 북쪽 축의 위/아래 훅, BackPocket/Dock, 양쪽 잔도 훅은 서로 다른 높이·착지 공간의 단계이므로 유지했다.
- Rim 훅은 CaveLift 지상 출구→U턴 경사로 복귀와 경사로 바깥 접근을 겸한다. 출구 옆 지면(1800,9800,H0), 축 위(2500,9800,H600), 바깥 동/북쪽(6000,11000,H0)/(4000,13000,H0)에서 접근을 검사한다. 처음 제안한 H3700은 경사로에 가려 H4500으로 올렸다.
- Shuttle 섬과 Pod5/검토점은(5000,−12500,H6000)→(−9000,−14500,H6000). 크기·두께·높이는 유지하며 Dock 검토점과 실공간 중심 거리는111.85m다. 낮은 산 경사로 위를 지나는 별도 이동 장치 공간을 만든다. 정적 보행 NoPath는 수정 후 베이크로 다시 확인한다.
- Hollow 전용 `/Game/Maps/Hollow_00/BP_HollowMovingPanel`은 기존 `ALNPMovingPanel`의 PanelMesh 기본 scale만(4,4,0.3)→(12,12,0.3), 표면400×400→1200×1200cm/두께30cm다. 생성기는 Hollow에만 이 클래스를 쓰며 다른 옥탄트 패널과 런타임 이동/복제 구현은 바꾸지 않는다. 클래스 부재 시 생성 전에 중단한다.
- Shuttle 패널은(6000,−8300,H6135)↔(−6500,−13250,H6135),600cm/s/끝 대기3초. 횡단 패널은(3000,−1300,H2015)↔(3000,1300,H2015),400cm/s/3초. 커진 모서리가 기존 접안 위치에서 지형을 침범해 위치·높이를 조정했다. 고정 자세로 이동하는 기존 패널의 구면 접안 높이차를 함께 고려한다.
- `HollowCaveMesaEntry`는(3800,9800,H0)→(1800,9800,H0). 옛 점은 U턴 암반 내부/지면 아래로 놓여 바닥 광선이 실패했고, 새 점은 실제 바닥을 확인했다. 런타임의 Up150cm 텔레포트 캡슐도 정식 검사에 추가했다.

## 검증과 복구

- 복구: `Saved/SliceOctant/Hollow_00/DevicesV018/Before/{Hollow_00,LNPSliceOctant.cpp,LNPHollowContentTest.cpp}`. 새 패널 BP 생성/저장 이후, 지형 조정 생성 이전 스냅샷이다. 내부 V017 원본은 별도로 유지한다.
- 원본 사전 검사: V017 Blender 외피+이동할 코드 판의 투영 격자에서26개 접근선 가림0/사거리 초과0. 확대 패널은 앞/뒤·좌/우540cm의9트랙1125점에서 캡슐 축→외피 최소 거리52.1/49.8cm로 반경42cm를 넘겼다. 이는 실제 UE Chaos 캡슐 검사 전의 진단이다. 접안 네 끝 모두 기존 바닥과 판의 겹침/점프 높이 범위 내 접근을 확인했다.
- `HollowContent`에 이전 잔도→다음 훅의 실제 Chaos 가시선/사거리, Rim 추가3방향, Mesa 텔레포트 바닥/캡슐, 두 패널 CDO 크기1200×1200×30cm, Shuttle 경로80m 이상, 각 접안 끝 점프 높이140cm 이내 바닥 겹침,9트랙 양방향 탑승 캡슐을 추가/갱신했다. 기존 외피 통로·접합·탈출 축·Nav·Pod8/16seed 검사는 유지한다.
- 사용자 생성·베이크 완료 확인 후 로그의 Authored8/errors0/lint0, 앵커26/패널2/Pod8/검토점29를 확인했다. 지형165086 tris는 유지한다. Support12/Layers24/TotalSamples2154508/payload15157500 bytes, Nav185568 cells/25 components/1155 portals, 후보157943/152041/81893/seam1107, 포털 베이크10.483초, Nav/traversal payload1503892/37707 bytes, Spawn후보12699/payload510452 bytes다.
- 승인된 V017 내부 역할8개 uasset의 생성 전/후 SHA256 일치를 확인했다. 런타임 생성은 기존 `SpawnActorDeferred`의 `MultiplyWithRoot` 기본값을 사용하므로 전용 BP의 루트 기본 scale이 유지된다. 런타임 패널 이동/복제 코드는 수정하지 않았다.
- `LootNPop.SurfaceNavigation.Bake.HollowContent` 성공3.469초, `LootNPop.SurfaceNavigation.Runtime.ProductionSurfaceDataDefinitions` 성공0.191초. 오류/경고0. 통로 바닥6455점/5트랙 양방향 캡슐12730회, 접합 바닥2592점/왕복 캡슐, 탈출 축2개, 훅26접근+Rim3방향, 패널9트랙 양방향 캡슐2214회,1200×1200×30cm 기본값, 접안4끝, Mesa 바닥/텔레포트 캡슐 통과.8slot 회전/16seed의64encounter별4+6 편성, 주요 Nav 왕복, Shuttle 정적 NoPath도 유지한다. 결과는 `DevicesV018/unreal-verification.json`에 보존했다.
- 다음 순서: **변경 구간만 마지막 PIE 묶음**. 위 자동 검사는 실제 훅 궤적·Mover 탑승·넉백 중 사용성·교전 성공의 증거로 확대하지 않는다.

## 수정 후 PIE 묶음 — 자동 검사 통과 후에만

1. `HollowCaveMesaEntry` 즉시 안전 착지, CaveLift 지상 복귀→Rim 새 훅→U턴 경사로, 경사로 바깥에서 같은 훅으로 복귀.
2. `HollowBasin`→서쪽 Low/PocketLip/Mid/High/정상과 동쪽 Low/Panel/Mid/High/정상. 각 잔도에서 다음 훅 발견·조준·착지와 중도 낙하 복귀, 서쪽 Mid→Panel 분기.
3. 횡단 패널의 넓어진 가장자리 포함 양방향 탑승/접안. `HollowShuttleDock`↔새 `HollowShuttleIsland` 패널 왕복, 산 경사로와의 공간 분리, 훅 왕복, 이동 중 넉백/추락 후 복귀. 전투 중 사용성은 사용자 플레이로 판단한다.
4. 중앙 공동 Pod→통합 훅→중앙 수직 출구. 멀어진 Shuttle Pod의 편성/전투 공간도 함께 확인한다.

현재 슬롯은 다음 서버 PIE에서 `LNP.Debug.TeleportToReview`로 다시 확인한다. 실패는 `LNP.Debug.WhereAmI`와 이동 방향을 받는다.
