# Phase 3 — 물리 히트리액트 본 구현

> 상태: 완료(2026-10-23). 3-1·3-2 구현, 수동 회귀·포즈 실측, 일회성 계측 정리 후 최종 에디터 빌드 통과
> 선행: Phase 1A(RBWC 노드 방식 확정, `research/PhysicsHitReactSpike.md`), Phase 2 완료(최종 포즈 파이프라인 확정 — 표시 ABP의 왼손 IK 뒤·출력 앞이 RBWC 자리)
> 순서: 3-1 → 3-2. 단위 하나가 세션 하나이고, 단위마다 커밋하고 사용자 확인을 받는다.
> 범위: 플레이어·`ActorPromoted` 적의 `VisualMesh` 상체 물리 반응(코스메틱·비복제, AN-004). 경직(GA_Stagger)·다운·Light/Heavy/Parried 경직 몽타주는 건드리지 않는다. 파워드 래그돌(AN-005)과 `PureEntity`(ISKM)는 범위 밖.

## 단위 나눔

| 단위 | 내용 | 완료 조건 |
|:---|:---|:---|
| **3-1** 경로·방향성 | 두 표시 ABP에 RBWC 노드 배치, 큐 → `PlayHitReact` → 임펄스 경로 본 연결(방향·세기·맞은 지점), 방향성 후보 3안 비교·선택, 스파이크 산출물 정리. 기존 히트리액트 몽타주는 유지하고 물리를 덧붙인다 | 아래 3-1 체크리스트 |
| **3-2** 몽타주 대체·방향성 보강·적 분리 | 방향성 보강(결정 D) 뒤 히트리액트 몽타주 완전 대체(결정 C) 판정·실행(`CHT_Montage` HitReaction 행 정리 포함), 적 반응 세기 차이(`PA_UEFN_Mannequin`) — 메시별 세기 또는 PhysicsControl 프로필 분리, 3-1에서 남긴 확인 항목(URO·LOD로 1프레임 임펄스 소실 등), 방향·무기별 곡선이 필요하면 GASP B11식 애니 곡선 → 알파·강도 | 아래 "단위 3-2" |

3-2에 필요한 결정은 3-1 결과와 함께 `Current.md`에 남긴다.

## 단위 3-1 — 경로·방향성

목표: 플레이어·승격 적의 표시 ABP에서 피격 물리 반응이 방향성 있게 동작한다. 에디터는 LootNPop(8000)만 쓴다.

### 설계

