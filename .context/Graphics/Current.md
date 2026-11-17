# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 1-B · 1-B5e 같은 계열 변형 마감(384개·재베이크·1인 PIE) · 다음은 1-B5f 새 계열·충돌 없는 식생 확대
> 마지막 확인/갱신: 2026-11-17(1-B5e-3 저장·재베이크·PIE·풀 복원·해시 확인)

## 1. 이어받을 결과

- **1-A 출발 기준선 완료.** Ridge slot1·확정 A/B/C, Medium3회 Frame 중앙값19.687ms. 재측정 출발점은 [보고서](analysis/Phase01_StartBaselineReport.md)이며 Meadow를 포함한 과거 Phase0 수치와 섞지 않는다.
- **WIP 지각 V005·수동 장식12개·보정 Fern·WIP 전용 SurfaceData/검사용 풀은 유지된다.** 재질·회전·런타임 접촉 경과는 [실행서](phases/Phase01_EnvironmentImport.md) §18~22다.
- **1-B5a 마감.** 커스텀 C++ PCG 노드 `LNPDecorationSampler`와 규칙 에셋 `LNPDecorationRuleData`로 구면 구역 지터 격자·실제 지각 trace·메시 전체 상자 보호 배치를 한다. §23.
- **1-B5b 마감(GX-014).** Moss·Sandstone·Cliff 바위는 Static+Blocker(`LNPStaticBlocker`)이고 Sandstone은 단순 컨벡스 파생 메시를 쓴다. 수동 바위5개도 승격했다. §24.
- **1-B5c 마감.** 간격만 줄여서는 구석에만 늘고 A/B/C 구도는 밀도 판정에 맞지 않았다. 사용자 결정으로 GX-015와 같은 계열 변형 먼저를 정했다. §25.
- **1-B5d 마감(GX-015).** 역할별 부호 있는 2D 여유·동선 지피 허용·간격(Clover280/바위450·500/Fern500)·경사(Clover·Fern 0~40°)·그래프 V004(Fern > 바위 > 지피). 시드51001 결과 Clover222·Sandstone12·Moss6·Fern5 = 245. WIP 저장·재베이크(Nav 셀−9·component+1)·1인 PIE 접촉/추격 정상. 바위 하나 주변 큰 Nav 구멍으로 지상 적이 평지를 크게 우회하는 어색함은 **바위를 옮기지 않고 SSN 후보(경로 다듬기)로만 기록**했다. §26.
- **1-B5e 마감(GX-016~018).** 변형18개 반입(덮어쓴47개는 첫 반입본과 같음)·근경 통과 뒤 역할21개로 늘렸다. 보호 형상이 평지의94%를 막던 원인(Pod 편성 구·지형 깎는 동선 폭)을 시각화해 동선 반폭50%·Pod 중심9m로 줄였고(GX-017), 그래프 V005(큰 바위 > Fern > 작은 바위 > 지피)로 큰 바위를 살렸다(GX-016). 바위95개에서 적 이동이 부자연스러워(blocker_rejects 12.5%) 바위 여유 +300/+450·간격2배로 줄였다(GX-018). 채택: Clover303·작은 바위27·큰 바위2·Fern52 = 384, blocker_rejects 2.6%(1-B5d 0.9%). 작은 바위를 가끔 못 돌아가는 문제는 사용자가 SSN 세션에서 본다. 원본의 더 큰 바위 후보(2~30m)는 §28 표(미반입). §27~29.
- **알려진 한계(SSN 인계).** 지상형 적은 `MaxStepUp`45cm 아래 장애물을 턱으로 넘고 Support(지각)로 접지해 **발이 바위를 관통한다.** 벽보행 적은 바위에 잘 붙는다. 사용자가 별도 SSN 세션에서 다룬다. 권장: "Support 없는 Blocker는 넘지 않는다" 불변 조건 → 1-B6 전 ISM Support(D-058 해제). 원인 위치·순서는 §24.
- **SSN Phase16 종료(2026-11-16).** 단위2(벽 옆 플레이어 근접 추격, D-085)까지 완료했다. ISM Support는 조사 후 **보류**했다(D-058 유지). Sandstone Support에는 ISM instance별 source와 볼록 충돌 Support가 모두 필요하고(볼록 hit에는 FaceIndex가 없음), Nav는 어차피 바위를 돌아가 이득이 작다. 바위는 Static+Blocker 그대로 두고 **1-B6은 ISM Support를 기다리지 않는다.** 1-B6에서 바위가 늘면 지상 적이 바위 앞에서 멈추는 빈도(`blocker_rejects`)와 Nav 단절로 갇히는 곳만 확인한다. 상세 SSN [Phase16 §4.1](../SurfaceSupportNavigation/phases/Phase16_SupportlessBlockerStep.md).
- **SSN 인계 결과(2026-11-16, 당시 단위1 완료·단위2 대기).** SSN [Phase16](../SurfaceSupportNavigation/phases/Phase16_SupportlessBlockerStep.md)에서 캐시 접지의 Support 없는 Blocker 겹침을 거부했다(D-082). 벽 근처 NoNode 정지는 Atlas NeedsExact 띠가 Nav에서 빠진 탓이었고 Nav 전용 보간(D-084, 베이커 schema10)으로 고쳤다. 실제 Ridge WIP PIE에서 정지가 재현되지 않았고 바위 관통 유지도 없었다. **WIP `DA_OctantSurface_LVI_Ridge_SurfaceProbe`는 schema10으로 재베이크됐다**(원본 백업 `Saved/Profiling/Phase16/NavGap/Backup/`). 정식7·fixture5 SurfaceData도 재베이크됐다. 남은 것은 벽 옆 플레이어 근접 추격·공격(단위2)과 ISM Support다. 그래픽스에서 WIP를 다시 구울 때는 commandlet이면 `load_map` 뒤에 굽는다.
- **엔진/도구 함정:** PCG `UseSeed()` 기본 false(시드42 고정), PCGVolume 브러시 QueryOnly, Python 선택자 `set_attribute_name`이 ref를 안 바꿈(`import_text`, 점 속성 확인은 `export_text()`), 디스크립터 BodyInstance 프로필 이름만 바꾸면 NoCollision, DrawNav는 Ridge 분지에서 반지름3000 이상 필요. Migrate 덮어쓰기 확인은 "모두 아니요"(이번엔 기존 반입본이 저장된 적 없어 무해했다).
- **Phase1 전체는 미완료다.** 전체 밀도·원거리 반복·정식 Ridge 적용·파라곤·콘텐츠 패키지 성능은 남았다. 원본104패키지·정식 Ridge/Support source·기본 편성·정식 풀·확정 카메라 경로는 보존했다.

