# Phase 9 Development 격자 정밀도 수정 V008

> 상태: Nav 정밀도 수정·전체126회귀·재패키징·snapshot 게시 통과. 1P 전체는 SourceKeys probe·적 배치 부족으로 미통과
> 날짜: 2026-10-12
> 범위: V007 패키지 1P의 Nav graph 실패 재현·최소 수정·재패키징·동일 1P 스모크

## 원인과 수정

V007의 Bastion Nav Layer1 `(326,270)`은 Nav 분할 수980에서 Support 분할 수735의 `(244.5,202.5)`에 놓인다. 아래쪽 삼각형의 세 꼭짓점은 flags3(Valid/Walkable)이고, 위쪽 삼각형의 추가 꼭짓점 `(245,203)`은 flags7(NeedsExact 포함)이다. 어느 삼각형을 선택하느냐에 따라 보간 가능 여부가 달라진다.

동일 저장 payload·17자리 정규화 방향 `(0.5704114504954324,0.47242666145327222,0.67189569628909829)`을 사용한 회귀 검사에서 Editor는 성공하고 Development 게임은 실패했다. 게임에서는 Bastion 위 노드와 Hollow Layer5 `(326,287)`의 graph 생성이 실패했다. `ComputeCell`의 연산 정밀도만 고정한 뒤 두 게임 실패가 모두 사라졌다. Editor `/fp:precise`와 Game `/fp:fast`가 격자 경계의 삼각형 선택을 다르게 만든 것이 원인이다. payload 손상이나 형상 결함으로 처리하지 않는다.

- `LNPSupportAtlas.cpp`: MSVC `float_control(precise, on, push)`/`pop`을 `ComputeCell` 함수에만 적용했다. 수식·전역 `/fp:fast`·격자 해상도·codec/schema는 유지했다.
- 새 `Runtime.SupportPrecision`: Editor와 Client context에서 세 production 저장본의 전체 Nav graph를 검사한다. Bastion 실패점은 보간 성공, 바로 인접한 NeedsExact 삼각형은 보간 거부를 요구한다. 가중치0인 꼭짓점도 포함해 세 꼭짓점 모두 안전해야 한다는 기존 규칙을 완화하지 않았다.
- 지형·LVI·SurfaceData·기본 설정은 수정하지 않았다. 원본 V007 패키지를 보존하고 별도 복제본에서 수정 전후 게임 실행 파일을 비교했다.

## 재현과 빌드

로컬 근거는 `Saved/SliceOctant/NavPrecisionV008`이다. `failed-node.json`·raw payload export는 읽기 전용이며 에셋을 저장하지 않았다.

| 검사 | 수정 전 | 수정 후 |
|:---|:---|:---|
| Editor `Runtime.SupportPrecision` | 성공1/실패0 | 전체 회귀에 포함 |
| Development Game `Runtime.SupportPrecision` | 성공0/실패1, Bastion·Hollow graph 실패 | 성공1/실패0, 0.15초 |
| 전체 저장 Nav 노드 | Editor만 전부 조회 가능 | Meadow67,729 + Bastion151,117 + Hollow185,568 = 404,414개 조회 성공 |
| 인접 NeedsExact 삼각형 | 안전 규칙 유지 | 보간 거부 통과 |

수정 전 게임 실행도 프로세스 exit0이므로 JSON 테스트 실패를 기준으로 판정했다. 최초 테스트 선언의 UE5.8에 없는 `ApplicationContextMask` 빌드 오류는 EditorContext/ClientContext로 정정했다. 정정 후 재현 빌드는 Editor7.35초·Game12.36초, 런타임 수정 후 전체 빌드는 Game30.23초·Editor26.06초에 성공했다. 각 빌드 로그와 `ReproEditorReport`·`ReproGameReport`·`FixedGameReport`를 보존했다.

## 전체 자동화·cook·패키지 1P

