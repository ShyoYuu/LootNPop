# Phase 8 작업 기록

## 2026-10-03 — 단위 1: Nav 점유 합성 기반

사용자가 Phase 8 완료 후 Phase 13으로 진행하는 순서를 승인했다. 구현 단위마다 완료·검증 후 진행 확인을 받는다. 실행 계획은 `../phases/Phase08_ConditionalPatchAndOverlay.md`다.

### 구현

- `FLNPNavMarkerBlocker`: `(slot, MarkerId)`와 generation을 포함한 runtime node ref 목록으로 활성 마커의 점유를 표현한다.
- `LNPNavOverlay::BuildOverlay`: Pod와 활성 마커 점유를 OR 합성한 뒤 기존 이음매 사본 차단과 Tile revision 계산을 적용한다. 전체 활성 목록을 매번 합성하므로 source 하나가 사라져도 겹친 source의 차단은 유지된다. 같은 차단 mask는 revision을 증가시키지 않는다.
- 기존 `BuildPodOverlay`는 마커 목록 없이 새 함수를 호출한다. production Pod 소비 경로는 그대로 이 경로를 사용한다.
- 잘못된 MarkerId·slot·slot 불일치·stale node 주소는 출력 변경 전에 거부한다. 함수의 false는 동일 상태 또는 입력 거부를 뜻하며 API에 이를 명시했다. 이전 overlay는 같은 generation에서만 사용한다.
- 기존 `Nav.PodOverlay`에 서로 겹친 두 마커, 하나/전부 제거, 같은 상태 재적용, Pod/마커 교차 겹침 제거, slot/generation 거부, 무관 slot의 차단/revision 유지 검사를 추가했다. 기존 Pod 우회와 scheduler 재계획 검사도 유지했다.

### 검증

| 검사 | 결과 | 증거 |
|:---|:---|:---|
| `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 전체 빌드 | Succeeded, 67.07초 | UBT `Log.txt`·`Trace.uba`(사용자 LOCALAPPDATA) |
| `Automation RunTests LootNPop.SurfaceNavigation.Nav.` | 실검증 15개 성공, 비용 재생 1개 SKIP; 실패 0 | `Saved/Logs/Phase08_Unit1_Nav.log`, `Saved/Tests/Phase08_Unit1/index.json` |
| `Nav.PodOverlay` 확장 검사 | Success | 위 로그 `PodOverlay` 결과 |
| 비용 재생 | CSV 입력 미지정으로 SKIP, 수행 성공 수에 포함하지 않음 | 위 로그 `Request cost replay skipped` |

에디터 프로세스가 감지되지 않은 상태에서 전체 빌드와 command-line 자동화를 실행했다. 두 실행은 프로젝트 밖 UBT/DDC 쓰기 때문에 처음부터 권한 확장으로 실행했다. Nav 자동화 로그에 `Error:`·assert·ensure·crash가 없고 에셋 변경이 없음을 확인했다. 초기화의 비대상 플랫폼 SDK 진단과 scalability 경고는 테스트 실패로 세지 않는다.

이 단위는 순수 Nav 점유 합성 기반이다. 아직 마커 Actor 상태 수집·Conditional Patch stream·Support/Walk Link 활성화·Mass phase 경계 게시를 연결하지 않았으며, 1P/2P·cook·부하 Gate는 후속 연결 뒤 수행한다. Phase 8 전체 완료가 아니다.

### 다음 행동

단위 2 진행 확인을 받은 뒤 patch authoring·베이크·codec을 구체화한다. 콘텐츠 조작이 효율적이면 MCP 연결을 요청하고, 전체 빌드가 필요하면 에디터 저장·종료를 요청한다.

## 2026-10-03 — 단위 2: Patch authoring·베이크·codec

### 구현

- 마커 `PatchSources`는 mesh·마커 로컬 안정 transform·Support 참여를, `PatchCondition`은 Intact/Bridge를 저장한다. source는 모두 해당 상태의 blocker이며 Support 참여 source만 보행면을 만든다. 빈 source 목록은 patch 없는 기존 요소다.
- `LNPConditionalPatch` codec v1: MarkerId GUID 순, 정적 Layer 뒤 연속 ID, sparse Support, source ordinal별 exact face map, 추가 Nav, base 점유·edge 차단, patch를 포함한 양방향 Walk edge다. 지각을 중복 저장하지 않는다. component ID는 patch 내부 0부터다.
- 베이커는 source를 마커 안정 자세에 놓고 기존 exact physics helper로 node·polyline clearance를 검사한다. base Nav canonical bytes를 보존하며 delta만 저장한다. seam/위치 상한·정적 sheet 내부의 캡슐 높이 겹침·연결 없는 Bridge를 거부한다.
- DataVersion 7·BakerSchemaVersion 9. 모든 stream은 versioned 빈 목록도 저장한다. 마커 GUID·actor transform·class·경로·속도/대기·조건·mesh·source transform·Support 참여를 canonical semantic hash에 포함한다. patch mesh saved hash와 Blueprint class package saved hash도 manifest에 포함한다.
- 로더는 codec/hash/size/count·순서·ID·row·face map·endpoint를 검사하며 decoded patch를 asset별 공유한다. serialized/resident memory 계수에 포함했다. 아직 활성 Support/Nav view나 Actor 전이는 연결하지 않는다.

### 검증

- `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 전체 빌드 성공. 최초 78.85초, 테스트 보완 후 마지막 빌드 6.92초. 마지막 증거 `Saved/Logs/Phase08_Unit2_Build.log`.
- `ConditionalPatch.Codec`: sparse round-trip·byte 결정론·빈 stream·잘린/추가 byte·잘못된 codec·중복 GUID·Layer alias·row 범위·face map·endpoint 거부, base 주소 보존과 변경 거부.
- `ConditionalPatch.Bake`: 저장하지 않는 fixture marker에 실제 cooked Support mesh와 blocker를 주입한다. sparse 바닥/Nav·base node/edge 차단·고립 Intact 바닥·blocker-only·GUID/ID·결정론, 조건/자세/Support 참여/경로/속도 freshness 변경을 확인했다. 겹침·seam·고립 Bridge 거부와 정적 섬 경계에 접합한 Bridge의 exact Walk edge 생성도 성공했다.
- 메모리 UPROPERTY round-trip에 Patch stream과 descriptor를 추가했다. loader에 Patch hash 오염·count 불일치 거부, 8 slot의 공유 검사를 추가했다. 기존 결정론 베이크 검사는 저장된 Patch stream도 비교한다.
- Meadow_00·Fixture_Crust·Fixture_Regression SurfaceData 3개 재베이크 성공. 현재 LVI의 PatchSources는 없으므로 각 stream은 0 patch/4바이트다. 새 형상은 위 자동화에서 검증하며 영구 동적 회귀 콘텐츠는 단위 5다. 원본 에셋 백업은 `Saved/Tests/Phase08_Unit2_AssetBackup`.
- 라이브 MCP로 TestMap03과 기존 DataVersion 6 에셋을 확인했다. 사용자 저장·에디터 종료 완료 응답 뒤 전체 빌드·명령행 검증을 실행했다. 테스트 중 엔진/에셋 조작을 위한 새 MCP 연결은 필요하지 않았다.