- **애님 인스턴스.** 스파이크 클래스 `ULNPPhysicalHitReactAnimInstance`를 `ULNPAnimInstance` 파생으로 바꾸고 두 표시 ABP의 부모로 삼는다. 표시 ABP는 원래 `ULNPAnimInstance`를 부모로 써서 왼손 IK·견착 보정 변수를 받으므로, 그 공급을 그대로 물려받고 RBWC 핀 세 개(`HitForces`·`HitControlParameters`·`PhysicsAlpha`)만 더한다. `ULNPAnimInstance`(메인 ABP 공용)에는 아무것도 넣지 않는다.
- **노드 위치.** `… → 왼손 TwoBoneIK → RigidBodyWithControl → ComponentToLocal → 출력`. 두 ABP 모두 이미 컴포넌트 공간 구간이 있어 변환 노드를 더 넣지 않는다. 노드 설정은 스파이크 값(research §2.2)을 그대로 옮긴다.
- **경로.** `FLNPApplyDamageGECommand`(서버)가 큐에 `Location`=맞은 지점, `Normal`=피격자→공격자, `RawMagnitude`=피해, `NormalizedMagnitude`=근접이면 1을 싣는다. 두 크기 필드는 0이면 직렬화되지 않는다. 화염 틱처럼 큐를 내지 않던 경로는 `bSuppressHitReact` 대신 `ELNPHitReactKind::None`이다. 큐 → `PlayHitReact(방향, 지점, 피해, 근접)` → 몽타주 재생 뒤 `QueueHitReact`.
- **세기.** `clamp(피해 / LNP.Anim.PhysHitReactRefDamage(20), 0.5, 2) × (근접 ? LNP.Anim.PhysHitReactMeleeScale(1.5) : 1)`. 같은 프레임 여러 피격은 가장 센 것 하나.
- **방향성 모드.** `LNP.Anim.PhysHitReactMode` 비트를 조합한다. 1 = 가슴 선속도(스파이크), 2 = `Up × PushDir` 축 각속도, 4 = 맞은 지점에 가장 가까운 바디(`spine_03~05`·`head`·`upperarm_l/r`)에 월드 지점 임펄스, 8 = 피격 구간 동안 `ParentSpace_Torso` 각강도 배율을 낮춤(`ControlAndModifierParameters` 핀 — 이 세트는 실제로 팔·목·다리까지 담아 사실상 온몸이다), 16 = 히트리액트 몽타주 생략(물리만 보기, `PlayHitReact`가 읽는다). 0 = 물리 끔(몽타주만). 기본값 12.
- **스파이크 정리.** `BeginPlay`의 애님 클래스 교체 CVar `LNP.Anim.PhysHitReactSpikeABP`는 지우고, `Content/Characters/PhysHitReactSpike/`는 노드를 옮긴 뒤 폴더째 삭제한다(설정 원본은 research §2.2 표에 남아 있다).

### 3-1 체크리스트

- [x] C++: 애님 인스턴스 부모 변경·방향성 모드·큐 파라미터·`ELNPHitReactKind`, 스파이크 CVar 제거(헤더 변경 → 에디터 재시작 1회)
- [x] 두 표시 ABP 부모를 `ULNPPhysicalHitReactAnimInstance`로 바꾸고 RBWC 노드 배치·핀 바인딩, 경고를 오류로 취급한 컴파일 통과
- [x] 스파이크 사본 폴더 삭제
- [x] PIE ① 방향성 비교 — 모드 1·2·4와 8 조합을 같은 조건에서 비교해 기본값을 고른다
- [x] PIE ② 확인 묶음 — HitStop, 사망 랙돌 진입·`ExitRagdoll` 표현 풀 재사용, 캡슐·Mover·판정 무영향, 근접 콤보·가드·패링 중 피격, 적도 근처, 승격 적, 리슨 2인 각 머신 로컬 재생
- [x] 커밋 뒤 깨끗한 트리에서 `Build.bat` 재확인
- [x] 문서: 이 체크리스트, `history/Phase03_Log.md`, `Roadmap.md`, `Current.md`(3-2 첫 행동·필요한 결정), `research/PhysicsHitReactSpike.md`(방향성 비교 결과), 공통 설계가 바뀌면 `TechDesign_CombatAnimation.md` §6.4

### 3-1 결과 (2026-10-21, 상세 `history/Phase03_Log.md`)

