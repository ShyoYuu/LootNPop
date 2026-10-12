# Phase 9 cook·Development 패키지 검증 V007

> 상태: cook 최신성 차단·Development 패키징 통과, 패키지 1P는 Nav graph 생성 오류로 실패
> 날짜: 2026-10-12
> 범위: 단위5의 cook·패키징과 패키지 로드 스모크. 런타임 수정은 다음 구현 단위 진행 확인 대기

후속 [V008](Phase09_NavPrecisionV008.md)에서 FP 원인을 확정·최소 수정하고 실제 패키지 snapshot 게시까지 확인했다. 아래는 V007 당시 실패 근거로 보존하며, 최신 잔여 실패와 다음 행동은 V008을 따른다.

## 입력과 실행

- V006의 정식 `DA_MassSpawnConfig`와 Meadow·Bastion·Hollow 풀을 유지했다. 코드·에셋·기본 설정은 변경하지 않았다. V006의 고유125검사 결과를 전체 회귀 근거로 유지하고, 이번에는 cook 전 production 최신성·결정론을 별도로 재실행했다.
- 에디터가 닫힌 상태에서 `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE`가 성공했다(최신 상태, 1.66초). UAT의 Editor·Game Development 빌드도 성공했다(63.57초).
- 로컬 실행 근거는 `Saved/SliceOctant/CookPackageV007`이다. `cook-after-gate.ps1`은 JSON 보고서에서 production 최신성 검사의 성공·오류0과 실패/미실행/진행 중0을 확인한 뒤에만 UAT를 호출한다. **이번 실행의 선행 차단 절차이며 plain BuildCookRun 자체에 validator를 추가한 것은 아니다.**

## cook 최신성 양성·음성 검사

- `Bake.ProductionPoolCurrentAndDeterministic`: 성공1/실패0, 268.343초, 테스트 오류·경고0. 세 production source 각각의 두 차례 베이크와 저장 source hash·Support/Navigation/Traversal/Spawn/ConditionalPatch 스트림의 일치를 확인했다. `FreshnessReport/index.json`·`freshness.log`에 보존했다.
- 음성 검사는 별도 프로세스의 `-ini:Game:[/Script/LootNPop.LNPSettings]:SphereRadius=30001.0`만 적용했다. 저장 에셋과 `DefaultGame.ini`는 바꾸지 않았다. `Bake.OctantBakeDeterministic`가 Crust/Regression fixture와 Meadow의 저장 source hash·Support 불일치6개를 검출했다(실패1, 27.675초).
- 음성 프로세스는 **exit0**이었으나 JSON은 실패였다. 로컬 실행기가 실패 보고서를 거부하고 `GatePassed=false/UATInvoked=false`를 기록했다. 종료 코드만으로 패키징 진행 여부를 결정하면 안 된다는 실제 근거다. `StaleNegativeReport/index.json`·`cook-decision.json`·`stale-negative.log`를 보존했다. 음성 실행 전후 `DefaultGame.ini` SHA256도 일치했다.

## Development BuildCookRun

- Win64 Development `-build -cook -stage -pak -archive -unattended` 성공, exit0, 실행기 측정136.925초(UAT 내부135.50초). 1,229 packages cook 완료.
- 맵은 `/Game/Maps/TestMap03`와 `/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression`, 추가 CookDir은 `Content/Maps/SurfaceNavigation/Fixtures`다. 기존 패키징 규약을 유지했다.
- `Saved/Cooked/Windows/LootNPop/Metadata/ReferencedSet.txt`에서 production 세 LVI·세 SurfaceData·정식 `DA_MassSpawnConfig` 포함을 확인했다. pak/IoStore 산출물이 생성됐다.
- 출력: `Saved/SurfaceNavigationPhase9V007Package/Windows`. 실제 게임 exe SHA256은 `F6DDAB4C294AA8D6F608F1F9116DF5618796406B781A52AF14A7F95E9B05F743`이다.
- 기존 Lyra Mannequin의 Missing Material Function·invalid ShaderMap cook 경고가 재현됐다. 경고0 패키지로 기록하지 않는다. 명령 인자·종료 코드·시간은 `package-exit.json`, 전체 출력은 `package-stdout.log`다.