## 2. 마지막 로컬 상태와 재현

| 항목 | 마지막 확인 상태 |
|:---|:---|
| 에디터 | LootNPop8000, **TestMap03**, PIE=false, 미저장 변경 없음. C++ 변경 없음(1-B5d 빌드 그대로) |
| WIP | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe` 저장됨. 메시15(수동 바위5 Blocker) + PCG 액터 `GraphicsDecorationPCG_V001`(그래프 **V005**·시드51001·GenerateOnDemand, 역할21개·384개, 449,622B) |
| PCG·파생 에셋 | `/Game/Graphics/Environment/ElectricDreams/RidgeDecoration_V001/`의 `DA_RidgeDecorationRules`(1-B5e, GX-016~018)·`PCG_RidgeDecoration_V005`(V001~V004는 복구용, 되돌릴 땐 규칙 복구 사본 `BeforeVariants_*`도 함께)·`SM_Ridge_SandstoneBoulder_01_Blocker`(Git 제외) |
| SurfaceData | WIP 전용 `DA_OctantSurface_LVI_Ridge_SurfaceProbe` 1-B5e 재베이크(5,242,301B, schema10, Nav 셀75,870·component70·portal424) |
| 풀 선택 | **원래 `/Game/Maps/OctantPoolData`로 복원됨.** SaveConfig 미호출, INI·정식 풀 해시 불변 |
| 사용자 CVar | `LNP.Spawn.EnemyDensity=1`(에디터 재시작 기본값) |

기존 빌더와 PCG 스크립트는 **이미 산출물이 있으므로 재실행하지 않는다.** 새 체크아웃은 §22까지 재현한 뒤 에디터 빌드 → WIP 열기 → `BuildRidgeDecorationPCG.py blocker-mesh`→`rules`→`graph`(V005)→`place`→`promote` → 육안 확인 뒤 WIP 저장(`save_current_level`) → TestMap03 재베이크다. 결과 검사는 읽기 전용 `InspectRidgeDecorationPCG.py`다. 실행은 에디터 상태 표시줄 Cmd에 `py "<경로>" <인자>`를 넣는다. 규칙 수치만 바꿀 때는 `rules` 뒤 `regenerate`로 다시 생성한다.

WIP를 정상 게임에서 다시 검토하려면 TestMap03에서 검사용 풀을 **메모리에서만 임시 선택**한다(실행서 §22). 매 실행의 실제 WIP slot을 로그에서 다시 찾고, 종료 후 원래 풀을 복원한다. 원본/파생 에셋·WIP·SurfaceData/풀·Saved는 Git 제외다. **World `duplicate_asset` 직후 `load_level` 크래시를 재시도하지 않는다.**

## 3. 다음 세션 후보

**다음 단위는 사용자가 고른다.** 구현 단위마다 다음 진행을 확인하고, 기존 미저장 사용자 변경을 임의로 버리거나 저장하지 않는다.

1. **1-B5f 새 계열·충돌 없는 식생 확대(실행서 §26 후보·§29).** 사암 톤 새 계열(잔돌 `SmallStonesPack`·낙엽 `DeadLeaves`·마른 가지 `DryBranches`/`WoodenSticksAndTwigs`·풀 `KikuyuGrass`/`GroundCover`·꽃1~2종)을 사용자가 8001에서 Migrate(덮어쓰기 "모두 아니요")한 뒤 반입 대조·근경. 함께 Fern끼리 겹침 허용(그래프 겹침 정리 완화)·Fern 동선 안쪽 허용을 해서 동선 근처를 충돌 없는 식생으로 채운다. 바위 규칙(GX-018)은 유지한다.
2. 선택: 큰 바위 엄폐 지형 교체(§28 표의 SmartAssets Cliff·사암 Ridge/Mesa 반입, 지정 위치). 사용자가 더 키울 수도 있다고 했다.
3. 낮은 우선순위: Electric Dreams 수준 우거진 식생 메모리 전용 비교(변형·새 계열 뒤).
4. **1-B6 전체 확장**은 그 뒤다. SSN 바위 접지(D-082)는 끝났고 ISM Support는 보류라 기다리지 않는다.

미검증 범위는 2인 호스트·게스트, 적 시야·사격 엄폐 대칭, 다른 slot/극점의 바위 접촉, 전체 밀도·게임 조명·패키지 GPU/물리 비용이다. 성능 판정은 패키지에서만 한다. 에디터 빌드·쿡 전에는 병행 작업을 확인한다. 화면 녹화는 별도 사용자 확인이 필요하다.

## 4. 근거와 복구 기록

공통 상위는 `Saved/Profiling/Graphics/Phase01/`이고 Saved는 로컬 자료다. 추적되는 재현 절차/판정 원본은 실행서 §7~29다.

| 자료 | 핵심 기록 |
|:---|:---|
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

> `.context/Graphics/README.md`, `Current.md`, `Decisions.md`를 읽고 1-B5f 새 계열·충돌 없는 식생 확대(실행서 §29)를 시작하자. 1-B5e는 마감했어. 8001에서 Migrate할 목록부터 알려줘. 에디터 빌드 전에는 꼭 물어보고, 구현 단위마다 다음 진행을 확인하고, 수동 조작이 더 효율적이면 즉시 요청해줘.