| 항목 | 결과 |
|:---|:---|
| 배치 | `ABP_Retarget`·`ABP_VisualCopyPose` 부모 `ULNPPhysicalHitReactAnimInstance`, `… → TwoBoneIK(hand_l) → RigidBodyWithControl → ComponentToLocal → 출력`. 노드 값은 스파이크와 필드 차이 0. 컴파일(경고=오류) 2/2 |
| 경로 | 큐 `Location`·`Normal`·`RawMagnitude`(피해)·`NormalizedMagnitude`(근접 1) → `PlayHitReact` → `QueueHitReact`. 자동 PIE에서 큐 직접 실행으로 힘 구성까지 확인 |
| 방향성 비교 ① 몽타주 위 | 모드 1·2·4·9·10·12 모두 "자연스럽고 괜찮다", 모드끼리 체감 차이 작음. 방향별 차이로 느낀 것은 대부분 4방향 몽타주였다 |
| 방향성 비교 ② 물리 단독(비트 16) | 모드 17·26·28과 과장값(강도 배율 0.1, 지점 800, 젖힘 20). "가벼운 피격은 몽타주를 대체할 수 있겠다 — 움찔은 확실하다. 맞은 부위·방향을 확실히 체감할 정도는 아니다" |
| 기본값 | 모드 12(지점 임펄스 + 강도 낮추기, 몽타주 유지), 강도 배율 0.1, 지점 임펄스 800cm/s. 모드끼리 차이가 작아 맞은 부위 정보를 쓰는 쪽을 골랐다 |
| 확인 묶음(리슨 2인) | 사용자 "전부 이상 없음"(근접 콤보·가드·패링 중 피격, HitStop, 적도, 랙돌 뒤 표현 풀 재사용, 사망·리스폰, 캡슐·Mover, 왼손). 보정 횟수 물리 끔 84/분 → 켬 60/분(늘지 않음), 호스트·게스트 각자 기록, `LogPhysicsControl` 경고 0 |
| 스파이크 정리 | `LNP.Anim.PhysHitReactSpikeABP`·`BeginPlay` 교체 코드·`Content/Characters/PhysHitReactSpike/` 삭제 |

## 단위 3-2 — 몽타주 대체·방향성 보강·적 분리 (완료 2026-10-23)

- [x] 첫 실험 C++(32·64·골반 타깃 핀) 구현, 에디터 전체 빌드
- [x] 두 표시 ABP 골반 월드 오프셋 설정·핀 연결, 컴파일(경고=오류)·저장
- [x] 사용자 PIE 28/60/84/116 비교·선호 모드 116 선정(경직 몽타주도 임시 생략해 비교, 비교 후 복구 완료)
- [x] 사용자 승인 후 히트리액트 몽타주 대체·Chooser HitReaction 10행 제거(나머지 16행 조건 동일·저장 확인)
- [x] 대체 후 일반 피격·경직 몽타주·공격/재장전 왼손 그립 사용자 PIE 확인
- [x] 적 반응 세기 사용자 PIE 판정 — "현재 세기가 적당하다고 느껴져". 메시별 세기 분리는 하지 않는다
- [x] 플레이어 근접 공격 중 물리 억제·NPC 공격 중 반응 확인(AN-024) — Live Coding 성공, 사용자 PIE 4항목 "모두 정상". 플레이어 억제 29회(잔여 종료 1회 포함)·공격 밖 등 물리 반응 41회·NPC 근접 몽타주 중 반응 70회·PhysicsControl 경고/오류·컨트롤 누락·ensure 0건
- [x] 현재 URO·RBWC LOD 평가 생략 설정 확인 — 두 VisualMesh URO 꺼짐·AlwaysTickPoseAndRefreshBones, 두 노드·전역 LOD 제한 −1. 현재 설정 유지, 평가 생략을 켤 때 임펄스 수명 재검토(연구 §9)
- [x] 새 기본값으로 적도·2인·랙돌/LOD 재접근·골반/다리·플레이어 리스폰 최종 수동 회귀 — 사용자 "모두 정상". 같은 Actor 풀 재사용을 직접 추적한 검증은 아님(증거 한계·기존 Mover 경고는 로그)
- [x] 후속 실측 — 100의 플레이어 근접 공격 1,043샘플 차이·실제 알파 모두 0, NPC 공격 중 활성 313샘플 가슴 평균 2.7308cm·19.4093°. 알파 0 대조군 10,550샘플 차이 0(모드 0 구간은 미기록). 연구 §10
- [x] 골반 과장 완화 — 기본 이동 3cm·젖힘 6°·비틀림 7.5°(종전보다 25% 감소). 사용자 "골반 켠 100이 나음"으로 유지. 68은 골반 차이 0, 100 최대 2.475cm·7.9223°. 앞의 광범위 수동 회귀는 조정 전, 이후 1인 실측·체감 판정은 조정 후
- [x] 마지막 에디터 전체 빌드 — 계측 훅 제거 후 소스 컴파일 통과. 재실행된 에디터의 DLL 잠금으로 링크 1회 실패, 사용자 종료 후 최종 명령 `Succeeded`(남은 링크·메타데이터 2/2, 2.96초)
- [x] 일회성 매 평가 계측·`PhysHitReactMeasure` 제거. 기본 꺼짐 피격/억제 로그·68/100 비교 유지, 원본 로그·집계·계측 소스 사본은 `Saved`에 보존
- [x] 종료 문서 정리·한 번의 최종 커밋 — 사용자 "Phase 3 종료 정리까지 다 마치고 마지막에 한번에 커밋" 승인에 따라 이 문서를 포함해 묶음. 다음 구현은 착수 전 사용자 확인

