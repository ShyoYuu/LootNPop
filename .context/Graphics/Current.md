# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: [Phase 1-BR Ridge 지표 Electric Dreams 교체](phases/Phase01_RidgeReferencePatch.md) · U4 Z1 서쪽 산괴(GX-023) · 기준 시험본 V017(사면 정리·하상 골·섬 지지면 지각) · 다음은 조명·구역 밖 재질
> 마지막 확인/갱신: 2026-11-20

## 1. 이어받을 결과

- **1-A 출발 기준선 완료.** Ridge slot1·확정 A/B/C, Medium3회 Frame 중앙값19.687ms. 재측정 출발점은 [보고서](analysis/Phase01_StartBaselineReport.md)이며 Meadow를 포함한 과거 Phase0 수치와 섞지 않는다.
- **WIP 지각 V005·수동 장식12개·보정 Fern·WIP 전용 SurfaceData/검사용 풀은 유지된다.** 재질·회전·런타임 접촉 경과는 [실행서](phases/Phase01_EnvironmentImport.md) §18~22다.
- **1-B5a 마감.** 커스텀 C++ PCG 노드 `LNPDecorationSampler`와 규칙 에셋 `LNPDecorationRuleData`로 구면 구역 지터 격자·실제 지각 trace·메시 전체 상자 보호 배치를 한다. §23.
- **1-B5b 마감(GX-014).** Moss·Sandstone·Cliff 바위는 Static+Blocker(`LNPStaticBlocker`)이고 Sandstone은 단순 컨벡스 파생 메시를 쓴다. 수동 바위5개도 승격했다. §24.
- **1-B5c 마감.** 간격만 줄여서는 구석에만 늘고 A/B/C 구도는 밀도 판정에 맞지 않았다. 사용자 결정으로 GX-015와 같은 계열 변형 먼저를 정했다. §25.
- **1-B5d 마감(GX-015).** 역할별 부호 있는 2D 여유·동선 지피 허용·간격(Clover280/바위450·500/Fern500)·경사(Clover·Fern 0~40°)·그래프 V004(Fern > 바위 > 지피). 시드51001 결과 Clover222·Sandstone12·Moss6·Fern5 = 245. WIP 저장·재베이크(Nav 셀−9·component+1)·1인 PIE 접촉/추격 정상. 바위 하나 주변 큰 Nav 구멍으로 지상 적이 평지를 크게 우회하는 어색함은 **바위를 옮기지 않고 SSN 후보(경로 다듬기)로만 기록**했다 → SSN에서 해결(아래 "SSN 작은 Blocker 우회 결과"). §26.
- **1-B5e 마감(GX-016~018).** 변형18개 반입(덮어쓴47개는 첫 반입본과 같음)·근경 통과 뒤 역할21개로 늘렸다. 보호 형상이 평지의94%를 막던 원인(Pod 편성 구·지형 깎는 동선 폭)을 시각화해 동선 반폭50%·Pod 중심9m로 줄였고(GX-017), 그래프 V005(큰 바위 > Fern > 작은 바위 > 지피)로 큰 바위를 살렸다(GX-016). 바위95개에서 적 이동이 부자연스러워(blocker_rejects 12.5%) 바위 여유 +300/+450·간격2배로 줄였다(GX-018). 채택: Clover303·작은 바위27·큰 바위2·Fern52 = 384, blocker_rejects 2.6%(1-B5d 0.9%). 작은 바위를 가끔 못 돌아가는 문제는 SSN에서 해결했다(아래 "SSN 작은 Blocker 우회 결과"). 원본의 더 큰 바위 후보(2~30m)는 §28 표(미반입). §27~29.
- **1-B5f-3 저장 완료.** V006에서 바위 29개의 배치를 재현하고 새 7종을 `NoCollision`으로 추가했다. 화면에 표시하는 Fern은 서로 겹치고 동선 안에 배치된다. 사용자가 확률 80%를 승인했다. 신규 311개·Clover 275개·바위 29개·Fern 846개(동선 안 331개), 총 1,461개다. §30~32.
- **1-B5f 1인 PIE 마감.** 실제 Ridge slot 1의 ISM 26개·인스턴스 1,461개가 저장값과 일치했다. 사용자는 잔돌·Fern 주변 보행과 시야에서 어색한 부분이 없다고 판정했다. PIE 종료 뒤 원래 풀·Ridge WIP·카메라를 복원했고 대조 파일 8개 해시는 동일했다. §33.
- **SSN 작은 Blocker 우회 결과(2026-11-18, 1-B5d·1-B5e 인계 2건 해결).** SSN [Phase16_SmallBlockerDetour](../SurfaceSupportNavigation/phases/Phase16_SmallBlockerDetour.md) 단위1~6(D-087~D-095)에서 처리했다. 바위·WIP 배치는 바꾸지 않았다.
  - 작은 바위에 붙어 섬(1-B5e): 캐시 접지가 바위로 거부되면 ±45°·±90° 옆걸음(D-087). 직선 보행·waypoint 건너뛰기·경로 단순화가 실제 발→목표 선분이 지나는 삼각형을 모두 확인한다(D-090·D-093). 단위4 최종 PIE에서 `blocker_rejects` 41/390,416(약 0.01%, 1-B5e 채택 시 2.6%).
  - 평지를 크게 우회(1-B5d): 경로 추종이 Nav 구멍을 실제 보행 검증으로 가로지른다(D-088). 바위 하나 주변의 큰 Nav 구멍은 바위가 아니라 지형 요철이 원인이었고, 베이커 clearance 바닥 허용 10cm(D-089, schema11)로 메웠다.
  - Nav 밖으로 걸어 들어간 플레이어(바위 주머니)는 발자취로 추격하고, 점프·바위를 넘어야 들어가는 자리는 도달 불가로 Alert 뒤 Pod로 돌아간다(D-091·D-092·D-094).
  - 남은 한계: 높이 약 30cm 가파른 턱 때문에 슬롯 위치에서 공격 거리에 못 드는 근접 적은 선다. 내려가기만 되는 턱으로 밀려 들어간 적은 못 나온다. 지형 쪽 원인은 베이크·지형 작업 범위다(SSN §8.7).
  - 그래픽스 영향: GX-018(바위95 → 29개)은 당시 `blocker_rejects` 12.5% 때문이었다. 바위 밀도 재측정은 1-BR U7의 선택 항목이다.
