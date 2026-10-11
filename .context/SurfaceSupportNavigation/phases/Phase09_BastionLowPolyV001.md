# Bastion 전체 단순화 V001 — 메시·외피 사용자 확정

2026-10-11 착수. 전체 단순화와 Meadow 동굴 입구 절단을 물려받은 지각 구멍의 원인 수정을 완료했다. 최신 재베이크·원본·지각 경계·실제 Chaos·8slot/Nav/Pod·회귀 정식7검사가 모두 통과했다. 사용자는 구멍 복구·맵 전반 추가 틈/구멍 미발견을 확인하고 메시·외피를 확정했다. 후속 PIE에서 패널 크기·훅 과밀/의도·관성 착지 개편을 요청했다. 최신 장치 작업은 [V002](Phase09_BastionDevicesV002.md)가 소유하며, 광원 성능 조정은 후속이다. Pod 전투까지 확인한 것으로 확대하지 않는다.

## 복구와 기준선

- 복구: `Saved/SliceOctant/Bastion_00/LowPolyV001/Before/`에 Bastion 전체 에셋, 상위 폴더에 변경 전 `LNPSliceOctant.cpp`.
- `baseline-meshes.json`: 지각227585 + Generated136940 = 364525 tris, 메시210개. Authored 없음. Decoration 포함, 엔진 Cube/조명 구슬 제외.
- `baseline-markers.json`: 훅135·패널1·Pod12의 실제 저장본 transform. 마커 클래스 전체136개.
- `baseline-header.json`: Support1856557샘플/13183313bytes, Nav150808셀/1232936bytes, Traversal3152요소/57747bytes, Spawn13084요소/526620bytes. 원본 manifest203개.
- 작업 전 `BastionEncounters`·`HollowContent`·`ProductionSurfaceDataDefinitions`: 성공3, 오류/경고0, 3.505초. 새 형상 검증을 대신하지 않는다.
- `preserved-hashes.json`: 승인 Hollow/Meadow의 작업 전 SHA256. 변경 후 비교한다.

## 공간과 소유권

평면: 외성벽→빈 북/남 안뜰→성채→중앙 심연/나선. 안뜰 여섯 폐허는 지붕 연결로 두 환형 통로와 접속하며 B01/B05는 계속 비운다. 공중 I01~I12는 성벽/성채에서 정상으로 이어지는 별도 순환, 남쪽 V01~V04는 빠른 상승, 지하 C01~C06은 두 탈출 체인이다.

단면: 공동−100m→지상0→성채20/40/60/80m→정상180m. 중앙 기둥 반경10m·틈2m·나선폭9m와 기존 회전당 상승 높이를 보존한다. 정상 반경40m·나팔 실루엣과 마지막 나선의 머리 공간을 유지한다.

| 구역/교체 ID | 새 원본/외피 소유자 | 연결/보존 |
|:---|:---|:---|
| 지각 | 기존 `DeriveCrust`·동일48각형 심연 절단 | R300m, Meadow 이음매15m/전이20m 유지 |
| W01/W02 | `LNPBastionLowPoly.inl`의 수직 구간 합집합 | 문·경사·층계참을 공유 UV 경계로 생성. 내부 성벽판/장식 계단 제거 |
| B02/B03/B04/B06/B07/B08/S04/S08 | 같은 코드의 건물 부피 합집합 | 문·폐허·층 바닥 구멍·실제 경사로 유지, 슬래브 구멍 모서리와 경사 옆면 닫음 |
| D01~D59 | 닫힌 구면 판 | 기존 BuildingPoint의 지붕 접속과 폭/끝점 유지 |
| I01~I12/T01~T03/C01~C06/V01~V04 | 닫힌 다각 원판 | 큰 전투 발판·독립 섬·엇갈린 상승/복귀 유지 |
| P01_Core/P01_Dish | 기둥과 나팔을 한 코드 부피로 대체, P01_Dish 소유 | 내부 이중 원통 제거, 두께 있는 정상 나선 개구부 |
| P01_Ramp0~2 | 닫힌 나선 띠의 높이별 역할 조각 | 높이 분할 단면은 공유하고 전체 양 끝만 닫음 |
| A01 | 공동 바닥·천장·수직 통로의 한 코드 부피 | 동일 심연 절단 단면. 공동 바닥만 Coarse Support |
| Pod/훅/패널/등불/엄폐 | 기존 Bastion spec | Pod12/편성125, 훅135/개별 사거리, 패널1 유지. 성채 훅과 일부 다리 훅 위치는 새 착지부에 맞춰 조정 |

