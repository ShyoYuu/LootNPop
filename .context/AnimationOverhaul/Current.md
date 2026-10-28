# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: **5-0·5-1 완료, 5-2 진행 중. §28의 생존 Actor UAF 제거 보완 단위는 완료했다. 이전 End3개 누락 조건에서 End 각1·차단 태그 제거·근접 엔티티 실제 정리를 확인했다. 나머지 ANS/Actor·월드 종료 검증은 다음 단위 진행 확인 대기다. 전체 종료 게이트는 미판정이다.**
> 마지막 갱신: 2026-10-28

Phase 0·1A·1C·2·3은 완료, 1B는 대기다. **최종 일곱 게이트·Go/No-Go는 미판정**이며 UAF 마이그레이션 승인으로 해석하지 않는다.

## 새 세션의 시작점과 승인 경계

1. `CLAUDE.md`·`README.md`·`Current.md`·`Decisions.md`와 `phases/Phase05_UAFSpike.md` §2·§4·§28을 먼저 읽는다. 공급/Bool/슬롯의 상세가 필요할 때만 §20·§24·§26~§27을 추가로 읽는다. 최종 게이트는 `Roadmap.md` Phase 5가 소유한다.
2. §26의 **native DefaultSlot 최소 연결 단위는 완료**다. 실제 UAF 피격 task·정상 종료·대표 구간 전진·재생 중 Stop0/Stop0.5/교체·종료 잔류 없음·해당 구간 순서 오류0을 확인했다. 첫 무효 중단 표본은 재시험으로 보완했다. 0초 Stop은 의도대로지만 플레이 감각은 어색하다는 사용자 관찰을 유지하고 실사용 블렌드 정책은 바꾸지 않았다.
3. §27의 End3개 누락/차단 태그 잔류는 §28의 활성 참조 사본·모듈 해제 End 전달로 보완했다. 같은 활성 Melee 제거에서 다음 시뮬레이션 프레임에 **End3개 각1·태그 제거·엔티티 파괴 후 무효**를 확인했다. 정상 공격3회·정상 종료 후 UAF 제거에서도 프로젝트 ANS 중복 End는 없었다. native 사본은 Idle1/활성4개로 프로젝트3개 외 참조1개는 대상 미특정이다. **다음 제안은 Combo/Cancel 활성 제거와 Actor/월드 종료 경로 검증**이며 사용자 진행 확인 전 착수하지 않는다. 월드 종료 중 호출 생략이라는 현재 구현 한계를 유지하고, 전체 종료 게이트·최종 Go/NoGo는 미판정이다.
4. **한 번에 한 구현 단위만 진행하고 완료마다 계속할지 확인한다.** PIE/GUI 수동 조작이 빠르면 즉시 요청한다. 전용 UAF 그래프 편집 MCP가 없으므로 간단한 트레이트 추가는 수동 GUI가 효율적이다. 인계만을 위해 기존 빌드/녹화를 반복하지 않는다.

## 새 세션에서 바로 할 일

1. 인계된 코드/AG와 실제 에디터 상태를 읽기로 대조한다. 아래 마지막 확인은 PIE=false·원래 GameMode·진단2/2지만 재시작 후 유지된다고 가정하지 않는다. 사용자에게 다음 단위 진행 요청을 받으면 기존 정상 시험을 반복하지 않고 Combo/Cancel의 활성 모듈 제거부터 시작한다. 재현 절차·명령과 성공 기준은 실행서 §28 마지막에 있다.
2. 그 결과와 Actor/월드 종료의 생명주기/정리 소유권을 확인해 보완 범위를 정한다. 현재 월드 종료 중 End 생략·미특정 native 참조1개를 숨기지 않는다. Combo/Cancel·Actor/월드의 모든 조합을 자동화하거나 새 계측 도구를 만들기 전에 수동 조작을 요청한다. Phase 6·무기 레이어 마이그레이션은 승인하지 않았다.

## 이번 세션에서 확정한 결과

