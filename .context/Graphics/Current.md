# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: [Phase 1-BR Ridge 지표 Electric Dreams 교체](phases/Phase01_RidgeReferencePatch.md) · 계획 확정(GX-021) · U1 착수 대기
> 마지막 확인/갱신: 2026-11-19(10×10m 시험 생략, 지표 전체 교체로 재정의)

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
  - 그래픽스 영향: GX-018(바위95 → 29개)은 당시 `blocker_rejects` 12.5% 때문이었다. 바위를 다시 늘릴 수 있는지는 재측정할 수 있다(§3 후보).
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

## 3. 다음 세션 후보

**Phase 1-BR을 재정의했다(GX-021, 2026-11-19 사용자).** 10×10m 시험을 생략하고, 경계 전이부를 뺀 Ridge 지표 전체를 Electric Dreams로 교체한다. 기존 1-B6은 여기에 흡수했다.

교체 방식은 다음과 같다.

- 지각은 경계 고리와 기반면 한 장으로 만든다.
- 절벽·능선·메사는 ED 메시로 세운다.
- 충돌은 배치에서 파생한 `_Floor`·`_Shell` 프록시가 맡는다.
- 동굴·부유섬·석교는 V011 형상을 유지하고 다음 패스에서 바꾼다.

단위는 U1~U8이고 Z1 파일럿에서 U6 PIE 게이트를 먼저 통과시킨다. 구현 단위마다 다음 진행을 확인하고, 기존 미저장 사용자 변경은 임의로 버리거나 저장하지 않는다.

1. **착수 전 정리.** 이전 세션의 미커밋 Graphics 문서와 `BuildRidgeDecorationPCG.py`·`.gitignore` 변경을 커밋할지 사용자에게 확인한다.
2. **[U1 설계도 추출](phases/Phase01_RidgeReferencePatch.md) §4.** `ExportRidgeBlueprint.py`로 `RidgeBlueprint_V001.json`을 만든다. 새 WIP `Graphics/EDTerrain_V001/`에 V011 고스트와 보호 형상을 깐다.
3. **U2 에셋 카탈로그·반입.** 원본 ED 에디터에서 역할별 후보를 읽기 전용으로 조사한다. Migrate는 사용자가 하며, 두 에디터를 동시에 띄우지 않는다.
4. 선택: 바위 밀도 재검토. GX-018 축소의 근거였던 `blocker_rejects`가 SSN 수정(D-087~D-095)으로 거의 0이 됐다. 바위를 GX-018 이전(95개) 쪽으로 되돌려 1인 PIE에서 `LNP.SurfaceNav.EnemyGround.Report`와 추격 체감을 다시 잴 수 있다. 하지 않으면 GX-018 유지.
큰 바위 엄폐 지형 교체 후보(실행서 §28 표)는 1-BR U2·U4에서 함께 다룬다.

미검증 범위는 2인 호스트·게스트, 적 시야·사격 엄폐 대칭, 다른 slot/극점의 바위 접촉, 전체 밀도·게임 조명·패키지 GPU/물리 비용이다. 성능 판정은 패키지에서만 한다. 에디터 빌드·쿡 전에는 병행 작업을 확인한다. 화면 녹화는 별도 사용자 확인이 필요하다.

## 4. 근거와 복구 기록

공통 상위는 `Saved/Profiling/Graphics/Phase01/`이고 Saved는 로컬 자료다. 추적되는 재현 절차/판정 원본은 실행서 §7~33이다.

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

> `.context/Graphics/README.md`, `.context/Graphics/Current.md`, `.context/Graphics/Decisions.md`, `.context/Graphics/phases/Phase01_RidgeReferencePatch.md`를 읽고 Phase 1-BR의 U1(설계도 추출)부터 시작하자. 구현 단위마다 다음 진행을 확인하고, 수동 조작이 더 효율적이면 즉시 요청해줘.