전체 SurfaceNavigation 자동화: report 87개 Success(경고 포함 1개), 실패/미실행 0. 이 중 비용 재생 1개는 CSV 미지정 SKIP이므로 실검증 성공은 86개다. B-option 독립 에셋 검사는 현재 authoring 맵 대신 기존 추출 에셋을 검사했다는 경고 1개다. `Error:`·assert·ensure·fatal은 없었다. 증거: `Saved/Logs/Phase08_Unit2_Full.log`, `Saved/Tests/Phase08_Unit2/index.json`. 테스트 전용 에셋 3개를 실행 직전 백업에서 복구했고 SHA256 모두 일치했다(`Saved/Tests/Phase08_Unit2/TestAssetRestore.json`). 단위 2 완료이며 단위 3 진행 확인을 기다린다. cook·1P/2P·late join·부하 Gate는 Actor 활성화 연결 뒤 단위 5에서 수행한다.

## 2026-10-03 — 단위 3: 런타임 활성화와 경로 무효화

사용자의 Phase 8 재개 요청으로 단위 3을 진행했다. 구현 범위는 런타임 view·게시·경로 무효화이며 Actor 상태 연결은 단위 4다.

### 구현

- 로드 시 `LNPConditionalPatchRuntime::Prepare`가 전체 patch Layer/component/node 주소를 고정한다. 동일 asset의 결합 Support·Nav·asset graph는 slot끼리 공유한다. patch 없는 production asset은 정적 로드 경로를 유지한다. DataVersion 7/schema 9는 그대로이며 재베이크하지 않았다.
- `BuildActiveSnapshot`은 비활성 기준 snapshot과 활성 `(slot, MarkerId)` 집합으로 Support mask·추가 node·양방향 Walk edge·base node/edge 차단을 합성한다. 중복 상태는 게시하지 않고 겹친 blocker 하나가 사라져도 다른 source의 점유를 보존한다.
- patch 차단이 정적 component 내부를 갈라놓을 수 있으므로 Pod 점유를 제외한 실제 활성 graph의 연결 성분을 다시 계산한다. 현재 group은 node별이며 split component의 요약 group은 `MAX_uint32`다. 실제 topology가 바뀔 때만 connectivity version을 증가시킨다.
- 실제 node/edge·Support 상태 변화의 Tile revision만 증가한다. 명시적 edge 변화는 양쪽 Tile을 갱신한다. Pod overlay가 patch revision을 합성하고, 완성된 성공 경로는 통과 Tile로 검증한다. 실행 중 A*는 version 변경 시 Stale, 접근점 경로·도달 불가 결과는 새 연결을 평가하기 위해 재계획한다.
- `SetPatchActive`와 Pod 제거는 변경을 모은다. Nav path subsystem의 Mass PrePhysics 시작 delegate가 worker dispatch 전 Support/Nav snapshot·overlay를 게시하고 invalidated 경로를 큐에 넣는다. 지상 적은 연결성 version 변경 후 Home 도달성을 다시 확인한다. 이전 snapshot ref는 불변으로 유지된다.

