# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: **5-0·5-1 완료, 5-2 진행 중. 원본 입력 공급 분리와 Bool 갱신 보정 검증 완료. 다음 native DefaultSlot 최소 연결은 진행 확인 대기.**
> 마지막 갱신: 2026-10-28

Phase 0·1A·1C·2·3은 완료, 1B는 대기다. **최종 일곱 게이트·Go/No-Go는 미판정**이며 UAF 마이그레이션 승인으로 해석하지 않는다.

## 새 세션의 시작점과 승인 경계

1. `CLAUDE.md`·`README.md`·`Current.md`·`Decisions.md`를 읽고 `phases/Phase05_UAFSpike.md` §2·§4·§19~§25를 확인한다. 최종 게이트는 `Roadmap.md` Phase 5가 소유한다.
2. 이번 요청은 **인계 문서 정리와 단일 커밋**이다. 다음 몽타주 조사/구현은 시작하지 않았다. 새 세션에서 사용자 진행 요청을 받으면 §20의 **native DefaultSlot 하나 최소 연결**부터 이어간다. 기존 Strafe 뒤·Offset Root/발 배치 앞을 후보로 검토하고 첫 in-place 몽타주를 선택한다.
3. 연결 뒤 실제 UAF/GAS/ANS 대상 메시·단일 몽타주 전진·재생/정상 종료·즉시 Stop/블렌드아웃 Stop/교체를 확인한다. 현재 virtual 메시 조회 결과만으로 해당 계약을 통과시키지 않는다. 다른 슬롯·ANS5종·무기 레이어·루트모션·적/네트워크는 후속 게이트다.
4. **한 번에 한 구현 단위만 진행하고 완료마다 계속할지 확인한다.** PIE/GUI 수동 조작이 빠르면 즉시 요청한다. 전용 UAF 그래프 편집 MCP가 없으므로 간단한 트레이트 추가는 수동 GUI가 효율적이다. 인계만을 위해 기존 빌드/녹화를 반복하지 않는다.

## 이번 세션에서 확정한 결과

| 범위 | 결과 | 한계/상세 원본 |
|:---|:---|:---|
| 5-1 종료 | 최초 최소 기능 조건을 기존 근거에 대응해 완료 판정 | 최종 품질 동등성은 별도. 모든 본 수치 일치·특정 전환 클립 강제 선택을 새 조건으로 추가하지 않음. §19 |
| 5-2 연결 경로 조사 | 기본 Montage/Notify/GAS의 메시 선택과 일반 ABP 평가 충돌 확인 | 기본 Montage Trait는 아직 연결 전. §20 |
| 원본 공급 분리 | 시험 BP 재부모화·숨김 일반 평가 끔·명시적 입력/루트 피드백, 남극 부근/적도 수동 관찰 이상 없음 | GAS ActorInfo/UAF 몽타주/ANS 실제 저장 대상은 연결 후 확인. §21 |
| MM 순서 오류 | 실제 PIE에서 임계값 이하 양수 MM 자식의 갱신 누락/평가 잔류 재현. 보정 전133쌍 중 진단/오류1회 | 최초 무진단 이벤트의 인스턴스는 소급 특정하지 않음. 탭 닫기 요청 때 이미 닫혀 있었으므로 전/후 실험도 아님. §22~§23 |
| Bool 보정 | 기본 큐에서 빠진 유효 자식만 보충. **점프/착지67쌍·실제 보충2회·진단/PoseSearch 오류0·사용자 이상 없음** | DeltaTime0·NewlyRelevant=true·AlwaysUpdate=true·False 자식 경계·모든 그래프의 런타임 전수 검증은 아님. §24 |