- **알려진 한계(SSN 인계).** 지상형 적은 `MaxStepUp`45cm 아래 장애물을 턱으로 넘고 Support(지각)로 접지해 **발이 바위를 관통한다.** 벽보행 적은 바위에 잘 붙는다. 사용자가 별도 SSN 세션에서 다룬다. 권장: "Support 없는 Blocker는 넘지 않는다" 불변 조건 → 1-B6 전 ISM Support(D-058 해제). 원인 위치·순서는 §24.
- **SSN Phase16 종료(2026-11-16).** 단위2(벽 옆 플레이어 근접 추격, D-085)까지 완료했다. ISM Support는 조사 후 **보류**했다(D-058 유지). Sandstone Support에는 ISM instance별 source와 볼록 충돌 Support가 모두 필요하고(볼록 hit에는 FaceIndex가 없음), Nav는 어차피 바위를 돌아가 이득이 작다. 바위는 Static+Blocker 그대로 두고 **1-BR(구 1-B6)은 ISM Support를 기다리지 않는다.** 1-BR에서 바위가 늘면 지상 적이 바위 앞에서 멈추는 빈도(`blocker_rejects`)와 Nav 단절로 갇히는 곳만 확인한다. 상세 SSN [Phase16 §4.1](../SurfaceSupportNavigation/phases/Phase16_SupportlessBlockerStep.md).
- **SSN 인계 결과(2026-11-16, 당시 단위1 완료·단위2 대기).** SSN [Phase16](../SurfaceSupportNavigation/phases/Phase16_SupportlessBlockerStep.md)에서 캐시 접지의 Support 없는 Blocker 겹침을 거부했다(D-082). 벽 근처 NoNode 정지는 Atlas NeedsExact 띠가 Nav에서 빠진 탓이었고 Nav 전용 보간(D-084, 베이커 schema10)으로 고쳤다. 실제 Ridge WIP PIE에서 정지가 재현되지 않았고 바위 관통 유지도 없었다. **WIP `DA_OctantSurface_LVI_Ridge_SurfaceProbe`는 schema10으로 재베이크됐다**(원본 백업 `Saved/Profiling/Phase16/NavGap/Backup/`). 정식7·fixture5 SurfaceData도 재베이크됐다. 남은 것은 벽 옆 플레이어 근접 추격·공격(단위2)과 ISM Support다. 그래픽스에서 WIP를 다시 구울 때는 commandlet이면 `load_map` 뒤에 굽는다.
- **엔진/도구 함정:** PCG `UseSeed()` 기본 false(시드42 고정), PCGVolume 브러시 QueryOnly, Python 선택자 `set_attribute_name`이 ref를 안 바꿈(`import_text`, 점 속성 확인은 `export_text()`), 디스크립터 BodyInstance 프로필 이름만 바꾸면 NoCollision, DrawNav는 Ridge 분지에서 반지름3000 이상 필요. Migrate 덮어쓰기 확인은 "모두 아니요"(이번엔 기존 반입본이 저장된 적 없어 무해했다).
- **Phase1 전체는 미완료다.** 전체 밀도·원거리 반복·정식 Ridge 적용·파라곤·콘텐츠 패키지 성능은 남았다. 원본104패키지·정식 Ridge/Support source·기본 편성·정식 풀·확정 카메라 경로는 보존했다.