## 패키지 1P 실패

- 실제 패키지 exe로 `TestMap03 -nullrhi -nosound -corelimit=4 -unattended -FixedSeed -LNPLoadBaselineProduction -LNPLoadBaselineSeed=1 -LNPLoadBaselinePlayers=1 -LNPLoadBaselineQuit`를 실행했다. Support cache·exact lateral sweep·parallel movement는1, 추가 관통 진단은0이다. `LNPLoadBaselineSeed`를 월드의 `OctantGenSeed`로 해석하지 않는다.
- slot0/3 Meadow, slot1/5/7 Bastion, slot2/4/6 Hollow가 모두 로드됐다. HitIdentity 등록716 sources, 미분류 exact blocker0이다.
- **`LNPSurfaceDataSubsystem: Nav slot 1 graph: Nav Layer 1 node (326,270) has no Support.`** 오류로 초기 snapshot 게시가 중단됐다. `Published generation`과 face/source/SurfaceData probe 성공 로그가 없다. 패키지 1P는 실패이며 2P·late join·성능·메모리 Gate로 진행하지 않았다.
- 초기화가 중단돼 harness 자동 종료까지 도달하지 못했다. 이번 실행의 소유 프로세스(PID7768, 정확한 패키지 exe 경로 일치)를 종료했다. `package-1p-exit.json`의 exit−1은 이 수동 종료 결과이며 crash 종료로 해석하지 않는다. 종료 전 로그에는 assert·ensure·fatal/unhandled exception 표식이 없었다.
- 시작 시 기존 Lyra ShaderMap 오류6개와 GameplayCue 경로 경고도 있다. 기능 차단 원인으로 확인한 것은 위 Nav graph 오류이며 머티리얼 경고를 함께 수정하지 않았다. 원본은 `package-1p.log`다.

## 원인 후보와 다음 단위

`LNPNavGraph::BuildAssetGraph`는 저장 Nav 좌표의 정규화 방향을 `LNPSupportAtlas::QueryLayer`로 다시 조회한다. `ComputeCell`은 방향을 격자로 되돌린 뒤 `floor`와 삼각형 분기로 세 꼭짓점을 고른다. 세 꼭짓점 중 Invalid/NeedsExact가 하나라도 있으면 보간을 거부한다.

실제 컴파일 응답 파일에서 Editor는 `/fp:precise`, Game Development는 `/fp:fast`다(`Intermediate/Build/Win64/x64/{UnrealEditor,UnrealGame}/Development/LootNPop/LootNPop.Shared.rsp`). 같은 저장본이 Editor의 production 검증에서는 통과하고 cooked Game에서 위 노드에 실패했으므로 **경계에서의 부동소수점 판정 차이가 유력한 후보**다. 아직 해당 노드의 양쪽 격자 좌표·세 꼭짓점/flags를 비교하지 않았으므로 확정 원인으로 기록하지 않는다.

사용자 진행 확인 뒤 별도 수정 단위에서 다음을 수행한다.

1. Bastion Layer1 `(326,270)`의 Support 격자·가중치·flags를 Editor/Game에서 비교하고 실패를 재현하는 회귀 검사를 만든다.
2. 재현 근거에 맞춰 격자 경계 판정을 최소 수정한다. 전역 `/fp:fast` 해제나 지형 변경을 먼저 적용하지 않는다. 베이크 결과가 바뀌는 수정이면 freshness 규약과 재베이크 범위를 함께 판단한다.
3. 관련 자동화·cook freshness·재패키징·동일 1P 스모크를 통과시킨 뒤 다음 2P/late join 단위 진행을 확인한다. MCP 연결이나 전체 빌드용 에디터 종료가 필요하면 즉시 요청한다.

Phase9 전체는 미완료다. 별도 세션 소유인 NPC LoS·교전과 단위5의 멀티플레이·부하·메모리 Gate도 남아 있다.