이번 구조는 규칙적인 구면 판·환형/직교 부피로 재현할 수 있어 코드가 소유한다. Blender FBX 반입을 추가하지 않는다. Hollow 전용 생성/장치 경로는 변경하지 않는다.

## 현재 검증과 다음 순서

1. 에셋을 쓰기 전 외피 그룹의 경계·비다양체·winding·퇴화/중복·Support int16 범위를 검사한다. 나선 높이 분할은 합쳐 검사한다. 자기교차와 실제 조립 충돌은 별도다.
2. Live Coding 첫 시도에서 신규 initializer 오류1건과 기존 Unity 묶음의 익명 namespace 태그 이름 충돌을 확인했다. 신규 오류를 수정했다. Git 제외 로컬 XML의 Unity 해제는 Live Coding의 기존 묶음에 적용되지 않아 원복했다. `LNPOctantTriangleExtractor.cpp`의 두 파일 내부 태그 변수만 `ExtractorSupportTag`/`ExtractorCoarseSupportTag`로 바꿔 충돌을 제거했다. 태그 값과 판정은 같다. 이후 C++ 컴파일은 통과했으나 Live Coding 패치가 LNK2011(PCH 미링크)로 적용되지 않았다. 에디터를 종료하고 `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 전체 빌드를 실행해 성공했다(116.67초, 종료 코드0). 새 원본 자동 검사는 재개한 에디터에서 실행할 예정이다.
3. 원본 검사→저장 LVI의 Chaos 바닥/양방향 캡슐·접합/훅/패널 검사→베이크→편성/8slot/Nav 검사를 진행한다. 검사 오류를 임의 제외하거나 기준을 낮춰 통과시키지 않는다.
4. 최종 비용 표·마커 비교·Hollow/Meadow 해시 비교를 기록한 뒤 마지막 PIE를 요청한다. 완료 표시는 사용자 PIE 이후다.

## 원본 사전 검사

- 첫 검사: 97그룹·49678삼각형, 건물8그룹에서 경계4~8개 검출. 나머지 공동/나팔/나선/성벽/섬/다리는 통과.
- 원인: 경사로 윗면이 옆 슬래브 밑면 높이를 지나는 셀에서 구간의 상하 순서가 바뀌었다. 슬래브 두께에 따른 실제 교차 X를 공유 격자에 추가했다.
- 수정 후 Live Coding 링크 성공. BastionLowPolySource: 성공, 오류/경고0, 0.036초. 97그룹·50886삼각형, 경계/비다양체/winding/퇴화/중복/int16 오류0.
- 이 수치는 구조물 후보만 포함한다. 지각/Decoration과 저장 에셋의 실제 조립은 다음 단계다. 자기교차 통과를 의미하지 않는다.
- BP_LNPPlayer 실조회: 그래플 정지150cm·종료속도600cm/s, 중력2000cm/s². 낙하 설정은 추가 실조회해 관성 검사의 입력으로 기록한다.

## 첫 생성 전 재개 지점 — 당시 기록

- 에디터 재시작 후 연결 복구. 원본 검사는 통과했으며 새 에셋은 아직 생성하지 않았다.
- 사용자에게 TestMap03/PIE off에서 LintSliceOctant Bastion_00 → BuildSliceOctant Bastion_00 실행을 요청한 상태다. 콘솔 실행 API 부재로 제작 가이드의 수동 절차를 따른다.
- 저장 LVI Chaos 검사에 기존59다리·나선3트랙·성벽 경사로3트랙·건물 경사로3트랙·4방향 문·전체135훅 접근·패널9트랙과 양끝 접안을 구현했다. 컴파일/Live Coding은 성공했지만 새 저장 조립에서 아직 실행하지 않았다.
- 베이크 후 검사 BastionLowPolyNav에 8slot의 Pod96투영·59다리 및 공동/지상/정상 나선 양방향 Nav를 추가했다. 아직 실행하지 않았고 Nav 단절/머리 공간/모든 훅 첫 착지·관성·자기교차 검사는 더 보완해야 한다.
- 원본 외피 통과만으로 제작 완료나 베이크/실제 충돌 통과를 선언하지 않는다. 다음 단계는 실제 생성 → Chaos 수정 → Bake → 기존 회귀/8slot 검사 → 해시/비용/마커 비교 → 마지막 PIE다.

## 생성 대상 확인

- 사용자 첫 생성 완료 응답 후 로그를 조회하니 실제 실행 대상은 Hollow_00이었다. Bastion LVI 수정시각은 작업 전 상태이며 후보 생성 로그가 없다. 이 상태에서 실행한 BastionLowPolyContent 실패는 이전 Bastion 지형의 진단이며 새 후보 결과로 쓰지 않는다.
- Hollow 재생성으로 달라진10파일을 Saved/SliceOctant/Bastion_00/LowPolyV001/HollowRegenerated/에 복사했고 승인본 HEAD 아카이브 HollowApproved.tar를 보존했다. 복구 여부를 사용자에게 확인한 상태다.
- Bastion_00을 명시한 Lint→Build 재실행을 요청했다. 새 생성 로그 확인 후 실제 Chaos 검사를 다시 실행한다.

## Bastion 저장 후보 첫 검사와 Hollow 복구

- 14:12 Bastion Lint0, Build 성공. 지각227585+구조물50886=278471삼각형(장치/엔진 Cube 제외), 원본 구조물97그룹/에셋 part99, actor198. buildings8/bridges59/islands25/anchors135/panels1/spawns12, authoredErrors0. 이전364525 대비23.61% 감소이며 아직 검증/채택 전 후보 비용이다. 사용하지 않는 이전 Generated 에셋은 폴더에 남아 있어 폴더 전체 합계로 비교하지 않는다.
- 실제 저장 Chaos 첫 검사: 실패141건. 다리 자유 끝 바깥의 바닥 요구, 층계참 끝 경계 좌표, 스텝 없는 직선 캡슐과 실제 접합/훅 접근 문제를 섞지 않고 조사 중이다. 실패를 통과로 처리하지 않는다.
- 검사를 실제 다리 범위(끝1cm 안쪽)로 제한하고,40cm 스텝의 상승/횡단/하강3경로가 모두 비어야 통과하도록 했다. 접합은 별도 선회 경로를 추가해 확인할 예정이다.
- 정상 나선의 기존 개구부 끝까지5도 평탄 접속부를 추가했다. 폐허 층의 램프 끝에서 중앙 바닥으로 돌아서는400cm 폭 바닥 띠를 보존한다. 닫힌 원본과 저장 조립에서 재검사가 필요하다.
- 사용자 Hollow 롤백 지시에 따라 재생성10파일을 별도 백업한 뒤 HEAD 승인본으로 복구했다. 지각 파일 읽기 잠금 때문에 에디터 종료 후 마쳤다. Hollow/Meadow 보존 대상35파일의 SHA256 불일치0. 셸 프로세스 목록은 에디터 연결 상태를 신뢰성 있게 나타내지 못했으며 MCP 읽기 응답이 실제 연결을 확인했다.
- BP 실조회 낙하 감속200, 종단속도2000, 점프속도750(중력2000에서 최고점140.6cm). 패널 양끝은 기존50cm 틈을 고려해100cm 최고점의 짧은 양방향 캡슐 점프를 검사하며 첫 후보에서 통과했다.

## 저장 조립 수정과 첫 베이크

- 저장 Chaos에서 드러난 공중 폐허 입구 가림·짧은 다리 급경사·나선 접속 높이차를 수정했다. 공중 폐허 S04/S08의 다리를 실제 입구 중앙에 맞추고 무너진 벽 토막을 줄였다. D56은 마지막 상승 발판과 정상 나팔을 우회한다. D58/D59는 나선 각도에 따른 폭 방향 높이를 반영한다. 짧은 다리의 수평 착지부는 길이에 맞춰 제한해 중앙 경사를 보존했다.
- 성채 띠 훅의 반경을 H60m:49→52m, H40m:59→62m, H20m:70→73m로 조정했다. 다리 훅은 새 다리 중심/높이를 따른다. D59 훅은 성채 벽 가림을 피해 경로75%에 둔다. 개수135와 개별 사거리 override는 보존한다.
- Source 검사는 전체 Floor 삼각형의 구면 Up 경사와 자기 교차를 추가했다. 0.01cm 용접 후 동일 그룹의 비인접 삼각형 교차를 점/선/공면 다각형까지 검사한다. 공유 정점이 있는 삼각형은 인접으로 제외하므로 그룹 사이의 조립 간섭이나 모든 접합의 완전성을 뜻하지 않는다. 초기 boolean 교차 API의 오검출은 실제 교차 결과 API로 교체했다.
- 실제 Chaos 검사는 저장 LVI의 메시/transform/충돌 설정을 별도 월드에 복제한다. 다리·나선·성벽·건물 경사로를3트랙에서 양방향 캡슐로 검사하며40cm 스텝은 상승/횡단/하강이 모두 비어야 허용한다. 공유 정점의 선 광선 누락에는1cm 구형 sweep만 사용한다. 정상 나선→나팔 출구도3트랙으로 검사한다.
- 훅은 접근/거리/가림·의도한 첫 착지·8방향100cm 착지 여유·실제 BP 이동값을 이용한 관성 낙하를 검사한다. 패널은 실제 BP 크기와 회전 경로9트랙, 양끝 접안 점프를 검사한다. 이는 사용자 조작감 검증을 대신하지 않는다.
- 14:47~14:49 첫 베이크 성공: Support100원본/203Layers/1812489샘플/12852977bytes, Nav150654셀/396components/2547portals/1228584bytes, Traversal68515bytes, Spawn12authored/13032후보/525020bytes. Portal 단계79.019초. 이 결과는 D58 연장 전 후보이므로 최종 비용으로 쓰지 않는다.
- 베이크 후 다리59개·지붕24접속·공동/정상 나선·그래플 전용 섬 분리 등 8slot 검사에서 북쪽 자연 지면→나선만16방향이 실패했다. Pod96투영·머리 공간·편성별 천장 조건, 8slot×16seed의12encounter/125편성 검사는 통과했다. Nav 알고리즘은 변경하지 않고 D58의 지면 쪽 끝 반경33.5→40m로 연장했다.

## 14:59 수정 후보 — 재베이크 대기

- Build 저장 확인: lint0, 구조물56100삼각형, 지각227585, 합283685. 기준364525 대비22.18% 감소, 구조물만136940→56100으로59.03% 감소. 장치/엔진 Cube와 미사용 파일은 비용에서 제외한다.
- 원본97그룹·실제 Chaos 정식2검사: 성공2, 오류/경고0, 1.669초. 바닥14658점·양방향 캡슐28716회·훅135접근/132착지/1056여유점/1161관성낙하·패널450회 통과.
- 사용자에게 수정 후보의 BakeOctant를 요청했다. 다음은 새 베이크의8slot/Nav/Pod 및 회귀 검사, 실제 시각 검토·보존 해시/최종 비용 기록, 마지막 사용자 PIE다.

## 북쪽 Nav 단절 원인과 회귀 검사

- 15:03 재베이크에서도 북쪽 지면→나선의16방향 단절이 남았다. 다른 경로와 Pod 검사는 통과했다. D58 길이 연장만으로 해결했다고 판단하지 않는다.
- 베이크된 지면 노드를100cm 간격으로 조회하니 반경43.8m 바깥의 지면 group0과40.4m 안쪽의 group28 사이가 끊겼다. 같은 구간을 베이커의 반경50/반높이88/바닥 여유1cm 캡슐로 재현해 `A01_Shell` 윗마감이 지각 위 약1.1cm로 돌출한 충돌을 검출했다(좌우 두 트랙). 기존 플레이어 검사의14cm 여유에서는 드러나지 않았다.
- A01의 지각 아래 마감은 심연 절단 경계에서 H0을 유지하고100cm 바깥까지 H−100cm로 내려 묻는다. 지각·이음매 원본과 Nav/베이커 알고리즘은 유지한다. 수정 원본97그룹 외피/경사/자기 교차 통과. 저장본 회귀 검사가 오류2건을 재현한 상태에서 재생성을 요청했다.
- 새 회귀 검사는 북쪽 접근3트랙에서 베이커와 같은 캡슐·충돌 채널·바닥 여유로 양방향 sweep한다. 최종8slot/Nav 통과가 재베이크의 성공 기준이다.

## 최종 저장 후보의 베이크·자동 검사

- 15:08 생성 lint0, 15:11 재베이크 성공. 윗마감 회귀 검사 포함 원본·Chaos2검사 통과 후 베이크했다. Distance Field는 베이크 입력이 아니며 Chaos 삼각형과 필요한 Static Mesh 컴파일 완료를 사용한다.
- patch_15에서 정식6검사 모두 성공, 오류/경고0, 6.325초: `BastionLowPolySource`, `BastionLowPolyContent`, `BastionLowPolyNav`, `BastionEncounters`, `HollowContent`, `ProductionSurfaceDataDefinitions`. 결과는 `Saved/SliceOctant/Bastion_00/LowPolyV001/final-tests.json`에 저장했다.
- 97외피 그룹·전체 Support 경사·비인접 자기 교차, 실제 바닥14658점/양방향 캡슐28716회, 훅135접근/132착지/1056여유점/1161관성낙하, 패널450회와 양끝 접안 통과. 북쪽 Nav용3트랙 회귀 검사는 별도이며 위 플레이어 캡슐 횟수에 합산하지 않았다.
- 베이크 Nav는8slot×양방향1408경로·Pod96투영을 통과했다. 별도 편성 검사는8slot×16seed에서 Pod12개의 편성125마리, 접근 구역·Pod400cm 여유를 확인했다. 실제 NPC 인식·추격·네트워크 합류는 이 검사의 범위가 아니다.
- 기존 함수 교체로 참조가 사라진 `BuildIsland`/`IslandFloorH`만 제거했다. 이전 Generated11파일(B02/B03/B04/B06/B07/B08/S04/S08/W01/W02의 Deco, P01_Core_Shell)은 참조0·기준 복구본과 해시 일치를 확인한 뒤 AssetTools로 삭제했다. `Before/Generated`와 `unused-asset-backups.json`에 복구 근거를 보존했다. bake manifest에 포함된 활성 메시를 삭제한 것은 아니다.
- 저장 마커136개(훅135/패널1), Pod12개를 기준선과 대조했다. 훅34개 위치가 새 경사/우회/성채 착지부에 맞춰 달라졌다. 패널 및 Pod 위치는 모두 동일하다. `final-markers.json`과 `marker-comparison.json`에 전후 transform을 남겼다. 개별 훅 사거리는 실제 저장 마커와 spec을 대조하는 Content 검사로 확인했다.
- Hollow/Meadow35파일 해시 불일치0(`preserved-hashes-after.json`). 작업 전 승인본 복구와 Hollow 회귀 검사 모두 통과했다.

| 동일 집계 지표 | 작업 전 저장본 | 최종 자동 검사 통과 후보 |
|:---|---:|---:|
| 지각 삼각형 | 227585 | 227585 |
| Generated 구조물 삼각형 | 136940 | 56100 |
| 지형 합계 삼각형 | 364525 | 283685 |
| Support 샘플 | 1856557 | 1813344 |
| Support payload(bytes) | 13183313 | 12859082 |
| Nav cells | 150808 | 151140 |
| Nav payload(bytes) | 1232936 | 1232488 |
| Traversal 요소/bytes | 3152 / 57747 | 3660 / 68211 |
| Spawn 요소/bytes | 13084 / 526620 | 13117 / 527940 |

구조물59.03%·전체22.18% 감소다. Support payload는2.46% 줄었지만 Nav는 유지 수준이고 Traversal18.12%·Spawn0.25% 증가했다. 면 수 감소를 전체 메모리/프레임 개선으로 설명하지 않는다. 최종 Support100원본/203Layers, Nav343components/2553portals/1107seams, Spawn12authored/13105random후보다. Portal 후보558397/512285/355377, spacingRejected352824, Portal 단계80.089초. 작업 전 동일 실행의 Portal 시간은 수집하지 않았으므로 전후 속도 개선율은 제시하지 않는다. 실제 최종 header는 `final-header.json`이다.

## 재개 후 확인과 시각 검토

- 한도 초기화 뒤 재접속했을 때 에디터는 TestMap03/PIE off였다. 중단 사이의 전체 빌드 DLL이 적용된 새 에디터에서 자동 검사를 재발견하고 정식6개를 다시 실행해 모두 통과했다(오류/경고0, 6.510초). 결과는 `reopened-tests.json`이다. 기존 입력 코드와 MassSpawnConfig의 ActorMix 선택 변경은 다른 작업의 변경으로 보존했다.
- Bastion LVI를 열어 전체 배치·북쪽 나선 접속·정상 나팔·공동·안뜰 폐허·공중 폐허의 와이어프레임 캡처6장을 검토했다. `Saved/SliceOctant/Bastion_00/LowPolyV001/visual-*.png`에 보존했다. 와이어프레임은 배면까지 겹쳐 보이므로 실제 시야 가림·머티리얼·플레이 감각의 통과 증거로 확대하지 않는다.
- TestMap03과 직전 카메라 위치로 복귀했다. 최종 Hollow/Meadow35파일 재비교도 불일치0이며 `git diff --check`는 오류가 없다. 뷰포트 표시 모드는 사용자 PIE 전 Alt+4로 Lit으로 복귀한다.

## 사용자 PIE 지각 구멍 — 원인 수정·재베이크·자동7개·사용자 외피 재검토 통과

- 사용자가 큰 구멍으로 빠져 외우주로 계속 낙하함을 보고했다. `WhereAmI` 시점은 낙하 중이 아니라 구멍 직전 정지였고10cm 전진하면 빠졌을 위치다. 로그는 slot0/Bastion local(-7587,7978,H98), floor=`SM_Octant_Bastion_00_R30000`이다.
- 원본 Meadow, Bastion 방사 높이 변형 직후, 저장 Bastion 지각에 같은53정점 닫힌 경계가 있다. local bounds X[-8104.196,-7150.371],Y[7484.467,8015.649], 보고점에서 최근접 경계 정점19.3cm다. 범위 수치는 로컬 투영 좌표이며 정확한 플레이어 전진 거리 측정은 아니다.
- 원본 유래는 `4a2e860`의 Meadow 동굴 입구 절단이다. `history/Phase04b_Log.md`의(15°,60°) 동굴 적용은 원본 삼각형을237606→237841로 바꿨다. `LNPCaveKitPlacer.cpp`가 지각 입구를 자르고 키트4액터를 함께 배치했다. Bastion 생성기는 이 콘텐츠 지각만 복제했으며 동굴 키트는 가져오지 않았다. Hollow에는 원본 내부 경계 복구가 이미 있었으나 Bastion에는 적용하지 않았다. 새 심연 cutter·저폴리 구조물이 보고 지점에 구멍을 만든 것은 아니다.
- 기존 `BastionLowPolySource`의97그룹 검사 대상에 지각은 없었다. 저장 Chaos의 지정 경로·훅 착지 표본도 이 구멍 전체를 덮지 않았다. 이전 자동6개 통과와 와이어프레임 대표 시점 검토를 전체 외피의 무결성으로 확대하지 않는다.
- 수정은 `BuildSliceOctant Bastion_00`의 지각 복제 단계에서 기존 경계 정점으로 누락 면을 복원한 뒤 Bastion의 의도된 심연을 자르는 순서다. 별도 땜빵 액터를 만들지 않는다. 원본 Meadow·Hollow 에셋과 이음매는 유지한다. 기존 ear clipping을 그대로 쓰면 복구 면1개의 보행 법선 cosine0.652가 검출돼, Bastion 복구에는 경계 제약 Delaunay를 사용한다. Hollow의 기존 삼각분할 경로는 유지한다.
- 새 `Bake.BastionCrustIntegrity`는 입구 절단 이전 지각 전체의 의도하지 않은 내부 경계, 기존 이음매 정점의 정확한 보존, 복구 면의 방향·보행 경사를 검사한다. 수정 전 내부 경계1개를 재현했고 수정 후0개, 복구51면, source237892tris다. `BastionCrustIntegrity`/`BastionLowPolySource`가0.292초·오류/경고0으로 통과했다. Live Coding patch_3 Success.
- 저장본 Chaos는 원래 구멍과 주변 X[-8200,-7000]/Y[7300,8200]의50cm격자475점에 바닥 존재·법선 검사를 추가했다. 수정 전142점 누락을 재현한 뒤 재생성을 요청했다. 수정 후 결과는 아래 기록을 따른다.
- 초기 저장본 경계 진단은 심연 절단 주변의 T접합/분할 경계도 세므로69성분을 실제 구멍69개로 해석하지 않는다. 정식 지각 경계 검사는 의도된 절단 이전에 수행하며, 저장된 보고 영역은 실제 Chaos로 별도 확인한다. 근거는 `crust-boundaries-before.log`, `crust-integrity-before.json`, `crust-chaos-regression-before.json`, `crust-source-after.json`이다.

- 20:25 KST 사용자 재생성 완료. 로그는 내부 경계1개 복구, 지각227636tris(+51), 구조물56100tris 유지, lint0·저장 성공이다. 지형 합계는283736tris로 변경됐다. `BastionCrustIntegrity`/`BastionLowPolySource`/`BastionLowPolyContent`가1.937초·오류/경고0으로 통과했다. 실제 저장 Chaos의 해당475점 바닥 누락은142→0이며 기존 보행·훅·패널 검사도 통과했다. 근거는 `crust-generated-tests.json`과 에디터 로그다.

- 20:28 KST 재베이크 저장 성공. Support1813344샘플/12858925bytes, Nav151161cells/343components/2553portals/1107seams/1232656bytes, Traversal3660/68211bytes, Spawn12authored+13110random/528140bytes다. Portal79.292초·spacingRejected352824. `crust-baked-header.json`과 `crust-bake-after.log`에 최신 근거를 보존했다. 지각 복구 후 지형 합계283736tris로 기준364525 대비22.16% 감소다. 위 비용 표는 구멍 복구 전 후보의 값이다.
- 재베이크 후 정식7개(`BastionCrustIntegrity`, `BastionLowPolySource`, `BastionLowPolyContent`, `BastionLowPolyNav`, `BastionEncounters`, `HollowContent`, `ProductionSurfaceDataDefinitions`)가6.513초·오류/경고0으로 통과했다. 해당475점 충돌 누락0, 기존 보행/훅/패널,8slot Nav1408왕복경로·Pod96투영과 편성 회귀도 유지된다. 결과는 `crust-baked-tests.json`이다.

2026-10-11 사용자 재검토 결과: “아까 발견했던 구멍이 잘 없어진거 확인했어. 이외에도 맵을 구석구석 돌아다녀봤는데 틈, 구멍은 더 찾지 못했어.” 후속 답변은 “외피의 틈·구멍 위주로 확인”이다. 보고된 구멍 복구와 사용자 탐색 범위의 외피 검토는 통과로 기록한다. 동선·훅·패널·Pod 전투는 미검토이며 아래 묶음으로 요청한다.

재현 위치로 돌아갈 때는 PIE 전 EnemyDensity0, 서버 콘솔 `LNP.Debug.TeleportToReview`로 실제 Bastion 슬롯을 확인하고 `LNP.Debug.TeleportToOctant <Slot> -7587 7978 0`으로 이동한다. 명령은 입력 H에150cm를 더해 스폰한다.

## 마지막 사용자 PIE 묶음 — 외피 통과, 동선·장치·전투 미검토

시각 검토를 마치고 TestMap03으로 돌아왔다. 형상 검토는 에디터 콘솔의 `LNP.Spawn.EnemyDensity 0`을 PIE 시작 전에 설정하면 적을 끌 수 있다. 기본 편성 검토는 PIE를 종료하고 `LNP.Spawn.EnemyDensity 1`로 돌린 뒤 재시작한다. 스폰은 월드 시작 때 한 번이므로 PIE 도중 밀도를 바꾸는 것으로 기존 적이 바뀌지는 않는다.

PIE 서버 콘솔에서 `LNP.Debug.TeleportToReview`로 실제 Bastion 슬롯 목록을 확인한다. 이동은 `LNP.Debug.TeleportToReview <Name> <Slot>`이다. 슬롯 번호는 실행마다 실제 목록에서 선택하며 임의 고정하지 않는다.

| 검토 지점 | 묶어서 확인할 내용 |
|:---|:---|
| NorthGate → Cavern → Dish | 지면 접속·나선3트랙과 정상 출구, 공동 낙하/전투 여유·보행/훅 복귀 |
| OuterRampBase / OuterLanding / OuterMiddle / CitadelTop | 성벽·성채의 경사/층계참/문 왕복, 가장자리와 훅 종료 관성 |
| Courtyard / TallBuilding | 빈 북/남 광장, 폐허 층 이동·지붕 연결·회피/하강 복귀 |
| SkyRuinLow / SkyRuinHigh | 입구/출구 다리·폐허 개구부, 공중 순환의 다음 앵커 식별과 착지 |
| FastAscentBase / FastAscentTop | 남쪽 연속 상승과 정상 우회 다리·하강/중도 해제 복귀 |
| PodTerraceNorth / PodTerraceSouth / PodTerraceCitadel | 전투 공간·천장·엄폐, 실제 Pod/NPC 배치와 경로 합류 |
| CitadelTop ↔ 공중섬 패널 | 패널 중앙/가장자리 탑승·양끝 왕복/접안·훅 복귀, 2인 사용 시 호스트/게스트 |

통과한 사용자 범위만 기록한다. 자동 검사는 실제 Mover 입력·가시성·착지감·전투 합류를 완전히 시뮬레이션하지 않는다. 사용자 PIE 채택 전에는 전체 재제작 완료로 표시하지 않는다.
