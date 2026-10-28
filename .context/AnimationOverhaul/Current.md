# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: **5-0·5-1 완료, 5-2 진행 중. §32 Combo/Cancel의 Actor·월드 종료 대표 검증4개는 완료했다. End 각1·중복/ensure/관련 오류0이며 Combo Actor의 생존 PlayerState 태그 제거도 확인했다. 월드 독립 태그/사후 조회 한계를 유지한다. PIE 종료·GameMode 복원·에셋6개 보존 완료, 다음 UpperBody 슬롯·그룹 연결 조사 진행 확인 대기다. 전체 게이트/Go-NoGo는 미판정이다.**
> 마지막 갱신: 2026-10-28

Phase 0·1A·1C·2·3은 완료, 1B는 대기다. **최종 일곱 게이트·Go/No-Go는 미판정**이며 UAF 마이그레이션 승인으로 해석하지 않는다.

## 새 세션의 시작점과 승인 경계

1. `CLAUDE.md`·`README.md`·`Current.md`·`Decisions.md`와 `phases/Phase05_UAFSpike.md` §2·§4·§31~§32를 먼저 읽는다. 슬롯 구성은 §26~§27, 종료 보완의 근거는 §28~§30을 필요할 때 읽는다. 최종 게이트는 `Roadmap.md` Phase 5가 소유한다.
2. §26의 **native DefaultSlot 최소 연결 단위는 완료**다. 실제 UAF 피격 task·정상 종료·대표 구간 전진·재생 중 Stop0/Stop0.5/교체·종료 잔류 없음·해당 구간 순서 오류0을 확인했다. 첫 무효 중단 표본은 재시험으로 보완했다. 0초 Stop은 의도대로지만 플레이 감각은 어색하다는 사용자 관찰을 유지하고 실사용 블렌드 정책은 바꾸지 않았다.
3. §31 활성3종 보완과 §32 Combo/Cancel의 Actor/월드 종료 대표 검증은 완료했다. Actor 파괴의 태그 제거·활성 Melee 실제 무효와 월드 종료 End 전달/정리 요청을 구분한다. 월드 GAS 메시None·AfterDestroyUnavailable은 독립 태그/실제 무효 증명이 아니다. **다음 제안은 UpperBody 슬롯·그룹 최소 연결 경로 조사**이며 진행 확인 전 착수하지 않는다. native 참조1개 미특정·예약 GT 이벤트/콜백 내부 파괴/Branching/다른 배치 모드와 전체 게이트는 미판정이다.
4. **한 번에 한 구현 단위만 진행하고 완료마다 계속할지 확인한다.** PIE/GUI 수동 조작이 빠르면 즉시 요청한다. 전용 UAF 그래프 편집 MCP가 없으므로 간단한 트레이트 추가는 수동 GUI가 효율적이다. 인계만을 위해 기존 빌드/녹화를 반복하지 않는다.

## 새 세션에서 바로 할 일

1. 진행 확인을 받은 뒤 기존 UpperBody 몽타주/그룹/상체 마스크 계약과 native UAF Montage·레이어 경로를 읽어 최소 연결 방안을 좁힌다. 슬롯 이름만 추가한 결과를 본 마스크/AO/IK 계약 통과로 보지 않는다. 간단한 GUI 배선이 효율적이면 즉시 수동 요청한다.
2. §31~§32의 정상/대표 종료 검증·빌드를 인계 확인만으로 반복하지 않는다. 월드 독립 태그/사후 무효, 예약 GT 이벤트/콜백 내부 파괴/다른 배치 모드와 전체 게이트의 미판정 한계를 유지한다. Phase 6·무기 레이어 마이그레이션은 승인하지 않았다.

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
| 모듈 종료 보완 | 정상 공격15쌍·정상 종료 후 중복 End0. 생존 Actor의 활성 차단2종+Melee 제거에서 End 각1·태그 제거·엔티티 실제 정리·관련 오류0 | Combo/Cancel은 §29 후속 검증 완료. Actor/월드는 §31~§32 대표 확인. 다른 배치 모드는 미검증. §28 |
| Combo/Cancel 활성 제거 | 별도 새 PIE에서 Combo/이동 차단 Begin/End 각1과 Cancel Begin/Tick/End 각1. 제거 후 다음 시뮬레이션 프레임에 End·추가 콜백/관련 태그 잔류 없음 | Queued(mode0) 표본. Melee 비활성·native 추가 참조1개 미특정. Actor/월드는 §32 대표 확인. 다른 배치 모드는 미검증. §29 |
| Actor/월드 종료 재현 | 두 경로에서 활성 차단2종+Melee End 각0·Module release could not end 4 states 경고. Actor 파괴 뒤 생존 PlayerState에 차단 태그2개 잔류 | 보완 전 실패 근거. Melee DestroyQueued 없음·실제 엔티티 잔류 미확인. 월드 종료 직전 추가1프레임의 Tick 포함. §30 |
| 조기 종료 보완 | 정상15쌍·정상 종료 뒤 중복0. 활성 Actor 파괴/생존 Actor 제거는 End3종 각1·태그 제거·엔티티 실제 무효·ensure0. 월드 종료는 End3종 각1·DestroyQueued·관련 오류0 | 월드 독립 태그/사후 무효는 미관측. Combo/Cancel은 §32 대표 검증 완료. 콜백 내부 파괴/다른 배치 모드는 미검증. §31 |
| Combo/Cancel 소유 Actor 종료 | 새 PIE4개에서 Combo/Cancel Actor 파괴·월드 종료의 End 각1·중복/ensure/관련 오류0. Combo Actor 태그 제거·Cancel 종료 후 추가 Tick0 | 월드 독립 태그/사후 조회는 불가. Cancel 월드 Stop 처리 중 Tick1회 추가 뒤 End1, End 이후 Tick0. §32 |

