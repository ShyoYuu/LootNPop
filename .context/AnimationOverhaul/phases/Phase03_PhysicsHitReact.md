# Phase 3 — 물리 히트리액트 본 구현

> 상태: 진행 중(3-1 완료 2026-10-21, 다음 3-2)
> 선행: Phase 1A(RBWC 노드 방식 확정, `research/PhysicsHitReactSpike.md`), Phase 2 완료(최종 포즈 파이프라인 확정 — 표시 ABP의 왼손 IK 뒤·출력 앞이 RBWC 자리)
> 순서: 3-1 → 3-2. 단위 하나가 세션 하나이고, 단위마다 커밋하고 사용자 확인을 받는다.
> 범위: 플레이어·`ActorPromoted` 적의 `VisualMesh` 상체 물리 반응(코스메틱·비복제, AN-004). 경직(GA_Stagger)·다운·Light/Heavy/Parried 경직 몽타주는 건드리지 않는다. 파워드 래그돌(AN-005)과 `PureEntity`(ISKM)는 범위 밖.

## 단위 나눔

| 단위 | 내용 | 완료 조건 |
|:---|:---|:---|
| **3-1** 경로·방향성 | 두 표시 ABP에 RBWC 노드 배치, 큐 → `PlayHitReact` → 임펄스 경로 본 연결(방향·세기·맞은 지점), 방향성 후보 3안 비교·선택, 스파이크 산출물 정리. 기존 히트리액트 몽타주는 유지하고 물리를 덧붙인다 | 아래 3-1 체크리스트 |
| **3-2** 몽타주 대체·적 분리·남은 확인 | 3-1 방향성 결과를 보고 사용자가 정한 몽타주 대체 범위 실행(`CHT_Montage` HitReaction 행 정리 포함), 적 반응 세기 차이(`PA_UEFN_Mannequin`) — 메시별 세기 또는 PhysicsControl 프로필 분리, 3-1에서 남긴 확인 항목(URO·LOD로 1프레임 임펄스 소실 등), 방향·무기별 곡선이 필요하면 GASP B11식 애니 곡선 → 알파·강도 | 3-2 착수 때 구체화 |

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
- [ ] 커밋 뒤 깨끗한 트리에서 `Build.bat` 재확인
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

## 단위 3-2 — 몽타주 대체·적 분리·남은 확인 (착수 전, 결정 필요)

3-1 결과로 정해야 할 것(착수 전에 사용자 결정):

- **결정 C — 몽타주 대체 범위.** ① 가벼운 히트리액트 몽타주(`CHT_Montage` HitReaction 행)를 물리로 대체(사용자 3-1 평가로는 가능) ② 몽타주 유지 + 물리 덧붙임(현재) ③ 무기·방향별로 섞기. 경직 몽타주는 어느 쪽이든 유지(AN-004).
- **결정 D — 방향성 보강을 3-2에 넣을지.** 물리 단독으로 부위·방향 체감이 약한 원인은 구조다(골반·다리 키네마틱 + 부모 공간 각도 컨트롤만이라 몸 전체가 밀리거나 반 걸음 물러나는 성분이 없다). 후보: 골반에 약한 월드 공간 컨트롤을 두고 피격 때만 풀기, 몸통만 담는 세트를 따로 만들어 척추만 무르게(지금 세트는 온몸), 유지 시간 늘리기, 방향별 가산 기울기를 절차적으로 더하기.

그 밖의 3-2 범위: 적 반응 세기 차이(`PA_UEFN_Mannequin` 바디 구성 — 메시별 세기 또는 PhysicsControl 프로필 분리), URO·LOD로 평가가 건너뛰어질 때 1프레임 임펄스 소실 여부, 대체 시 몽타주가 하던 일(왼손 IK 알파·가산 슬롯) 확인.