### 검증과 수정 경위

- 첫 전체 빌드는 unity 묶음 변경으로 기존 Patch/Nav codec의 익명 namespace `SerializeNode`가 충돌했다. Patch codec 함수만 `SerializePatchNode`로 구분했다.
- `ConditionalPatch.Runtime`: Support 활성/제거·preferred handle·stable node 주소·명시적 edge A*·slot 분리·중복 상태·겹친 blocker 제거·edge-only 정적 component 분할·지역 revision·성공 경로 유지/재계획·도달 불가 결과 재연결·Pod 재선택·잘못된 주소/generation 거부를 확인한다.
- `ConditionalPatch.Publication`: 서버 상태 요청이 경계 전에는 게시하지 않고, 같은 프레임의 상태 변화를 최종 상태로 합성하며 이전 reader의 데이터가 불변인지 확인한다. 실제 delegate가 부르는 경계 함수의 데이터 검증이다. 실제 PIE 틱 순서와 Actor/collision의 원자적 전환은 단위 4~5에서 검증한다.
- 기존 `ConditionalPatch.Bake`에 실제 cooked mesh patch의 8-slot 로드·결합 데이터 공유·slot별 활성 mask·exact Walk edge 소비 검사를 추가했다.
- 첫 전체 자동화(`Saved/Tests/Phase08_Unit3/index.json`)는 89개 중 87개 Success·2개 Fail이었다. `GraphView`와 `NavAssembly`는 Nav 차단 node의 지면 정보까지 거부한 회귀다. 지면 정보 조회는 기존 이음매 계약을 유지하고 비활성 Support mask만 적용하도록 수정했다. 보행 투영·component/group 조회는 계속 차단한다.
- 수정 후 위 실패 2개와 Runtime·Publication은 4/4 성공, 오류/경고 0이다(`Saved/Tests/Phase08_Unit3_RegressionFix/index.json`, `Saved/Logs/Phase08_Unit3_RegressionFix.log`). 최종 전체 자동화 결과는 아래에 기록한다.

### 최종 결과