- 최신 Development 전체 빌드: **15 actions·58.89초·Succeeded·종료 코드0**. 원본 공급층 전체 빌드14 actions·98.10초와 기존 구면 수치 회귀1/1(오류/경고0)은 이전 근거다. Live Coding은 사용하지 않았다.
- 최종 보정 시험 Root91창/4,925진단 프레임에서 CR 앞 거리≤30cm·Yaw≤90°·Tilt0°를 유지했다. 창 합계를 정확한 전체 PIE 프레임 수로 해석하지 않는다. CR 뒤 본별 포즈 동등성이나 모든 위도 조합의 근거가 아니다.
- 최종 PSD/포즈 품질 비교·지속 고속/전환·무기 통합·몽타주/ANS·적/네트워크·Mover 루트모션·최종 일곱 게이트는 남아 있다. 기존 Foley 이벤트 미처리 경고도 유지한다. 성능은 측정하지 않았다.

## 유지할 구현과 에셋

- 시험 BP `/Game/Characters/UAFSpike/BP_PlayerUAFSpike`의 부모는 `/Script/LootNPop.LNPUAFSpikePlayerCharacter`다. AnimSourceMesh는 기존 `ABP_SourceUAFSpike`·일반 평가=false·AlwaysTickPoseAndRefreshBones다. 정확한 SkeletalMeshComponent 질의만 원본 메시를 반환하고 다른 질의는 Super에 위임한다. 기본 플레이어/적 클래스는 바꾸지 않았다.
- 시험 폰이 몽타주 전용 초기화와 프로젝트 Native 초기화/슬롯 그룹을 준비하고 PrePhysics 틱에서 기존 B0/공유 입력을 공급한다. GFrameCounter 가드는 프레임당 최대 한 번 공급한다. UAF PrePhysics는 폰 공급 틱 뒤에 의존한다. 두 serial 표본만으로 매 게임 프레임 공급률을 전수 측정했다고 주장하지 않는다.
- 루트 평가 task의 완료 상태를 모듈 피드백에 기록한다. 게임 스레드는 UAF ReadComponent와 잠금으로 사본만 읽어 기존 OffsetRootTransform/bHasOffsetRoot를 갱신한다. FutureFacing/Aim은 이 CR 앞 UAF 루트를 기준으로 삼는다. 워커 trait 상태 포인터를 게임 스레드에서 직접 읽지 않는다.
- AG 흐름: **MM/Fall_Loop Bool + Pose History + LNP Bool Blend Update → Passthrough/Strafe/입력 어댑터 → Offset Root/최종 구면 보정 → Control Rig(v2)/Passthrough Blend → Output**. locator는 Current Actor→VisualMesh다. 기존 구면 CR·0.3/0.2초 Bool 알파·착지 리셋/발판 정책은 유지한다.
- Bool 보정은 기본과 같은 가중치/AlwaysUpdate/관련성 및 BlendingOut/NewlyRelevant 조건을 읽어 기본이 제외한 유효 자식만 추가한다. 기본 큐와 중복 등록하지 않고 자식 수명/가중치/블렌드를 바꾸지 않는다. DebugLocomotion2에서 `[UAFBoolUpdate]`가 실제 보충을 기록한다. `[UAFMMOrder]` 진단과 엔진 오류 검사도 억제하지 않았다.
- UAF/Internal·UAFAnimGraph/Internal·UAFWarping/Private·UAFPoseSearch 내부 API 의존성이 있다. UE 버전 갱신 시 재검토한다. 새 타입/trait 레이아웃 변경은 에디터 종료 후 전체 빌드한다. 과거 Live Coding은 완료 알림 Slate/mimalloc 경로에서 크래시했으며 원인은 미확정이다(§14).
- 현재 AG는 사용자가 보정 트레이트를 한 번 추가/Compile/Save한 버전이다. SHA256은 `5EFF88E13FEA53D4639517802ACB04D3E8EADA7A166789301CFFD793FCD9EBDC`다. 과거 AG 백업으로 덮어쓰지 않는다. 시험 Yaw 발판 BP는 유지하되 과거 임시 Actor/외부 파일은 정리돼 있다. 재배치 없이 이전 Teleport 명령을 실행하지 않는다.

## 인계 시 에디터와 맵 상태

