# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 1-B · 1-B5a 대표 구역 PCG 소수 배치 마감 · 1-B5b 바위 충돌 승격 계획 초안(미승인·미착수)
> 마지막 확인/갱신: 2026-11-16(재베이크·1인 PIE·풀 복원·종료 상태 확인)

## 1. 이어받을 결과

- **1-A 출발 기준선 완료.** Ridge slot1·확정 A/B/C, Medium3회 Frame 중앙값19.687ms. 재측정 출발점은 [보고서](analysis/Phase01_StartBaselineReport.md)이며 Meadow를 포함한 과거 Phase0 수치와 섞지 않는다.
- **WIP 지각 V005·소수 장식12개·보정 Fern·WIP 전용 SurfaceData/검사용 풀은 유지된다.** 재질·회전·런타임 접촉 경과는 [실행서](phases/Phase01_EnvironmentImport.md) §18~22다.
- **1-B5a 마감.** 커스텀 C++ PCG 노드 `LNPDecorationSampler`와 규칙 에셋 `LNPDecorationRuleData`를 만들었다. 구면 구역 지터 격자·실제 지각 trace를 쓰고, 회전·스케일·WPO 여유를 포함한 메시 전체 상자로 동선·Pod·훅·문·암반·카메라·이음매를 보호한다. 시드51001 대표 구역 배치는 **Clover15·Sandstone2·Moss2·Fern1 = 20개**(Decoration/NoCollision ISM)다. 사용자 근경 판정 통과 뒤 WIP를 저장했다. WIP 재베이크 수치가 §22와 모두 같았고, 1인 PIE(slot2 회전, EnemyDensity0)에서 보행·착지·카메라·사격 이상 없음을 확인했다. §23.
- **엔진 함정 2개:** PCG 설정 `UseSeed()` 기본 false면 시드가 42로 고정된다(노드에 override). PCGVolume 브러시 기본 충돌은 QueryOnly다(`place`가 끈다).
- **1-B5b 계획 초안.** 사용자는 바위를 발·사격이 관통하는 것이 어색하다고 지적했다. GX-013은 1차 장식 범위라 NoCollision이 최종 확정은 아니다. 적은 SurfaceData를 쓰고 Decoration은 NoCollision이 계약이므로, 단순 충돌 켜기가 아니라 **높이 있는 바위 역할만 지형 계약 소스로 승격 + 재베이크**하는 방향이다. 1-B6 확장 전에 한다. §24.
- **Phase1 전체는 미완료다.** 전체 밀도·원거리 반복·정식 Ridge 적용·파라곤·콘텐츠 패키지 성능은 남았다. 원본104패키지·정식 Ridge/Support source·기본 편성·정식 풀·확정 카메라 경로는 보존했다.

## 2. 마지막 로컬 상태와 재현

