# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 1-B · 1-B5b 바위 Blocker 승격 마감 · 다음은 대표 구역 밀도·원거리 확인 또는 1-B6 준비(사용자 선택)
> 마지막 확인/갱신: 2026-11-16(재베이크·1인 PIE 2회·풀 복원·종료 상태 확인)

## 1. 이어받을 결과

- **1-A 출발 기준선 완료.** Ridge slot1·확정 A/B/C, Medium3회 Frame 중앙값19.687ms. 재측정 출발점은 [보고서](analysis/Phase01_StartBaselineReport.md)이며 Meadow를 포함한 과거 Phase0 수치와 섞지 않는다.
- **WIP 지각 V005·수동 장식12개·보정 Fern·WIP 전용 SurfaceData/검사용 풀은 유지된다.** 재질·회전·런타임 접촉 경과는 [실행서](phases/Phase01_EnvironmentImport.md) §18~22다.
- **1-B5a 마감.** 커스텀 C++ PCG 노드 `LNPDecorationSampler`와 규칙 에셋 `LNPDecorationRuleData`로 구면 구역 지터 격자·실제 지각 trace·메시 전체 상자 보호 배치를 한다. §23.
- **1-B5b 마감(GX-014).** Moss·Sandstone·Cliff 바위를 Static+Blocker(`LNPStaticBlocker`)로 승격했다. PCG는 그래프 V002(필터 → Blocker/Decoration Spawner)이고 막는 역할은 2D 동선 여유 +50cm다. 시드51001 결과는 Clover15·Moss2·Fern1(1-B5a와 같은 위치)·Sandstone1이다. 수동 바위5개도 승격했고 Sandstone은 단순 컨벡스 파생 메시를 쓴다. 재베이크는 Nav 셀 −1 외 같고, 1인 PIE에서 접촉·막힘·착탄·카메라와 DrawNav 바위 주변 차단을 확인했다. §24.
- **알려진 한계(SSN 인계).** 지상형 적은 `MaxStepUp`45cm 아래 장애물을 턱으로 넘고 Support(지각)로 접지해 **발이 바위를 관통한다.** 벽보행 적은 바위에 잘 붙는다. 사용자가 별도 SSN 세션에서 다룬다. 권장: "Support 없는 Blocker는 넘지 않는다" 불변 조건 → 1-B6 전 ISM Support(D-058 해제). 원인 위치·순서는 §24.
- **SSN 인계 결과(2026-11-16, 단위1 완료·단위2 대기).** SSN [Phase16](../SurfaceSupportNavigation/phases/Phase16_SupportlessBlockerStep.md)에서 캐시 접지의 Support 없는 Blocker 겹침을 거부했다(D-082). 벽 근처 NoNode 정지는 Atlas NeedsExact 띠가 Nav에서 빠진 탓이었고 Nav 전용 보간(D-084, 베이커 schema10)으로 고쳤다. 실제 Ridge WIP PIE에서 정지가 재현되지 않았고 바위 관통 유지도 없었다. **WIP `DA_OctantSurface_LVI_Ridge_SurfaceProbe`는 schema10으로 재베이크됐다**(원본 백업 `Saved/Profiling/Phase16/NavGap/Backup/`). 정식7·fixture5 SurfaceData도 재베이크됐다. 남은 것은 벽 옆 플레이어 근접 추격·공격(단위2)과 ISM Support다. 그래픽스에서 WIP를 다시 구울 때는 commandlet이면 `load_map` 뒤에 굽는다.
- **엔진/도구 함정:** PCG `UseSeed()` 기본 false(시드42 고정), PCGVolume 브러시 QueryOnly, Python 선택자 `set_attribute_name`이 ref를 안 바꿈(`import_text`), 디스크립터 BodyInstance 프로필 이름만 바꾸면 NoCollision, DrawNav는 Ridge 분지에서 반지름3000 이상 필요.
- **Phase1 전체는 미완료다.** 전체 밀도·원거리 반복·정식 Ridge 적용·파라곤·콘텐츠 패키지 성능은 남았다. 원본104패키지·정식 Ridge/Support source·기본 편성·정식 풀·확정 카메라 경로는 보존했다.

## 2. 마지막 로컬 상태와 재현