| 범위 | 결과 | 한계/상세 원본 |
|:---|:---|:---|
| 5-1 종료 | 최초 최소 기능 조건을 기존 근거에 대응해 완료 판정 | 최종 품질 동등성은 별도. 모든 본 수치 일치·특정 전환 클립 강제 선택을 새 조건으로 추가하지 않음. §19 |
| 5-2 연결 경로 조사 | 메시 선택/일반 평가 충돌에 공급 분리·native 슬롯으로 대응. 정상 공격 콜백의 GAS/ANS 메시/인스턴스 일치 확인 | 중단/종료·전체 슬롯/그룹은 후속. §20·§26~§27 |
| 원본 공급 분리 | 시험 BP·명시적 입력/루트 피드백과 구면 스모크 유지. 정상 공격의 실제 GAS ActorInfo/ANS 원본 대상 확인 | 다른 종료/네트워크 조건 전수 검증은 아님. §21·§27 |
| MM 순서 오류 | 실제 PIE에서 임계값 이하 양수 MM 자식의 갱신 누락/평가 잔류 재현. 보정 전133쌍 중 진단/오류1회 | 최초 무진단 이벤트의 인스턴스는 소급 특정하지 않음. 탭 닫기 요청 때 이미 닫혀 있었으므로 전/후 실험도 아님. §22~§23 |
| Bool 보정 | 기본 큐에서 빠진 유효 자식만 보충. **점프/착지67쌍·실제 보충2회·진단/PoseSearch 오류0·사용자 이상 없음** | DeltaTime0·NewlyRelevant=true·AlwaysUpdate=true·False 자식 경계·모든 그래프의 런타임 전수 검증은 아님. §24 |
| DefaultSlot | Montage Trait의 **Always Update Source=true**로 유지. native 피격 task·정상 종료·대표 전진·재생 중 Stop0/Stop0.5/교체 확인, 유효 중단 구간 순서 오류0 | false에서 MM/Pose History 오류 각236회. 0초 Stop의 감각은 어색함. 전체 슬롯/ANS·GAS 대상 통과는 아님. §26 |
| FullBody/ANS 결과 | 정상/이동 취소/대시 교체·활성 ANS Stop0/Stop0.5는 End1·정리 확인. 두 몽타주 런타임 그룹 DefaultGroup 일치 | §27 시스템 제거 실패는 §28의 같은 활성3종 제거에서 보완 확인. 나머지 종료 경로/전체 계약은 미판정 |
| 모듈 종료 보완 | 정상 공격15쌍·정상 종료 후 중복 End0. 생존 Actor의 활성 차단2종+Melee 제거에서 End 각1·태그 제거·엔티티 실제 정리·관련 오류0 | Combo/Cancel 활성 제거·Actor/월드 종료·다른 이벤트 배치 모드 미검증. §28 |

- 최신 Development 전체 빌드: **7 actions·47.13초·Succeeded·종료 코드0**(§28 ANS 종료 보완). §27 ANS 진단16 actions·106.09초·Bool 보정15 actions·58.89초·공급층14 actions·98.10초·기존 구면 수치 회귀1/1은 이전 근거다. Live Coding은 사용하지 않았다.
- 최종 보정 시험 Root91창/4,925진단 프레임에서 CR 앞 거리≤30cm·Yaw≤90°·Tilt0°를 유지했다. 창 합계를 정확한 전체 PIE 프레임 수로 해석하지 않는다. CR 뒤 본별 포즈 동등성이나 모든 위도 조합의 근거가 아니다.
- 최종 PSD/포즈 품질 비교·지속 고속/전환·무기 통합·몽타주/ANS·적/네트워크·Mover 루트모션·최종 일곱 게이트는 남아 있다. 기존 Foley 이벤트 미처리 경고도 유지한다. 성능은 측정하지 않았다.

## 유지할 구현과 에셋