| 검사 | 결과 | 증거 |
|:---|:---|:---|
| `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 전체 빌드 | 성공. 런타임 보완본 114.72초, 최종 이음매 수정 빌드 5.93초 | `Saved/Logs/Phase08_Unit3_Build.log`, `Phase08_Unit3_BuildFinal.log` |
| 최종 전체 SurfaceNavigation 자동화 | report 89개 Success(경고 포함 1개), 실패/미실행 0. 비용 재생 1개 SKIP을 뺀 실검증 88개 성공 | `Saved/Tests/Phase08_Unit3_Final/index.json`, `Saved/Logs/Phase08_Unit3_Final.log` |
| 테스트 에셋 3개 복구 | 실행 직전 백업과 SHA256 모두 일치 | `Saved/Tests/Phase08_Unit3_Final/TestAssetRestore.json` |
| diff 형식 검사 | `git diff --check` 성공 | 최종 workspace 검사 |

최종 report의 경고는 기존 B-option 추출 에셋 검사 1개이며 `Error:`·assert·ensure·fatal은 없다. 비용 재생은 CSV 입력 미지정으로 SKIP했다. 첫 전체 실행의 추가 HTTP timeout 경고는 최종 실행에는 없다. 테스트 에셋 백업은 `Saved/Tests/Phase08_Unit3_TestAssetBackup/Manifest.json`이다. 에디터 프로세스가 없는 것을 확인하고 전체 빌드·명령행 자동화를 권한 확장으로 실행했다. 이 단위는 재베이크·새 영구 콘텐츠·Actor 상태를 요구하지 않아 라이브 MCP 연결은 요청하지 않았다.

단위 3 완료. 상태형·파괴 Actor의 상태/자세·collision·exact identity를 patch 전환과 연결하는 단위 4와 최종 cook·1P/2P·late join·부하 Gate는 남아 있다. 구현 단위별 확인 규칙에 따라 여기서 멈추고 단위 4 진행 확인을 받는다.

## 2026-10-03 — 단위 4: 상태형 요소와 파괴

사용자의 단위 4 진행 확인을 받고 구현했다. 마커 `ElementClass=ALNPStatefulTerrain`이 기존 서버 deferred 스폰 경로를 그대로 사용한다. 새 영구 회귀 콘텐츠와 최종 네트워크/패키지 Gate는 단위 5다.

### 구현

- 초기 복제 Config는 `(slot, MarkerId)`·마커 월드 transform·source mesh/안정 transform·조건·StandingTransform·TransitionSeconds다. State는 상태 값·revision·server world epoch의 시작 시각이다. 서버·클라가 같은 이름의 net-addressable Movable component를 만들며 ReplicatedMovement와 물리 시뮬레이션은 사용하지 않는다.
- Bridge 조건은 Standing으로 시작한다. 서버 `StartTransition()`이 시작 시각을 확정하고 translation 선형/quaternion 최단 slerp로 안정 형상에 도달한다. 서버 Actor 틱에서 Bridge 상태를 확정해 patch를 켠다. 전이 중 exact collision과 DynamicSupport 운반만 유지하며 계획적 traversal은 꺼져 있다.
- Intact 조건은 온전한 collision과 활성 patch로 시작한다. 서버 `BreakTerrain()`은 다음 요소 틱에서 collision·표현·exact 등록·patch를 제거한다. 중복 요청은 revision을 반복 증가시키지 않고 임의 잔해/새 보행면은 만들지 않는다. 전이·파괴 API는 서버에서 호출하는 게임플레이 연결점이며 새 HP/피격 시스템은 추가하지 않았다.
- API는 상태만 바꾸고 실제 형상/identity/patch 요청은 TG_PrePhysics Actor 틱에서 적용한다. 요소 틱→DynamicSupport와 dirty exact registry 게시→Mass PrePhysics 시작에서 Support/Nav/경로 overlay 게시→worker 순서다. 기존 PostPhysics exact registry 게시도 유지한다. 마지막 요소가 제거된 뒤 빈 DynamicSupport frame도 게시해 이전 운반 정보가 남지 않게 한다.
- 안정 source ordinal의 베이크 face map·generation을 runtime exact entry에 연결한다. 안정 NPC 착지는 SurfaceHandle을 사용하고 전이 중에는 `(slot, MarkerId)` DynamicSupport를 쓴다. blocker-only source는 Support 역할을 갖지 않는다. 회전 접촉점의 `linear + angular × offset` 속도를 물리 body와 NPC 이탈 관성에 반영한다.
- Standing 자세는 단위 scale, 전이 시간은 양수로 제한하고 Bridge patch 마커 semantic hash에 포함한다. 해당 입력을 바꾸면 stale이다. stream/geometry 알고리즘과 DataVersion 7/schema 9는 유지한다. patch가 없는 현재 production 입력 bytes는 바뀌지 않아 재베이크하지 않았다.

### 검증과 수정

- 첫 빌드에서 `DOREPLIFETIME` 매크로가 요구하는 `OutLifetimeProps` 인자 이름을 수정했다. 최종 `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 전체 빌드는 성공했다(`Saved/Logs/Phase08_Unit4_BuildFinal.log`, 6.30초).
- 선별 Patch·DynamicSupport 자동화 6/6 성공, report 오류/경고 0(`Saved/Tests/Phase08_Unit4_Targeted/index.json`). 베이크 stale 검사는 Standing 자세/전이 시간 변경과 Standing scale 거부를 추가했다. DynamicSupport 회귀는 회전 접촉점 이탈 속도와 안정 Layer의 SurfaceHandle 선택을 추가했다.
- `ConditionalPatch.StatefulTerrain`은 서버 deferred 초기화·Standing collision·Support/blocker 역할·중복 상태 revision·전이 자세/운반·traversal 비활성·서버 종점 확정·patch 경계 게시·stable exact face map·반복 안정 상태·파괴 collision/identity 제거·Intact 초기 점유/제거·클라이언트 권위 거부를 검사한다. 복제 Config/State 데이터와 클라이언트 역할로 수신 순서/late pose를 모사한다. 실제 네트워크 번치·component movement-base 참조는 단위 5에서 검사한다.
- 실제 raycast 검사를 보강한 첫 전체 실행은 90개 중 89개 Success(경고 포함 1개)·StatefulTerrain 1개 Fail이었다(`Saved/Tests/Phase08_Unit4_Final/index.json`, 188.73초). 같은 월드에 겹쳐 만든 클라이언트 모사 형상을 raycast 대상에서 제외하지 않은 fixture 오류로 안정 source 검사와 파괴 후 empty 검사가 실패했다. query에서 모사 Actor를 제외해 검사 대상 Actor를 분리했다. 이 수정은 테스트 fixture만 바꾼다.
- 수정 후 StatefulTerrain 1/1 성공, report 오류/경고 0이다(`Saved/Tests/Phase08_Unit4_RegressionFix/index.json`, `Saved/Logs/Phase08_Unit4_RegressionFix.log`). stable collision hit의 Layer 해석과 파괴 후 hit 소실까지 통과했다. 나머지 전체 회귀는 이미 통과했으므로 같은 전체 목록을 반복하지 않았다. 수정 전 전체 실패 report를 보존하며 단일 전체 실행 90/90 성공으로 보고하지 않는다.