## 2. 1-B5f 기준 상태와 재현

| 항목 | 마지막 확인 상태 |
|:---|:---|
| 에디터 | LootNPop8000, **Ridge WIP**, PIE=false, WIP dirty=false. |
| WIP | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe` 저장됨. 메시15(수동 바위5 Blocker) + PCG 액터 `GraphicsDecorationPCG_V001`(그래프 **V006**·시드51001·GenerateOnDemand·1,461개·1,045,509B) |
| PCG·파생 에셋 | `/Game/Graphics/Environment/ElectricDreams/RidgeDecoration_V001/`의 `DA_RidgeDecorationRules_V006`·`DA_RidgeDecorationTallVegetationRules_V006`·`PCG_RidgeDecoration_V006`·`SM_Ridge_SandstoneBoulder_01_Blocker`. V005 및 해당 기본 규칙도 복구용으로 남겨뒀다. |
| SurfaceData | WIP 전용 `DA_OctantSurface_LVI_Ridge_SurfaceProbe`. 1-B5e 재베이크(5,242,301B, schema10, Nav 셀75,870·component70·portal424) 뒤 **SSN 단위3이 schema11로 재베이크**(D-089, Nav 셀76,466·component48·portal528, SSN Phase16_SmallBlockerDetour §4.3). WIP를 다시 구우면 schema11 베이커다 |
| 풀 선택 | 원래 `/Game/Maps/OctantPoolData`로 복원했다(`SelectionState.json`). INI·정식 풀 해시는 동일하다. |
| 사용자 CVar | `LNP.Spawn.EnemyDensity=1`(에디터 재시작 기본값) |

기존 빌더와 PCG 스크립트는 **이미 산출물이 있으므로 재실행하지 않는다.** 새 체크아웃은 §22까지 재현하고 §30의 7종을 반입한 뒤 에디터 빌드 → WIP 열기 → `BuildRidgeDecorationPCG.py blocker-mesh` → `rules` → `graph`(V006) → `promote` → 수동 바위 확인·WIP 저장 → `place` → 육안 확인·WIP 저장(`save_current_level`) → TestMap03 재베이크다. 결과 검사는 읽기 전용 `InspectRidgeDecorationPCG.py`다. 실행은 에디터 상태 표시줄 Cmd에 `py "<경로>" <인자>`를 넣는다. 규칙 수치만 바꿀 때는 `rules` 뒤 `regenerate`로 다시 생성한다.

WIP를 정상 게임에서 다시 검토하려면 TestMap03에서 검사용 풀을 **메모리에서만 임시 선택**한다(실행서 §22). 매 실행의 실제 WIP slot을 로그에서 다시 찾고, 종료 후 원래 풀을 복원한다. 원본/파생 에셋·WIP·SurfaceData/풀·Saved는 Git 제외다. **World `duplicate_asset` 직후 `load_level` 크래시를 재시도하지 않는다.**

## 3. Phase 1-BR 진행 상태와 다음 작업

**Phase 1-BR을 재정의했다(GX-021, 2026-11-19 사용자).** 10×10m 시험을 생략하고, 경계 전이부를 뺀 Ridge 지표 전체를 Electric Dreams로 교체한다. 기존 1-B6은 여기에 흡수했다.

교체 방식은 다음과 같다.

- 지각은 경계 고리와 기반면 한 장으로 만든다.
- 절벽·능선·메사는 ED 메시로 세운다.
- 충돌은 배치에서 파생한 `_Floor`·`_Shell` 프록시가 맡는다.
- 동굴·부유섬·석교는 V011 형상을 유지하고 다음 패스에서 바꾼다.

단위는 U1~U8이고 Z1 파일럿에서 U6 PIE 게이트를 먼저 통과시킨다. 구현 단위마다 다음 진행을 확인하고, 기존 미저장 사용자 변경은 임의로 버리거나 저장하지 않는다.

- **U1 완료.** `RidgeBlueprint_V001.json`의 Pod 12개·훅 6개·검토점 27개를 정식 V011과 대조했다. 기본 WIP의 고스트 3개와 `DrawRidgeDecorationKeepOuts.py draw-br`로 설계도를 표시할 수 있다. [검증 기록](history/Phase01_Log.md#2026-11-19--1-br-u1-설계도-추출)
- **U2 완료.** 원본 후보 55개에서 메시 17개를 선정하고 새 파일 96개(3.91GiB)를 반입했다. 기존 43개는 해시가 같다. LootNPop에서 메시 17개의 치수·fallback·재질·충돌, 의존성 139개와 하상 혼합 재질을 검증했다. [검증 기록](history/Phase01_Log.md#2026-11-19--1-br-u2-지형-에셋-선정반입)
- **U3 완료.** 기본 WIP의 V012 기반면·보존 구조물·재투영 마커 45개가 Blender 검증을 통과했다. UE 최대 seam 스냅은 0.00763cm이고, 조기 베이크 seam 반지름 차이는 0.000cm다. 근거는 `Saved/SliceOctant/Ridge_00/V012/`에 있다.
- **U4 Z1 서쪽 절벽 검토 중.** V001의 15개는 에셋 용도와 개별 배치를 보는 초벌이었다. 사용자는 Z1에서 보이는 절벽 대부분을 ED 암벽이 이루고, 메우기 어려운 이음에만 기반면이 조금 드러나는 수준을 요구했다. V002에서 서쪽 벽과 왕관을 ED 8개로 연결하고, 각 단면에 맞춰 구면 기반면을 벽 뒤로 물렸다. `PillarApron`과 `NorthLedge`는 곡면의 뒷면이 노출되던 배치를 제외했다. `WestUpperShelf`도 새 왕관 조립으로 대체했다.
- **서쪽 절벽 표본 확인.** 정면 표본 1,394개에서 ED 메시가 첫 교차점인 비율이 V001 527개(37.8%) → V002 1,350개(96.8%)였다. Blender의 조형 지각과 UE fallback 삼각형을 사용한 가시 면 표본이다. `WestWallCenter`는 하천 둑 바닥으로 쓰인 MassiveSandstoneCliff 03에서 원본의 절벽 외피인 SmartAssets Cliff 06으로 교체했다. 교체 뒤 피복은 1,350개로 같았다. 중앙벽과 양옆 벽의 반복, 왕관 접합부, 인접 사면의 피복은 다음 조형 작업에 남아 있다.

| 작업본 | 경로와 상태 |
|:---|:---|
| U1~U3 기준 및 이전 Z1 초벌 | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/LVI_Ridge_EDTerrain_V001` · V012·마커 45개·초벌 메시 13개. 조기 베이크는 초벌 배치 전의 U3 기반면을 검증한 결과다. |
| Z1 배치 초벌 | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1Composition_V001/LVI_Ridge_EDTerrain_Z1Composition_V001` · 기반면·하상 재질 `MI_Ridge_Z1_Channel`·시각 메시 15개. |
| ED 조립체 시험(GX-022 판정 완료) | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1AssemblyTest_V001/Z1AssemblyTest_V001` · TestMap03 조명 6개 + V002·`PL_LargeCliff_01b` Level Instance. 재생성 `BuildRidgeZ1AssemblyTest.py build`→`instances`. `wall <버전>`은 이 레벨이 열린 상태에서 실행한다. 버전별 시험 레벨 `Z1AssemblyWall_V001~V011`. 근거 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/` |
| 서쪽 절벽 비교본 | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1Composition_V002/LVI_Ridge_EDTerrain_Z1Composition_V002` · `V012_Z1WestCover` 조형 기반면·시각 메시 16개. `MI_Ridge_Z1_Channel`을 공유한다. 아래 조립체 시험 레벨들의 원본이다. |
| ED CloseRange 구운 레벨 | `/Game/LNPBake/CloseRange/CR_{LargeAssembly,SmallAssembly,Ground,Ditch,Props}` · ED에서 구워 Migrate. 원점은 ED LargeAssembly 위치 `(90250, -4360, 1660)` |
| 산괴 고원 지각 | `EDTerrain_V001/Z1MassifCover_V002/` · 분지 기준 +46m 고원(x −2,200 서쪽). V001은 벽 뒤 봉우리가 남아 기각 |
| 평면 그룹 드레이프 | `EDTerrain_V001/Z1MassifDrape_V002/Z1MassifDrape_V002` · Ground·SmallAssembly·Props·Ditch를 인스턴스마다 구면에 다시 앉힌 월드 좌표 레벨(V001은 Ditch 제외) |
| 산괴 지각 재질 | `EDTerrain_V001/Z1MassifSurface_V001/MI_Ridge_Z1_MassifSurface` · 하상 재질(V005 절벽·지면 + 하상) 사본에 구역 마스크를 서쪽 고원까지 넓힘. 시험 레벨 전용(넓힌 범위에 이음매 제외 없음) |
| 사면 정리 드레이프 | `EDTerrain_V001/Z1MassifDrape_V003`(납작한 타일·데칼을 지각 법선으로 눕히고 45° 초과 제외, 35,350개)·`Z1MassifDrape_V004`(+모든 인스턴스 60° 초과 제외, 31,734개). 시험 레벨은 V011에서 드레이프만 바꾼 `Z1AssemblyWall_V012`·`V013` |
| 이전 기준 시험본 | `EDTerrain_V001/Z1AssemblyWall_V011/Z1AssemblyWall_V011` · TestMap03 조명 + V002 사본(서쪽 벽 제외, 지각은 Z1MassifCover V002) + 02c·02e·02d 벽 + CR LargeAssembly 강체 + Drape V002 + 산괴 지각 재질. 사용자 판정 "엄청 맘에 든다"(Unlit 부감) |
| 이전 기준 시험본 | `EDTerrain_V001/Z1AssemblyWall_V013/Z1AssemblyWall_V013` · V011에서 드레이프만 Drape V004로 바꿨다(사면 선반 정리). 사용자 채택(2026-11-20) |
| 하상 골 지각 | `EDTerrain_V001/Z1MassifCover_V003` · V002 고원에 CloseRange 하상 띠를 50cm 판 렌더링용 지각. 드레이프 배치 높이는 V002 그대로 |
| 이전 기준 시험본 | `EDTerrain_V001/Z1AssemblyWall_V014/Z1AssemblyWall_V014` · V013에서 지각 메시만 Z1MassifCover V003으로 바꿨다(하상 고리 노출). 사용자 채택(2026-11-20) |
| 섬 지지면 지각 | `EDTerrain_V001/Z1MassifCover_V004`·`V005` · ED 섬 지지면 높이장(`Scripts/Profiling/Graphics/CloseRangeSupport_V001.json`·`V002.json`)을 따라 둑 고리 +3m 단을 받친 렌더링 지각. V005는 테두리 밖을 지지면 +10cm로 1m 넓혀 둥근 타일 가장자리를 묻는다. 하상 띠 −50cm 포함, 드레이프 배치 높이는 V002 그대로. 비교본 `Z1AssemblyWall_V016`(V004 지각)과 기각된 Drape V005·`V015`(가장자리 꺼짐 타일 제외)도 남아 있다 |
| **현재 기준 시험본** | `EDTerrain_V001/Z1AssemblyWall_V017/Z1AssemblyWall_V017` · V014에서 지각 메시만 Z1MassifCover V005로 바꿨다(테두리 허공·버섯 갓 정리). 사용자 채택(2026-11-20) |

