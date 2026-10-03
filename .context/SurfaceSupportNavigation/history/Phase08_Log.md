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