- 시험 BP `/Game/Characters/UAFSpike/BP_PlayerUAFSpike`의 부모는 `/Script/LootNPop.LNPUAFSpikePlayerCharacter`다. AnimSourceMesh는 기존 `ABP_SourceUAFSpike`·일반 평가=false·AlwaysTickPoseAndRefreshBones다. 정확한 SkeletalMeshComponent 질의만 원본 메시를 반환하고 다른 질의는 Super에 위임한다. 기본 플레이어/적 클래스는 바꾸지 않았다.
- 시험 폰이 몽타주 전용 초기화와 프로젝트 Native 초기화/슬롯 그룹을 준비하고 PrePhysics 틱에서 기존 B0/공유 입력을 공급한다. GFrameCounter 가드는 프레임당 최대 한 번 공급한다. UAF PrePhysics는 폰 공급 틱 뒤에 의존한다. 두 serial 표본만으로 매 게임 프레임 공급률을 전수 측정했다고 주장하지 않는다.
- 루트 평가 task의 완료 상태를 모듈 피드백에 기록한다. 게임 스레드는 UAF ReadComponent와 잠금으로 사본만 읽어 기존 OffsetRootTransform/bHasOffsetRoot를 갱신한다. FutureFacing/Aim은 이 CR 앞 UAF 루트를 기준으로 삼는다. 워커 trait 상태 포인터를 게임 스레드에서 직접 읽지 않는다.
- AG 흐름: **MM/Fall_Loop Bool + Pose History + LNP Bool Blend Update → Passthrough/Strafe/입력 어댑터 → Montage Trait(DefaultSlot) → Montage Trait(FullBody) → Offset Root/최종 구면 보정 → Control Rig(v2)/Passthrough Blend → Output**. 두 Montage는 Always Update Source=true·Disable Synchronization=false다. FullBody 대시 관찰/재생 교체·생존 Actor의 활성3종 종료 보완을 확인했지만 전체 종료 계약은 미판정이다. locator는 Current Actor→VisualMesh이며 기존 구면 CR·0.3/0.2초 Bool 알파·착지 리셋/발판 정책은 유지한다.
- Bool 보정은 기본과 같은 가중치/AlwaysUpdate/관련성 및 BlendingOut/NewlyRelevant 조건을 읽어 기본이 제외한 유효 자식만 추가한다. 기본 큐와 중복 등록하지 않고 자식 수명/가중치/블렌드를 바꾸지 않는다. DebugLocomotion2에서 `[UAFBoolUpdate]`가 실제 보충을 기록한다. `[UAFMMOrder]` 진단과 엔진 오류 검사도 억제하지 않았다.
- UAF/Internal·UAFAnimGraph/Internal·UAFWarping/Private·UAFPoseSearch 내부 API 의존성이 있다. UE 버전 갱신 시 재검토한다. 새 타입/trait 레이아웃 변경은 에디터 종료 후 전체 빌드한다. 과거 Live Coding은 완료 알림 Slate/mimalloc 경로에서 크래시했으며 원인은 미확정이다(§14).
- 현재 AG는 Bool 보정·DefaultSlot 뒤 FullBody를 사용자가 추가/Compile/Save한 버전이다. SHA256은 `DADEE0844CEE352A47965875247B2D1CA4A697E4B343A5D85B0E35EBC351C00D`다. FullBody 이전 정상 기준 사본은 SlotANS/AG_DefaultSlotBaseline.uasset에 있다. 과거 AG 백업으로 덮어쓰지 않는다. 시험 Yaw 발판 BP는 유지하며 재배치 없이 이전 Teleport 명령을 실행하지 않는다.
- `LNPUAFNotifyAudit.h/.cpp`와 ANS5종 호출부/Cancel Begin·End override는 전체 빌드를 통과했다. 기본값0은 꺼짐,1은 Begin/End/엔티티,2는 기존 기능 Tick도 기록한다. 실제 태그/몽타주/엔티티 정책을 바꾸지 않는다. Mass의 지연 파괴 뒤0.1초 읽기 확인은 월드 종료 시 미확인으로 기록하며 정리를 대신하지 않는다.
- `FLNPUAFNotifyCleanup`은 기본 dispatcher 뒤 OnEndExecution에서 활성 참조를 소유한 사본으로 보관한다. 기존 루트 trait가 기본 dispatcher 존재 후 등록하며 GT 모듈 해제 때 실제 NotifyEnd를 전달한다. 엔진/기본 dispatcher·태그/TTL 정책은 수정하지 않는다. 메시/월드가 사라졌거나 월드 종료 중이면 경고 후 생략하므로 그 경로까지 해결했다고 보지 않는다.

## 인계 시 에디터와 맵 상태