**Z1 서쪽 산괴 경과(2026-11-19~20, GX-022 → GX-023).** 조립체를 이어 벽을 만드는 시험(01b·01c 기둥 4개, 02 계열 벽 3개, 배경 조립체 축소)은 덩치가 모자라거나 나무까지 작아져 어색했다. ED 이음 방식을 조사해 보니 ED는 큰 형상을 랜드스케이프·바닥 메시로 만들고 조립체는 외피, 이음은 식생·잔재가 맡는다. 구면 Ridge에 가장 가까운 선례인 CloseRange(랜드스케이프 없음)의 PCG 결과를 ED에서 구워 반입했다. 산괴는 원래 크기 강체로 두고 기반면을 +46m 고원으로 깎았다. 곡률(d²/2R) 때문에 묻히던 평면 바닥 그룹은 인스턴스마다 구면에 드레이프했다. 마지막으로 지각 재질 구역을 넓혀 보라색 평지색을 사암 절벽·지면 재질로 바꿨다(V011). 사용자가 이 방향을 확정했다(GX-023). 버전별 결과는 [Phase 1 로그](history/Phase01_Log.md)의 "ED 조립체 이음 방식 조사"~"산괴 고원 지각 재질" 절에 있다. 이어서(2026-11-20) 사면 선반 타일(V013)·하상 자리(V014)·Lit 판정·고원 동쪽 테두리(V017)를 정리했다. 드레이프 배치 높이는 평탄 고원으로 두고 화면 지각만 ED 섬 지지면을 따르게 분리했다(GX-024). 경과는 같은 로그의 "사면 선반 타일 정리"~"고원 동쪽 테두리" 절이다.