- 마지막 읽기 조회는 **PIE=false·열린 에셋 없음·DebugLocomotion2**다. 재실행/새 PIE 뒤에는 상태와 실제 runtime ref를 새로 읽는다. 과거 Rewind/widget/인스턴스 ref를 재사용하지 않는다. 새 녹화는 없다.
- TestMap03의 현재 GameMode Override는 **BP_GameModeUAFSpike**다. §21에서 메모리만 바꾼 뒤 나중에 디스크 해시가 달라졌고, 재시작 후에도 시험 값인 것을 확인했다. 경위는 미확정이며 에이전트가 이번 인계에서 맵을 저장/원복하지 않았다.
- 사용자 확인한 원래 값은 **`/Game/Gameplay/BP_LNPGameMode.BP_LNPGameMode_C`**다. 시험 종료 정리 때 해당 속성만 복원하고 다른 사용자 맵 변경을 유지한다. 이번 커밋에는 TestMap03을 포함하지 않는다. 새 세션 시작 시 시험 GameMode를 그대로 사용할지 확인한다.
- 직접 조회가 지원되지 않는 RigVMPin.DefaultValue/TraitRootPinNames/AnimScriptInstance를 반복 호출하지 않는다. 간단한 GUI 추가와 실제 실행 로그를 사용했고, 필요할 때 사용자 읽기 전용 Python으로 인스턴스 경로를 확인했다. 새 자동화 도구를 만들지 않는다.

## 로컬 근거와 커밋 범위

`Saved`는 git 제외이므로 다른 체크아웃에 따라가지 않는다. 같은 로컬 프로젝트에서는 아래 원본을 재사용한다.

| 폴더(`Saved/AnimationOverhaul/` 아래) | 근거 |
|:---|:---|
| `2026-10-27_UAF51Closure/` | 5-1 종료·당시 GameMode 복원과 맵 백업/해시 |
| `2026-10-27_UAF52MontageInvestigation/` | 몽타주/ANS 메시 선택·초기화 조사 |
| `2026-10-27_UAF52SourceDriver/` | 공급층 전체 빌드·BP 설정/백업·구면 수치 회귀 |
| `2026-10-28_UAF52DriverSmoke/` | 남극 부근/적도 공급 스모크·serial·현재 디스크 맵 사본 |
| `2026-10-28_UAF52OrderInvestigation/`·`2026-10-28_UAF52OrderDiagnostic/` | 소스 경로 대조·오류 재현/MatchedIncident·진단 빌드 |
| `2026-10-28_UAF52BoolUpdateFix/` | 보정 빌드·AG 전후 사본·FixSmoke 로그/요약·보존 해시 |

- 사용자 요청으로 문서4개·시험 BP/AG2개·스파이크 소스/Build.cs10개의 **16파일을 한 커밋**으로 정리한다. 최근 프로젝트의 `feat: … 및 새 세션 인계` 제목과 한국어 항목별 본문을 따른다. 푸시는 하지 않는다.
- **기존 사용자 변경4개는 제외/보존한다:** `Content/Maps/TestMap03.umap`, `Content/Gameplay/DT_PlayerBaseStats.uasset`, `Content/ItemData/Weapons/Levels/DT_LongSword_Levels.uasset`, `Content/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies.uasset`. §24 전후 해당4개와 시험 BP의 해시가 같고 AG만 보정 추가로 변경됐다. 맵 전체 원복이나 자동 포함은 하지 않는다.
- Build.bat/cook/package는 처음부터 권한 확장 실행한다. 기준은 `LootNPopEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE`이며 전체 빌드/Live Coding을 동시에 사용하지 않는다. 종료 회신 직후 프로세스가 남아 있으면 종료 완료를 확인하고 빌드한다. 강제 종료하지 않는다.
- GASP 소스/참조 에디터는 쓰기 금지다(AN-009·AN-018). 소스 반입/리타깃은 README §4를 따른다. ISKM 총기 재베이크·1B 모캡·Locomotor는 별도 트랙이다.