| 항목 | 마지막 확인 상태 |
|:---|:---|
| 에디터 | LootNPop8000, **TestMap03**, PIE=false. 1-B5b C++ 포함 에디터 빌드 완료 |
| WIP | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe` 저장됨. 메시15(수동 바위5 Blocker) + PCG 액터 `GraphicsDecorationPCG_V001`(그래프 V002·시드51001·GenerateOnDemand) |
| PCG·파생 에셋 | `/Game/Graphics/Environment/ElectricDreams/RidgeDecoration_V001/`의 `DA_RidgeDecorationRules`·`PCG_RidgeDecoration_V002`(V001 그래프는 복구용)·`SM_Ridge_SandstoneBoulder_01_Blocker`(Git 제외) |
| SurfaceData | WIP 전용 `DA_OctantSurface_LVI_Ridge_SurfaceProbe` 1-B5b 재베이크(5,219,831B) |
| 풀 선택 | **원래 `/Game/Maps/OctantPoolData`로 복원됨.** SaveConfig 미호출, INI·정식 풀 해시 불변 |
| 사용자 CVar | `LNP.Spawn.EnemyDensity=1`(마지막 PIE 때 사용자 설정) |

기존 빌더와 PCG 스크립트는 **이미 산출물이 있으므로 재실행하지 않는다.** 새 체크아웃은 §22까지 재현한 뒤 에디터 빌드 → WIP 열기 → `BuildRidgeDecorationPCG.py blocker-mesh`→`rules`→`graph`→`place`→`promote` → 육안 확인 뒤 WIP 저장 → TestMap03 재베이크다. 결과 검사는 읽기 전용 `InspectRidgeDecorationPCG.py`다. 실행은 에디터 상태 표시줄 Cmd에 `py "<경로>" <인자>`를 넣는다. 규칙 수치만 바꿀 때는 `rules` 뒤 `regenerate`로 다시 생성한다.

WIP를 정상 게임에서 다시 검토하려면 TestMap03에서 검사용 풀을 **메모리에서만 임시 선택**한다(실행서 §22). 매 실행의 실제 WIP slot을 로그에서 다시 찾고, 종료 후 원래 풀을 복원한다. 원본/파생 에셋·WIP·SurfaceData/풀·Saved는 Git 제외다. **World `duplicate_asset` 직후 `load_level` 크래시를 재시도하지 않는다.**

## 3. 다음 세션 후보

**다음 단위는 사용자가 고른다.** 구현 단위마다 다음 진행을 확인하고, 기존 미저장 사용자 변경을 임의로 버리거나 저장하지 않는다.

1. **대표 구역 밀도·원거리 확인.** 같은 규칙으로 시드·간격을 바꿔 벤치마크 A/B/C 구도에서 원거리 반복과 밀도를 본다. 밀도 승인은 사용자 판정이다. 바위가 늘면 지상 적 발 관통도 늘어나므로 SSN 세션 진행 상황을 먼저 확인한다.
2. **1-B6 전체 확장**은 1과 SSN 바위 접지(최소한 Blocker 넘기 금지) 뒤에 한다.

미검증 범위는 2인 호스트·게스트, 적 시야·사격 엄폐 대칭, 다른 slot/극점의 바위 접촉, 전체 밀도·게임 조명·패키지 GPU/물리 비용이다. 성능 판정은 패키지에서만 한다. 에디터 빌드·쿡 전에는 병행 작업을 확인한다. 화면 녹화는 별도 사용자 확인이 필요하다.

## 4. 근거와 복구 기록

공통 상위는 `Saved/Profiling/Graphics/Phase01/`이고 Saved는 로컬 자료다. 추적되는 재현 절차/판정 원본은 실행서 §7~24다.

| 자료 | 핵심 기록 |
|:---|:---|
| `RidgeSurfaceTriangle_20261116/`·`RidgeSurfaceOrientation_20261116/` | V005 생성·중립 노멀·회전/경계 시험과 복원 |
| `RidgeRuntimeValidation_20261116/` | WIP 전용 첫 베이크·검사용 풀·1-B4e PIE 접촉·`Views/pie_game_shot.png` |
| `RidgeDecorationPCG_20261116/` | 1-B5a `Rules.json`·`BeforePCG.umap`·`Inspection_Seed51001.json`·`ManualVerdict.json` |
| `RidgeRockBlocker_20261116/` | 1-B5b `BeforeBlocker.umap`·`Promotion.json`·`Inspection_V002.json`·`CliffCollisionFit.*`·`BeforeRebake_SurfaceData.uasset`·`Rebake.json`·`SelectionState.json=restored`·`PieVerdict.json` |

UE5.8 scalar setter는 반환 false만으로 실패를 판정하지 말고 getter로 확인한다. Python 프로퍼티 이름은 bool의 `b` 접두어를 빼고 쓴다(`terrain_blocker`). PIE 게임창이 분리돼 있으면 native CaptureViewport의 검은 화면을 근거로 쓰지 않고 게임 콘솔 `Shot`으로 확인한다. PCG 재생성 직후 같은 프레임의 검사는 정리 대기 컴포넌트를 읽으므로 한 번 더 실행한다.

## 5. 새 세션 요청 예시

> `.context/Graphics/README.md`, `Current.md`, `Decisions.md`를 읽고 대표 구역 밀도·원거리 확인을 시작하자. 1-B5b(바위 Static+Blocker 승격·재베이크·1인 PIE)는 마감했고 원래 풀도 복원했어. 지상 적 바위 발 관통은 SSN 세션에서 다루는 중이야. 구현 단위마다 다음 진행을 확인하고, 기존 빌더와 PCG 스크립트는 재실행하지 말고 수동 조작이 더 효율적이면 즉시 요청해줘.