### 결정 C — 몽타주 대체 범위 (확정: 완전 대체를 목표로, 2026-10-22 사용자 결정)

가벼운 히트리액트 몽타주(`CHT_Montage` HitReaction 행)를 **물리로 완전히 대체하는 것을 목표로** 한다. 3-2에서 방향성을 보강한 뒤 대체 여부를 PIE로 최종 판정하고, 통과하면 `PlayHitReact`의 히트리액트 몽타주 재생을 걷어내고(지금 비트 16이 그 스위치의 시험판) `CHT_Montage` HitReaction 행을 정리한다. 경직(GA_Stagger)·다운·Light/Heavy/Parried 몽타주는 유지한다(AN-004).

실행 결과(2026-10-22): 사용자가 116을 선호하고 몽타주 제거를 승인했다. 기본 모드는 **100(4+32+64)**으로, 116에서 더 이상 필요 없는 몽타주 생략 비트 16만 제거했다. `PlayHitReact`의 몽타주 호출·방향 태그 분류 삭제, Chooser 10행 제거·남은 16행 조건 동일 확인, C++ Live Coding 성공. 경직 몽타주 임시 차단도 제거·복구했다. 사용자 PIE에서 일반 피격·경직 몽타주·공격/재장전 왼손 그립에 "문제 없어" 응답, 로그에서 모드 100 피격 74회·경직 몽타주 재생 2회·PhysicsControl 경고 0건 확인.

### 결정 D — 방향성 보강 (확정: 3-2에서 시도, 2026-10-22 사용자 결정)

물리 단독으로 부위·방향 체감이 약한 원인은 구조다 — 골반·다리가 키네마틱이고 부모 공간 각도 컨트롤만 있어, 몸 전체가 밀리거나 반 걸음 물러나는 성분이 없다. 또 강도 낮추기 세트(`ParentSpace_Torso`)가 온몸이라 팔·머리 흔들림이 몸통이 밀리는 방향을 가린다. 3-2 첫 실험(3-1 세션이 설계·컴파일까지 해 둔 안):

| 비트 | 내용 | 구현 요지 |
|:---|:---|:---|
| 32 골반 밀림 | 키네마틱 골반에 월드 공간 오프셋 — 미는 방향 이동·그쪽으로 젖힘·맞은 쪽 비틀림 | RBWC `KinematicTargets` 핀(새 UPROPERTY `FRigidBodyKinematicTargets` → **헤더 변경, 에디터 재시작 1회**)에 `pelvis` 타깃. 노드 초기 모디파이어 갱신의 `pelvis` 항목을 `kinematicTargetSpace=OffsetInWorldSpace`(`bEnableKinematicTargetSpace=true`)로 바꿔 위치는 더하고 회전은 골반 원점에서 월드 축으로 앞에 곱하게 한다. 곡선 e(t) = (t/T)·exp(1−t/T)×세기(정점 T 0.1초, 애님 시간). 이동 = PushDir×4cm, 젖힘 = 축 `Up × PushDir`·8°, 비틀림 = 축 Up·−10°×좌우값. 좌우값 = clamp(((맞은 지점 − `spine_04`)·(Up × PushDir)) / 20cm, −1, 1) — 그쪽 어깨가 밀리면 Up 둘레 음의 방향(검산: (Up×P)×P = −Up). 다리는 애니 포즈 그대로라 큰 값은 엉덩이 변형이 보인다 |
| 64 척추만 무르게 | 강도 낮추기를 척추 컨트롤에만 | 후보 `pelvis_spine_01`·`spine_01_spine_02`·`pelvis_spine_02`·`spine_02_spine_03`·`spine_03_spine_04`·`spine_04_spine_05` 중 표시 메시 PhysicsAsset에 두 바디가 있고 실제 부모 관계가 일치하는 것만 `ControlMultiplierParameters`에 넣는다. 없는 이름을 보내면 매 프레임 경고가 난다(첫 PIE에서 확인·수정). 8과 함께 켜면 8(온몸)이 이긴다 |