- 최신 Development 전체 빌드: **6 actions·45.08초·Succeeded·종료 코드0**(§31 재진입 유효성 가드/관측 보완). 이전4 actions·9.12초 성공과 정상 회귀는 별도 근거다. 첫 컴파일의 비공개 API C2248 및 이후 Actor 재시험 ensure도 로그에 보존한다. Live Coding은 사용하지 않았다.
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
- `LNPUAFNotifyAudit.h/.cpp`와 ANS5종 호출부/Cancel Begin·End override는 전체 빌드를 통과했다. 기본값0은 꺼짐,1은 Begin/End/엔티티,2는 기존 기능 Tick도 기록한다. 실제 태그/몽타주/엔티티 정책을 바꾸지 않는다. Mass 사후 읽기는0.1초 간격이며 Pause에서는 월드 시각이 진행할 때까지 기다린다. 월드 종료 시 미확인으로 기록하며 정리를 대신하지 않는다.
- `FLNPUAFNotifyCleanup`은 기본 dispatcher 뒤의 활성 사본을 잠금으로 보호한다. 시험 폰 Destroyed/EndPlay에서 연결 해제 전에 사본 종료 소유권을 인수하고 잠금 밖에서 컴포넌트 등록 해제/실제 End를 전달한다. 무효 UAF 참조는 읽지 않으며 이후 소멸자의 중복 End를 막는다. 소멸자 fallback의 메시/월드 소실·tearing down 가드는 유지한다. Audit은 Pause 중 월드 시각이 진행할 때까지 사후 관측을 기다린다. 예약 GT 이벤트·콜백 내부 파괴/다른 배치 모드·월드 사후 실제 무효까지 해결했다고 보지 않는다.

## 인계 시 에디터와 맵 상태

- 인계 직전 실제 읽기는 **PIE=false·TestMap03의 GameMode=원래 BP_LNPGameMode**다. GameMode 속성만 메모리 복원했고 맵은 저장하지 않았다. 사용자 에셋4개/시험 BP/AG 총6개는 §32 시작/종료/인계 HandoffHashes가 모두 같다. 이전 단위의 종료 상태는 각 실행서/로그가 소유한다.
- 진단2/2는 시험 뒤 변경하지 않았다. 재시작/다음 PIE에서는 실제 값과 런타임 참조를 다시 읽고 기존 ans_* Python 참조를 재사용하지 않는다. UAF 제거/Actor 파괴는 PIE 인스턴스에만 적용됐고 BP/AG는 그대로다. 이번 인계만을 위한 새 PIE·빌드·녹화는 하지 않는다.
- 디스크 맵은 이전부터 시험 GameMode를 포함한 사본이다(§21·§25). 재시작/맵 재로드 시 메모리 복원이 유지된다고 가정하지 않는다. 다음 시험에서 임시 지정하면 종료 때 해당 속성만 복원한다. 맵 전체 원복·자동 저장은 하지 않는다.
- 직접 조회가 지원되지 않는 RigVMPin.DefaultValue/TraitRootPinNames/AnimScriptInstance를 반복 호출하지 않는다. 간단한 그래프 배선/PIE는 수동 요청하고 실제 실행 로그를 사용한다. 새 자동화 도구를 인계 확인용으로 만들지 않는다.

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
| `2026-10-28_UAF52ComboCancelRelease/` | Before/AfterHashes·LogStart·Combo/Cancel 제거 전 로그·제거 로그/요약·Closure 근거. §29 |
| `2026-10-28_UAF52ActorWorldShutdown/` | Actor/월드 종료 전 로그·ActorDestroy/WorldStop 로그와 요약·월드 종료 전 태그·Before/AfterHashes·Closure 근거. §30 |
| `2026-10-28_UAF52EarlyNotifyShutdown/` | 소스4개 초기 사본/Audit 추가 사본·보존 해시·빌드/수정 실패와 최종 성공 로그·정상/Actor/월드/생존 제거 원문/요약·Closure. §31 |
| `2026-10-28_UAF52ComboCancelOwnerShutdown/` | Combo/Cancel Actor/World 종료 전·실행 로그/요약·Actor 생존 PlayerState 태그/PIE 종료 근거·시작/종료 해시·Closure. §32 |

- 사용자 인계 요청에 따라 이번 변경 **문서4개·소스5개 총9개**를 한 커밋으로 정리한다. 소스는 Cleanup2개·시험 폰2개·Audit.cpp다. 이전 커밋의 `feat:` 제목·한국어 항목별 본문 형식을 따른다. 사용자 에셋4개와 git 제외 `Saved`는 포함하지 않으며 푸시/후속 구현은 하지 않는다.
- **기존 사용자 변경4개는 보존한다:** `Content/Maps/TestMap03.umap`, `Content/Gameplay/DT_PlayerBaseStats.uasset`, `Content/ItemData/Weapons/Levels/DT_LongSword_Levels.uasset`, `Content/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies.uasset`. §28 시작 대비 해당4개/시험 BP/AG의 디스크 해시가 모두 같다. FullBody AG의 저장 변경은 이전 §27 결과다. 맵 전체 원복이나 자동 포함은 하지 않는다.
- Build.bat/cook/package는 처음부터 권한 확장 실행한다. 기준은 `LootNPopEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE`이며 전체 빌드/Live Coding을 동시에 사용하지 않는다. 종료 회신 직후 프로세스가 남아 있으면 종료 완료를 확인하고 빌드한다. 강제 종료하지 않는다.
- GASP 소스/참조 에디터는 쓰기 금지다(AN-009·AN-018). 소스 반입/리타깃은 README §4를 따른다. ISKM 총기 재베이크·1B 모캡·Locomotor는 별도 트랙이다.