| 항목 | 마지막 확인 상태 |
|:---|:---|
| 에디터 | LootNPop8000, **TestMap03**, PIE=false, dirty 0. 새 PCG 클래스를 포함한 에디터 빌드 완료 |
| WIP | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe` 저장됨. 기존 메시15 + PCG 액터 `GraphicsDecorationPCG_V001`(시드51001·GenerateOnDemand·볼륨 NoCollision) |
| PCG 에셋 | `/Game/Graphics/Environment/ElectricDreams/RidgeDecoration_V001/`의 `DA_RidgeDecorationRules`·`PCG_RidgeDecoration`(Git 제외) |
| SurfaceData | WIP 전용 `DA_OctantSurface_LVI_Ridge_SurfaceProbe`를 PCG 배치 뒤 재베이크. 수치는 이전과 동일 |
| 풀 선택 | **원래 `/Game/Maps/OctantPoolData`로 복원됨.** SaveConfig 미호출, INI·정식 풀 해시 불변 |
| 사용자 CVar | `LNP.Spawn.EnemyDensity=0`(이번 세션 사용자 설정). 에디터를 재시작하면 1로 돌아간다 |

기존 빌더와 이번 PCG 스크립트는 **이미 산출물이 있으므로 재실행하지 않는다.** 새 체크아웃은 §22까지 재현한 뒤 에디터 빌드 → WIP 열기 → `BuildRidgeDecorationPCG.py rules`→`graph`→`place` → 육안 확인 뒤 WIP 저장 → TestMap03 재베이크 순서다. 결과 검사는 읽기 전용 `InspectRidgeDecorationPCG.py`다. 실행은 에디터 상태 표시줄 Cmd에 `py "<경로>" <인자>`를 넣는다. 규칙 수치만 바꿀 때는 `rules`를 다시 돌리고 PCG 컴포넌트를 재생성한다(그래프·C++ 재빌드 불필요).

WIP를 정상 게임에서 다시 검토하려면 TestMap03에서 검사용 풀을 **메모리에서만 임시 선택**한다(실행서 §22). 매 실행의 실제 WIP slot을 로그에서 다시 찾고, 종료 후 원래 풀을 복원한다. 원본/파생 에셋·WIP·SurfaceData/풀·Saved는 Git 제외다. **World `duplicate_asset` 직후 `load_level` 크래시를 재시도하지 않는다.**

## 3. 다음 세션 후보

**다음 단위는 사용자가 고른다.** 구현 단위마다 다음 진행을 확인하고, 기존 미저장 사용자 변경을 임의로 버리거나 저장하지 않는다.

1. **1-B5b 바위 충돌 승격(권장 선행).** 착수 전에 다음을 사용자와 정한다: 승격 대상·높이 기준, Support 여부, 충돌 형상 출처, 새 GX 문구, SSN Phase 15 재측정 순서. 그 뒤 PCG 역할 플래그·계약별 Spawner·재베이크·발/탄환/적/카메라 검증을 한다. 계획은 [실행서](phases/Phase01_EnvironmentImport.md) §24.
2. **대표 구역 밀도·원거리 확인.** 같은 규칙으로 시드·간격을 바꿔 벤치마크 A/B/C 구도에서 원거리 반복과 밀도를 본다. 밀도 승인은 사용자 판정이다.
3. **1-B6 전체 확장**은 1·2 뒤에 한다.

미검증 범위는 PCG 결과의 독립 재검산, 전체8 slot/극점·바람 기준축 전환, 2인 호스트·게스트/적 전투, 전체 밀도·게임 조명·패키지 GPU 시간이다. 성능 판정은 패키지에서만 한다. 에디터 빌드·쿡 전에는 병행 작업을 확인한다. 화면 녹화는 별도 사용자 확인이 필요하다.

## 4. 근거와 복구 기록

공통 상위는 `Saved/Profiling/Graphics/Phase01/`이고 Saved는 로컬 자료다. 추적되는 재현 절차/판정 원본은 실행서 §7~24다.

| 자료 | 핵심 기록 |
|:---|:---|
| `RidgeSurfaceTriangle_20261116/`·`RidgeSurfaceOrientation_20261116/` | V005 생성·중립 노멀·회전/경계 시험과 복원 |
| `RidgeRuntimeValidation_20261116/` | WIP 전용 첫 베이크·검사용 풀·1-B4e PIE 접촉·`Views/pie_game_shot.png` |
| `RidgeDecorationPCG_20261116/` | `Rules.json`·`BeforePCG.umap`(PCG 전 WIP)·`Inspection_Seed42/51001.json`·`BeforeRebake_SurfaceData.uasset`·`SelectionState.json=restored`·`ManualVerdict.json`·`FinalLiveCheck.json` |

UE5.8 scalar setter는 반환 false만으로 실패를 판정하지 말고 getter로 확인한다. Python 프로퍼티 이름은 bool의 `b` 접두어를 빼고 쓴다(`closed_polygon`). PIE 게임창이 분리돼 있으면 native CaptureViewport의 검은 화면을 근거로 쓰지 않고 게임 콘솔 `Shot`으로 확인한다.

## 5. 새 세션 요청 예시

> `.context/Graphics/README.md`, `Current.md`, `Decisions.md`를 읽고 1-B5b 바위 충돌 승격을 시작하자. 1-B5a(PCG 소수 배치20개·재베이크 동일·1인 PIE)는 마감했고 원래 풀도 복원했어. 착수 전에 승격 대상·Support 여부·GX 문구·SSN Phase 15 순서를 먼저 같이 정하고, 구현 단위마다 다음 진행을 확인해줘. 기존 빌더와 PCG 스크립트는 재실행하지 말고 수동 조작이 더 효율적이면 즉시 요청해줘.