**재생성 순서(새 체크아웃).** 모두 LootNPop 에디터 Cmd의 `py "<프로젝트>/Scripts/Profiling/Graphics/BuildRidgeZ1AssemblyTest.py" <인자>`다. ED 쪽 굽기만 ElectricDreamsEnv 에디터다.

1. ED 에디터: `BakeEDCloseRange.py bake` → `verify` → 사용자가 `CR_*` 5개를 LootNPop으로 Migrate("모두 아니요").
2. Blender: `BuildRidgeZ1Composition.py -- massif-cover`(spec `MASSIF_COVER="V002"`) → UE `importcrust V002`(드레이프 배치 높이), `-- massif-island-v005` → `importcrust V005`(기준 렌더링 지각, 비교본은 `-- massif-channel` → V003, `-- massif-island` → V004)이고, 지지면 높이장은 ED 트레이스·CR 덤프(로컬)에서 `python Scripts/Profiling/Graphics/BuildCloseRangeSupport.py <V001|V002>`로 다시 만든다.
3. UE: `drape V004` → `surface` → V002 기준 시험 레벨(`Z1AssemblyTest_V001`)을 연 뒤 `wall V017`. 비교본 V011~V013은 `drape V002`·`V003`·`V004` 뒤 `wall V011`·`V012`·`V013`, 지각 비교본 V014·V016은 `wall V014`·`V016`, 기각된 지각 재질 비교본 V018은 `surface V002` 뒤 `wall V018`이다.
4. 화면: `look 1500 -1500 9000 -6500 300 -2500`(부감) → 콘솔 `HighResShot 1600x900`. `AutomationLibrary.take_high_res_screenshot`은 쓰지 않는다.