비교: 몽타주를 끈 상태(비트 16)에서 **28 / 60 / 84 / 116**으로 본다. 28 = 지점 임펄스 + 온몸 강도 낮추기, 60 = 28 + 골반 밀림, 84 = 지점 임펄스 + 척추만 강도 낮추기, 116 = 84 + 골반 밀림이다. 기존 제안 92/124는 비트 8과 64가 함께 켜져 8 우선 규칙 때문에 척추만 약하게 하는 비교가 안 되므로 수정했다(2026-10-22). CVar `LNP.Anim.PhysHitReactPelvisShift`·`PelvisLean`·`PelvisTwist`·`PelvisPeakSeconds` 기본값은 각각 4cm·8°·10°·0.1초다. 이것으로 부족하면 나머지 후보(골반 월드 공간 컨트롤을 피격 때만 풀기, 유지 시간 늘리기, 방향별 가산 기울기)를 본다. 체감 비교 때는 경직 몽타주가 끼지 않게 피격 간격을 두고 `LNP.Anim.PhysHitReactLog 1`로 히트리액트/경직 몽타주 재생을 구분한다(3-1 로그 §5).

그 밖의 3-2 범위: 적 반응 세기 차이(`PA_UEFN_Mannequin` 바디 구성 — 메시별 세기 또는 PhysicsControl 프로필 분리), URO·LOD로 평가가 건너뛰어질 때 1프레임 임펄스 소실 여부, 대체 시 몽타주가 하던 일(왼손 IK 알파·가산 슬롯) 확인.

## Phase 3 종료

- 최종 기본값: 모드 100, 골반 이동 3cm·젖힘 6°·비틀림 7.5°·정점 0.1초. 68은 골반만 끈 비교 값이다. 적 세기 분리는 사용자 판정에 따라 하지 않았다.
- 플레이어 근접 몽타주 전체·블렌드아웃 동안 물리 반응을 억제한다. NPC는 공격 중에도 허용한다(AN-024). 경직 몽타주·HitStop·판정 ANS는 유지했다.
- 리슨 2인·적도·랙돌/LOD 재접근·리스폰 수동 통과, 포즈 실측과 골반 완화 선호 판정 통과. 일회성 계측 제거는 진단 코드만 바꾸며 이미 검증한 반응 계산·ABP는 바꾸지 않았다.
- 현재 URO·RBWC LOD 평가 생략은 꺼져 있다. 해당 최적화를 켤 때 임펄스 수명을 재검토한다. 같은 Actor 풀 재사용의 직접 추적·물리 끔 기준을 새로 둔 성능 비교는 하지 않았다.
- 기존 적 Mover 0-Up 경고와 NPC 손의 큰 물리 추종 지연은 별도 미해결/허용 범위로 남긴다. 상세 근거·수치는 `history/Phase03_Log.md`, `research/PhysicsHitReactSpike.md` §9·§10. 다음 갈래는 `Current.md`에서 선택한다.