최종 검증은 전체 실행과 수정 후 선별 실행을 합쳐 실검증 89종 통과다. 비용 재생 1개는 CSV 미지정으로 SKIP했다. 기존 B-option 추출 에셋 경고 1개가 있으며 assert/ensure/fatal은 없다. 테스트가 재저장한 에셋 3개는 실행 직전 `Saved/Tests/Phase08_Unit4_TestAssetBackup/Manifest.json` 백업으로 복구했고 SHA256이 모두 일치한다(`Saved/Tests/Phase08_Unit4_Final/TestAssetRestore.json`). 콘텐츠 변경은 남지 않았다. diff 형식 검사도 통과했다.

단위 4 완료. 단위 5의 영구 회귀 콘텐츠·수동 PIE/패키지 1P·리슨 2P/late join·cook/stale·700마리 반복 Gate는 아직 수행하지 않았다. 에디터 프로세스가 없어 전체 빌드에 별도 종료 요청은 필요하지 않았고, 라이브 콘텐츠 작업이 없는 이 단위는 MCP 연결을 요청하지 않았다. 여기서 멈추고 단위 5 진행 확인을 받는다.

## 2026-10-03 — 커밋 후 Unity 빌드 실패 수정

사용자가 커밋 `637df28` 뒤 컴파일 실패를 알려 에디터 전체 빌드를 재실행했다. 에디터 프로세스는 없었으며 기본 옵션 빌드에서 C2084/C2264를 재현했다. `LNPConditionalPatch.cpp`와 `LNPNavRuntime.cpp`의 익명 namespace `SameAgent` 함수가 `Module.LootNPop.5.cpp`에 함께 포함된 충돌이다. 수정 중에는 Adaptive Unity가 두 파일을 개별 컴파일해 기존 검증에서 드러나지 않았다. 커밋 후 working set이 비면서 Unity 묶음에 다시 들어가 실패했다. 이전 빌드 성공만으로 커밋 상태의 Unity 구성을 검증하지 못한 누락이다.

Patch 쪽 정의와 호출 두 곳만 `SamePatchAgent`로 구분했다. 함수 본문과 런타임 동작은 바꾸지 않았다. `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE -DisableAdaptiveUnity`로 working set 예외 없이 같은 Unity 묶음을 컴파일해 성공했다(25.65초, `Saved/Logs/Phase08_PostCommit_UnityBuildFixed.log`). 이어 기본 옵션 에디터 전체 빌드도 성공했다(26.43초, `Saved/Logs/Phase08_PostCommit_DefaultBuildFixed.log`). 사용자 측 이전 실패 로그는 `Saved/Logs/Phase08_PostCommit_PreviousBuildFailure.log`에 보존했다. diff 형식 검사도 통과했다. 이름 충돌 수정이므로 런타임 자동화는 반복하지 않았다. 사용자 요청으로 코드·기록 수정을 기존 Phase 8 단위 3·4 커밋에 함께 포함한다. 단위 5는 진행하지 않았다.