**남은 문제(다음 세션 순서).**

- ~~사면 선반 타일~~ **완료(2026-11-20, V013 채택).** 원인은 70~85° 사면에 평지 배치를 현지 Up으로 세운 것이었다. Drape V004는 납작한 지면 타일·데칼을 지각 법선으로 눕히고 45° 넘으면 빼며, 60° 넘는 자리는 종류와 관계없이 뺀다. 남북 사면이 지각 절벽 재질로 드러난다. 능선 꼭대기 판 몇 개와 고원 동쪽 가장자리 지면 타일 테두리는 남았다. 경과는 [Phase 1 로그](history/Phase01_Log.md)의 "사면 선반 타일 정리"다.
- ~~하상 자리 맨 지면~~ **완료(2026-11-20, V014 채택).** 원인은 묻힘이 아니라 겹침이었다. ED 하상 바닥은 원점 높이라 드레이프 뒤 얇은 바닥 메시가 지각과 같은 높이에서 덮였다. 렌더링 지각에만 하상 띠(반폭 4.5m·깊이 50cm)를 파서 하상 고리를 드러냈다. 경과는 [Phase 1 로그](history/Phase01_Log.md)의 "하상 자리 지각 골"이다.

- **Lit 판정 완료(2026-11-20).** 산괴·식생·하상은 Lit에서도 잘 읽히고, 하상 바닥 틈으로 판 지각이 드러난 곳은 없었다. 회색 데칼은 Lit에서 보이지 않았고, 작은 흰 사각형은 에디터 데칼 아이콘이다. 경과는 [Phase 1 로그](history/Phase01_Log.md)의 "V014 Lit 판정"이다.

