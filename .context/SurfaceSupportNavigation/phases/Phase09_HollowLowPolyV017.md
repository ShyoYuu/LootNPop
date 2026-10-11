# Hollow 전체 단순화 V017 제작·검증 상태

2026-10-10. [전체 재제작 지시](Phase09_HollowLowPolyRebuild.md)의 외피 확정본이다. 큰 공간 구성/복구 위치는 [V010](Phase09_HollowLowPolyV010.md), 외피 경계·천장·Support 띠·구면 삼각분할은 [V016](Phase09_HollowLowPolyV016.md)을 따른다. **V017 생성·베이크와 장치 포함 정식2개 검사 성공. 오류/경고0, 합계3.606초. 사용자는 전체 PIE 탐색 후 틈/한쪽 면/깨진 형상 없이 외피를 확정했다.** CaveLift 지하/지상 복귀, Basin↔절벽 정상, Shuttle 패널 왕복도 확인했다. 이후 요청한 훅 정돈·Shuttle 섬 거리·패널9배 면적·검토점 수정은 [장치 조정 V018](Phase09_HollowDevicesV018.md)이 소유하며 내부 V017 외피를 유지한다.

## V016 결과와 장치 수정

- V016 생성 lint=0/베이크 성공. `HollowContent`와 `ProductionSurfaceDataDefinitions` 2개 성공, 오류/경고0, 합계3.649초. 경로 바닥6455/양방향 캡슐12730, 지각 접합 바닥2592/왕복 캡슐, 두 탈출구·주요 Nav 왕복·Pod8/8회전/16seed 편성4+6·Shuttle 정적 단절을 통과했다. Pod8 머리 공간3300cm 확인.
- 이후 같은 격리 Chaos 월드에 장치 검사를 추가했다. 26개 앵커의 대표 접근점→실제 마커 Visibility 광선/개별 거리, 패널2개 전 구간의 3트랙 양방향 탑승 캡슐을 검사한다. 패널은 통과했지만 앵커1/2/3/7/12/17은 지형 가림,7은 거리도 실패했다. 최초2개 검사 성공을 장치/PIE 성공으로 확대하지 않는다.
- 앵커1/2의 Y−2100→−1400,3의 Y−2100→−800: 상부 잔도 전면에서 보이도록 이동. 앵커7 Y1700→1000/MaxDistance8000→10000. 앵커12 Y1400→−2200: Ascent 진입면 쪽으로 이동. 앵커17 (−5000,0,H−5500)→(0,0,H−10000): 공동 천장 안의 위치를 중앙 수직 입구 아래로 이동.
- 북쪽 절삭은 X2200~3400/Y9500~10100, H−9100~300. 축(2500,9800)은 유지하고 오른쪽으로 넓혀 중간 판 전체가 빈 공간 안에 있게 한다. CaveLift 중심(3150,9800,H−5500), 반폭 실제150cm/두께200cm. 축의 직접 탈출을 막지 않는다.
- `HollowCaveMesaEntry` 검토점은 램프 안의 옛 위치 대신(3800,9800,H0)으로 옮겼다. 관련 중간 판 검토점도 갱신했다.

## V017 원본·반입 확인

- Blender67766 tris, 닫힌 성분1, manifold/방향/중복/퇴화 오류0, 공유 정점 없는 면 교차0, 역할 winding 보존, Support 정점 좌표 상한 통과. 예상 투영 법선 최대 오차4.589°.
- 통로 바닥6455점/누락0/최대 경사34.145°, 접합2592점/누락0/최대22.644°, 탈출구 광선10개 막힘0. 제안한 앵커26개 원본 접근선에서 마지막 가림1개를 찾아 앵커3을 추가 이동했다. 해당 재검사도 통과했다. UE 조립 검사는 별도다.
- UE 역할8개 삼각형 수 일치/bounds 최대 차이0.005738cm, LOD0/별도complex없음/단면complex-as-simple 확인·저장.
- 산출물 `Saved/SliceOctant/Hollow_00/LowPolyV017/{Sources.blend,HollowLowPoly.blend,manifest.json,*.fbx}`. UE 후보 `/Game/Maps/OctantWorkflow/HollowLowPolyV017`, production `Authored/SM_Hollow_00_LowPolyV017_*`. 이전 버전 배치는 접두사로 제외한다.

## 최종 자동 검사와 비용