- §28 시험 뒤 **PIE=false·열린 에셋 없음**을 확인했다. UAF 컴포넌트 제거는 종료된 PIE 인스턴스에만 적용돼 BP 에셋은 그대로다. DebugNotifies2/DebugLocomotion2를 유지한다. 다음 PIE/재시작 때 runtime ref를 새로 읽으며 기존 ans_* Python 참조를 재사용하지 않는다. 이번 단위는 새 녹화 없이 로그를 사용했다.
- §28 종료 때 GameMode 속성 하나만 원래 `/Game/Gameplay/BP_LNPGameMode.BP_LNPGameMode_C`로 메모리 복원/재조회했다. **맵 dirty=true·미저장**이다. 디스크 맵/다른 사용자 변경은 보존하며 맵 전체 원복/자동 저장은 하지 않는다.
- 디스크 맵은 이전부터 시험 GameMode를 포함한 사본이고 경위는 §21·§25와 같다. 재시작/맵 재로드 시 현재 메모리 복원이 유지된다고 가정하지 않는다. 다음 시험 시 실제 값을 새로 읽고, 시험용 값을 임시 지정한 경우 종료 때 속성만 복원한다. 맵 전체 원복·자동 저장/커밋은 하지 않는다.
- 직접 조회가 지원되지 않는 RigVMPin.DefaultValue/TraitRootPinNames/AnimScriptInstance를 반복 호출하지 않는다. 간단한 GUI 추가와 실제 실행 로그를 사용했고, 필요할 때 사용자 읽기 전용 Python으로 인스턴스 경로를 확인했다. 새 자동화 도구를 만들지 않는다.

## 로컬 근거와 변경 범위

`Saved`는 git 제외이므로 다른 체크아웃에 따라가지 않는다. 같은 로컬 프로젝트에서는 아래 원본을 재사용한다.

| 폴더(`Saved/AnimationOverhaul/` 아래) | 근거 |
|:---|:---|
| `2026-10-27_UAF51Closure/` | 5-1 종료·당시 GameMode 복원과 맵 백업/해시 |
| `2026-10-27_UAF52MontageInvestigation/` | 몽타주/ANS 메시 선택·초기화 조사 |
| `2026-10-27_UAF52SourceDriver/` | 공급층 전체 빌드·BP 설정/백업·구면 수치 회귀 |
| `2026-10-28_UAF52DriverSmoke/` | 남극 부근/적도 공급 스모크·serial·현재 디스크 맵 사본 |
| `2026-10-28_UAF52OrderInvestigation/`·`2026-10-28_UAF52OrderDiagnostic/` | 소스 경로 대조·오류 재현/MatchedIncident·진단 빌드 |
| `2026-10-28_UAF52BoolUpdateFix/` | 보정 빌드·AG 전후 사본·FixSmoke 로그/요약·보존 해시 |
| `2026-10-28_UAF52DefaultSlot/` | AG 변경 전/false/true 사본·첫 오류/재검증·UAF task·전진 비교·유효 Stop/교체·정리 조회/해시 |
| `2026-10-28_UAF52SlotANS/` | AG/빌드/보존·FirstNormal/MovementDash·PausedStops·**SystemShutdown 로그/요약/태그(종료 실패)**·ClosureAudit/EditorReads |
| `2026-10-28_UAF52NotifyCleanup/` | 보완 전 소스/해시·종료 가드·전체 빌드·NormalRegression/IdleRelease/ActiveSystemRelease 로그·요약·태그·AfterPIEStop/Closure 근거 |

- 이번 인계 커밋 범위는 **문서4개·시험 AG1개·소스11개, 총16개**다. 소스는 §27 진단8개와 §28 종료 보완3개이며, 포함 범위는 기존 소스7개/신규4개다. 사용자의 이번 요청으로 한 커밋에 정리한다. 이전 `feat:` 제목·한국어 항목별 본문 양식을 따르며 사용자 에셋4개와 git 제외 `Saved`는 포함하지 않는다. 후속 구현이나 푸시는 이번 요청 범위에 없다.
- **기존 사용자 변경4개는 보존한다:** `Content/Maps/TestMap03.umap`, `Content/Gameplay/DT_PlayerBaseStats.uasset`, `Content/ItemData/Weapons/Levels/DT_LongSword_Levels.uasset`, `Content/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies.uasset`. §28 시작 대비 해당4개/시험 BP/AG의 디스크 해시가 모두 같다. FullBody AG의 저장 변경은 이전 §27 결과다. 맵 전체 원복이나 자동 포함은 하지 않는다.
- Build.bat/cook/package는 처음부터 권한 확장 실행한다. 기준은 `LootNPopEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE`이며 전체 빌드/Live Coding을 동시에 사용하지 않는다. 종료 회신 직후 프로세스가 남아 있으면 종료 완료를 확인하고 빌드한다. 강제 종료하지 않는다.
- GASP 소스/참조 에디터는 쓰기 금지다(AN-009·AN-018). 소스 반입/리타깃은 README §4를 따른다. ISKM 총기 재베이크·1B 모캡·Locomotor는 별도 트랙이다.