- ~~고원 동쪽 가장자리 타일 테두리~~ **완료(2026-11-20, V017 채택).** 원인은 ED 섬의 하상 바깥 둑 고리 전체가 원점 +3m 단인데 그 아래 지각이 비어 있던 것이다. 렌더링 지각이 ED 섬 지지면 50cm 아래를 따르고, 테두리 밖은 지지면 +10cm로 1m 넓힌 뒤 45°로 내린다. 남은 점: 지각이 바깥 타일 띠를 1m 안팎 덮고 경계 일부에 50cm 격자 톱니가 있다. 테두리 양 끝·산괴 왼쪽 위에 타일 없는 지각 면이 있다. 경과는 [Phase 1 로그](history/Phase01_Log.md)의 "고원 동쪽 테두리"다.

- ~~CloseRange에 더 어울리는 지각 재질~~ **완료(2026-11-20, 기각·V001 유지).** CR 지면 메시 재질은 메시 고유 UV 스캔 아틀라스라 지각에 반복 투영할 수 없다. 반복 가능한 표면 12종 중 가장 가까운 DriedGrassOnDampSoil로 지면층만 바꾼 `Z1MassifSurface_V002`(`Z1AssemblyWall_V018`)는 사용자가 기각했다. 무늬가 늪지대 같고 V017의 MossyRockyGround가 더 어울린다고 봤다. 경과는 [Phase 1 로그](history/Phase01_Log.md)의 "지면층 교체 시험"이다.

1. **조명·구역 밖 재질.** TestMap03 조명(구 중심 점광원)에서는 분지 정면의 02 벽 정면이 대부분 그늘이다. 오른쪽(북동)에는 재질 구역 밖 V012 지각의 보라 평지색이 크게 보인다. 수직 부감의 진흙 웅덩이는 하늘 없이 어둡게 반사된다.
2. 그 뒤 Z1 정면 구도(분지 저시점) 확인 → 고원 남북 끝 절개 사면 → 배치 재현 원본(JSON) 확정 → U5 충돌 프록시·U6 베이크·러프 PIE. 고원·드레이프는 지금 모두 NoCollision 시각 시험이다.