- 생성 lint0/Authored8/errors0. 지각94224+코드 판3096+Blender67766=165086 tris. 기존453640 대비63.61% 감소. 엔진 기본 Cube2개와 장치/램프 메시를 제외한 동일 지형 집계다.
- `HollowContent`와 `ProductionSurfaceDataDefinitions` 성공2/실패0/오류0/경고0. 통로 바닥6455점/5트랙 양방향 캡슐12730회, 접합부 바닥2592점/양방향 캡슐, 탈출구2개, 앵커26개 Visibility 접근/거리, 패널3트랙 양방향 탑승 캡슐264회 통과.
- 전체8slot 회전에서 주요 Nav 왕복, 공동 연결, Shuttle 정적 보행 단절, Pod8/머리 공간/가장자리 여유,16seed의64개 encounter별 Flyer4+Crawler6 편성 유지 통과. 실제 Mover/훅 궤적/적 공격 성공을 뜻하지 않는다.

| 지표 | 이전 상세 지형 | V017 |
|:---|---:|---:|
| 지형 삼각형 | 453640 | 165086 |
| Support sources / Layers | 101 / 133 | 12 / 24 |
| TotalSamples | 2128909 | 2154328 |
| Support payload bytes | 15107739 | 15156232 |
| Nav cells / components / portals | 137109 / 663 / 2744 | 185567 / 25 / 1155 |
| Portal 후보 distance / step / clearance | 694126 / 643754 / 434223 | 157971 / 152069 / 81893 |
| Portal bake seconds | 152.292 | 9.994 |
| Nav / traversal payload bytes | 1116520 / 74591 | 1503884 / 37707 |
| Spawn candidates / payload bytes | 12349 / 496452 | 12704 / 510652 |
| seam endpoints | 1107 | 1107 |

동일 N735/역할별 기본 격자 옵션의 로그 비교다. 지형과 연결 배치가 바뀌었으므로 순수 알고리즘 벤치마크가 아니다. 포털 베이크 시간은93.44% 감소했으나 Support 샘플+1.19%/payload+0.32%, Nav payload도 증가했다. 폴리곤 감소를 메모리·런타임 성능 개선으로 확대하지 않는다. 지각 NeedsExact는13.23→37.01%지만 내부를 Blender Support로 넘겨 지각이 제거된 표본도 포함한다.

## 마지막 PIE 묶음

TestMap03 서버 PIE에서 `LNP.Debug.TeleportToReview`로 검토점과 Hollow slot을 확인한다. 이후 `LNP.Debug.TeleportToReview <Name> <Slot>`을 사용한다. 최초 자연 진입·왕복 구간은 해당 시작점으로만 이동하고 실제로 걷는다. 실패 위치는 `LNP.Debug.WhereAmI` 결과와 진행 방향을 함께 남긴다.

1. `HollowEntry`→왕복 산→`HollowPodWest`, `HollowOuterEast`→`HollowRimIsland`→`HollowNotch`를 왕복 걷는다. 외곽 지각 접합과 경사 양 가장자리의 걸림/추락도 본다.
2. `HollowCaveWest`↔`HollowSecretMouth`를 왕복한다. `HollowCaveShaft`의 중앙 출구와 북쪽 축(2500,9800)을 훅으로 탈출하며 중간 판 `HollowCaveLift` 착지와 지상 복귀를 본다. 북쪽 지상 접근점은 `HollowCaveMesaEntry`다.
3. `HollowBasin`→양쪽 잔도→절벽 정상→`HollowAscent`→`HollowArena`의 훅 조준/당기기/해제/착지, 중도 낙하 후 자력 복귀를 본다. 서쪽 잔도는 `HollowCliffLow`, 횡단 패널은 `HollowCliffPanel`에서 접근한다.
4. 횡단 패널과 `HollowShuttleDock`↔`HollowShuttleIsland`를 양방향 탑승한다. 대기·접안·플레이어 공간과 모든 인접 가장자리의 점프 우회/훅 복귀를 본다.
5. `HollowPocketWest/East/Back`의 굴 출입·Pod 전투 공간, 공동 Pod2곳·Shuttle Pod, 옆/하부로 떨어졌을 때 보이는 복귀 경로를 본다. 적을 끄고 동선을 확인했다면 적을 켠 PIE에서8개 편성/합류·교전도 확인한다. 알려진 벽보행 LoS 수정은 별도 세션이다.

위 묶음의 사용자 전체 탐색으로 V017 외피는 확정됐다. 후속 장치·훅은 V018~[V020](Phase09_HollowDevicesV020.md)의 베이크/자동 검사와 사용자 변경 구간 PIE까지 통과해 제작을 완료했다. 실제 NPC 교전 성공까지 확대 해석하지 않으며 Phase9의 NPC/풀 조합/패키지/부하 Gate는 남는다. 이 제작 방식의 다음 적용은 [Bastion 단순화 인계](Phase09_BastionLowPolyRebuild.md)를 따른다.