- 새 프로세스 전체126검사 모두 통과했다. 경고 없는 성공122·경고 포함 성공4·실패/미실행/진행 중0, 492.086초, 프로세스 exit0이다. 경고는 HTTP timeout3건과 기존 B-option authoring world 경고1건이다. 시작 시 RHI 등 엔진 경고도 있으므로 전체 경고0으로 기록하지 않는다.
- production 최신성·두 차례 베이크 결정론은266.359초·오류0·HTTP 경고1이다. 저장 source hash와 Support/Navigation/Traversal/Spawn/ConditionalPatch가 그대로 일치해 재베이크하지 않았다. Editor SupportPrecision은0.138초·오류/경고0이다.
- 테스트가 생성한 에셋3개는 실행 직전 실제 바이트를 복구하고 SHA256을 확인했다. production SurfaceData3개와 `DefaultGame.ini`도 입력 해시가 유지됐다. `test-asset-restoration.json`·`input-assets-verification.json`에 보존했다.
- 이 전체 보고서를 선행 게이트로 사용한 Win64 Development BuildCookRun이 exit0/64.649초(UAT63.79초), 1,229 packages cook·stage/pak/archive에 성공했다. 출력은 `Saved/SurfaceNavigationPhase9V008Package/Windows`, 실제 게임 exe SHA256은 `51B4E8DCD1E48EC88421B32AF0DF229A5B051335CD483AE34AABDDDEEA1797D5`다. production3LVI/3SurfaceData·정식SpawnConfig cook 포함을 `ReferencedSet.txt`에서 확인했다. 기존 Lyra ShaderMap 경고는 남는다.
- 로컬 실행기는 JSON 전체 실패/미실행/진행 중0과 production 최신성·SupportPrecision 성공을 요구한다. plain BuildCookRun 자체에 validator를 추가한 것은 아니다. V007의 실제 stale 음성 차단 근거는 유지한다.

### 실제 패키지 1P 결과

V007과 같은 `TestMap03 -nullrhi -nosound -corelimit=4 -unattended -FixedSeed -LNPLoadBaselineProduction -LNPLoadBaselineSeed=1 -LNPLoadBaselinePlayers=1 -LNPLoadBaselineQuit`를 실행했다. Support cache·exact lateral sweep·parallel movement는1, 추가 관통 진단은0이다. 프로세스 exit0/43.575초, timeout 없이 harness가 자동 종료했다. `package-1p.log`·종료 JSON·Nav 요청/timing CSV를 로컬 근거에 보존했다.

- Meadow slot0/3, Bastion1/5/7, Hollow2/4/6의 같은 풀 배치다. HitIdentity716 sources/미분류blocker0, generation1/validated slots8/source bindings358을 게시했다. Nav seam 높이차0cm, runtime Layer703/component·group641/seam links4330이다. V007의 Nav graph 오류는 재현되지 않았다.
- `ProbePanels`11개/실패0, `ProbeFaceIndex`2000방향/slot hit1997/FaceIndex누락0, `ProbeSurfaceData`8/8 queries·358/358bindings 모두 PASS다. capture4,577프레임에서 UnknownHits0·EnvelopeEscapes0·LayerJumps0, assert/ensure/fatal/unhandled exception 표식0이다. 추가 관통 검사는 꺼져 있으므로 관통 Gate 통과를 주장하지 않는다.
- **`ProbeSourceKeys`는 FAIL이다.** source는 Meadow11·Bastion100·Hollow12개로 총358개이고 slot 내 중복0이다. 기존 probe는 모든 key가8slot 각각에 있고 source 수가 같아야 한다고 요구한다. 단일 Meadow8slot 시절의 가정이 혼합 풀과 맞지 않아123unique keys 모두 inconsistent로 집계됐다. 실패를 PASS로 치환하지 않았으며, definition별 저장 source와 runtime key를 대조하도록 다음 단위에서 수정해야 한다.
- **적 배치 부족은 별도 실제 실패다.** Pod120/120은 배치됐지만 적은 요청2,235/배치2,130(105부족)이다. Bench01 540→500, Bench01_2 577→541, Bench01_3 308→308, Bench01_4 810→781이다. 원인은 미확정이며 테스트 편성으로 바꾸거나 부족을 허용하지 않았다.
- headless 관측 frame P95 7.62ms·exact P95 4.604ms·lock P95 0.070ms로 harness exact≤2ms는 FAIL이다. 이 실행은 고정 해상도·교전·2P 성능 Gate를 대신하지 않는다. 성능 합격으로 기록하지 않았다.
- 기존 ShaderMap 오류6개와 과거 Phase14에도 기록된 CharacterMovementComponent 추출 오류1개가 재현됐다. 이 단위에서 관련 없는 머티리얼·Mover를 수정하지 않았다.

따라서 **Nav graph 초기화 수정은 완료했지만 패키지1P 전체는 미통과**다. 종료 코드0만으로 통과를 판단하지 않는다.

## 남은 범위

사용자 진행 확인 뒤 별도 구현 단위에서 혼합 풀 `ProbeSourceKeys`를 definition별 저장 source와 대조하도록 수정하고, 정식 편성105마리 부족을 재현·원인에 맞춰 수정한다. 해당 자동화·재패키징·동일1P를 통과한 뒤 리슨2P·late join 진행을 다시 확인한다. 별도 세션 소유 NPC LoS·교전, 고정 해상도 성능·메모리·추가 관통 진단 Gate도 남아 있다. headless 1P 기능 결과를 이 항목들의 통과로 확대하지 않는다.