조사용 덤프 스크립트와 결과 JSON은 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/`에 있다(로컬). 에디터 Python은 모듈을 캐시하므로 시험 스크립트는 spec을 `importlib.reload`한다. ED 맵 첫 로드 시 물 재질 레이 트레이싱 셰이더 컴파일 오류가 로그에 남는다(영향 미확인).

LootNPop은 8000, ElectricDreamsEnv는 8001이며 두 에디터는 번갈아 사용한다. 연결이 도구 목록에 없으면 사용자에게 알리고 MCP 연결을 확인한다. 수동 조작은 사용자가 직접 하도록 요청한다.

미검증 범위는 2인 호스트·게스트, 적 시야·사격 엄폐 대칭, 다른 slot/극점의 바위 접촉, 전체 밀도·게임 조명·패키지 GPU/물리 비용이다. 성능 판정은 패키지에서만 한다. 에디터 빌드·쿡 전에는 병행 작업을 확인한다. 화면 녹화는 별도 사용자 확인이 필요하다.

## 4. 근거와 복구 기록

1-BR 자료는 `Saved/SliceOctant/Ridge_00/V012/`(Blender·반입·조기 베이크), `Saved/SliceOctant/Ridge_00/V012_Z1Composition/`(원본 용도 대조, `BeforeSourceRoles.umap`, `WestCoverageProbe_CenterWall.txt`), `Saved/SliceOctant/Ridge_00/V012_Z1WestCover/`(V002 형상·반입·조립 검증), `Saved/Profiling/Graphics/Phase01/EDTerrain_V001/`(에셋 반입 전후)에 있다. 아래 1-B5f 이전 자료의 공통 상위는 `Saved/Profiling/Graphics/Phase01/`이다. Saved는 로컬 자료이며 재생성 절차는 실행서에 있다.

| 자료 | 핵심 기록 |
|:---|:---|
| `RidgeDecorationNewFamilies_20261118/` | 1-B5f-3 V005 대조·Fern 밀도 전후 `Inspection_V006*.json`·`Verification.json`·`SaveWip.json`, 1인 PIE `PIEInstances.json`·`PIERun.log`·`SelectionState.json=restored` |
| `RidgeSurfaceTriangle_20261116/`·`RidgeSurfaceOrientation_20261116/` | V005 생성·중립 노멀·회전/경계 시험과 복원 |
| `RidgeRuntimeValidation_20261116/` | WIP 전용 첫 베이크·검사용 풀·1-B4e PIE 접촉·`Views/pie_game_shot.png` |
| `RidgeDecorationPCG_20261116/` | 1-B5a `Rules.json`·`BeforePCG.umap`·`Inspection_Seed51001.json`·`ManualVerdict.json` |
| `RidgeVariantImport_20261117/`·`RidgeVariantCloseup_20261117/` | 1-B5e-1 반입 전후 목록·덮어쓰기 해시 대조·`LoadInspection.json`, 1-B5e-2 `Placement.json`·`Clear.json` |
| `RidgeDecorationVariants_20261117/` | 1-B5e-3 `BeforeVariants*`(규칙·WIP·SurfaceData 복구 사본)·`Rules.json`·`Inspection_*`·`SaveWip.json`·`SelectionState.json=restored`(PIE 결과 포함)·`Hashes_prePIE.txt` |
| `RidgeDecorationExpand_20261116/` | 1-B5d `BeforeExpand*`(규칙·WIP·SurfaceData 복구 사본)·`Rules*.json`·`Inspection_V003*/V004*`·`SaveWip.json`·`SelectionState.json=restored`·`Views/` |
| `RidgeDecorationDensity_20261116/` | 1-B5c `Preflight.json`·`Variant_*`·`Inspection_*`·`Restore.json`·`Hashes_restored.json`·`Views/Compare_spacing_*` |
| `RidgeRockBlocker_20261116/` | 1-B5b `BeforeBlocker.umap`·`Promotion.json`·`Inspection_V002.json`·`CliffCollisionFit.*`·`BeforeRebake_SurfaceData.uasset`·`Rebake.json`·`SelectionState.json=restored`·`PieVerdict.json` |

UE5.8 scalar setter는 반환 false만으로 실패를 판정하지 말고 getter로 확인한다. Python 프로퍼티 이름은 bool의 `b` 접두어를 빼고 쓴다(`terrain_blocker`). PIE 게임창이 분리돼 있으면 native CaptureViewport의 검은 화면을 근거로 쓰지 않고 게임 콘솔 `Shot`으로 확인한다. PCG 재생성 직후 같은 프레임의 검사는 정리 대기 컴포넌트를 읽으므로 한 번 더 실행한다.

## 5. 새 세션 요청 예시

> `.context/Graphics/README.md`, `Current.md`, `Decisions.md`(GX-022~GX-024), `phases/Phase01_RidgeReferencePatch.md` U4를 읽고 Phase 1-BR의 Z1 서쪽 산괴를 이어가자. 기준 시험본은 `Z1AssemblyWall_V017`이다. Current §3 "남은 문제" 1번(조명·구역 밖 재질)부터 해줘. 지각 재질은 V001(MossyRockyGround)을 유지한다. 새 버전을 V017과 같은 Lit 부감에서 비교하고, 결과를 보여준 뒤 다음 단위로 넘어가기 전에 확인해줘. 수동 조작(Migrate·저장 등)이 필요하면 나에게 요청해줘.
