# Phase 3 작업 로그

> 소유 정보: Phase 3(물리 히트리액트 본 구현)의 날짜별 수행 내용·검증 결과·시행착오. 현재 유효한 단위 계획은 `phases/Phase03_PhysicsHitReact.md`, RBWC 설정·방향성 비교 결과 원본은 `research/PhysicsHitReactSpike.md`가 소유한다.

## 2026-10-21 — 단위 3-1 경로·방향성

LootNPop 에디터(8000)만 썼다. GASP 에디터는 쓰지 않았다.

### 1. 출발 상태 확인

- 두 표시 ABP의 부모는 `ULNPAnimInstance`다(왼손 IK·견착 보정 변수를 받는다). 스파이크 클래스 `ULNPPhysicalHitReactAnimInstance`는 `UAnimInstance` 파생이라 그대로 부모로 쓰면 그 변수가 사라진다 → 스파이크 클래스를 `ULNPAnimInstance` 파생으로 바꿨다(헤더 변경, 에디터 재시작 1회).
- `ABP_Retarget`: `RetargetPoseFromMesh → LocalToComponent → TwoBoneIK(hand_r 견착) → TwoBoneIK(hand_l) → ComponentToLocal → 출력`. `ABP_VisualCopyPose`: `CopyPoseFromMesh → LocalToComponent → TwoBoneIK(hand_l) → ComponentToLocal → 출력`. 컴포넌트 공간 구간이 이미 있어 RBWC는 왼손 IK와 `ComponentToLocal` 사이에 바로 들어간다.
- 큐 `GameplayCue.LNP.Character.HitReact`는 이미 `Location`=맞은 지점(`FLNPApplyDamageGECommand`의 `ImpactPoint`)을 싣고 있었다. 핸들러가 `Normal`만 읽고 있었다.
- 두 PhysicsAsset 바디: `PA_Mannequin`(MoverSample, 22개)은 `spine_01`이 없고, `PA_UEFN_Mannequin`(23개)은 `spine_01`·`spine_02`가 모두 있다. 지점 임펄스 후보(`spine_03~05`·`head`·`upperarm_l/r`)는 양쪽에 다 있다.
- 피해 분포(최근 PIE 로그 `[GE] HP`): 20.0이 대부분, 6.0 일부 → 기준 피해 20.

### 2. 엔진 사실(방향성 설계 근거)

- `ApplyForceAndTorque`(`RigidBodyWithControl.cpp`): `LocationSpace=World`면 `AddForceAtLocation`, 각성분은 `AddTorque`로 따로 들어간다. Chaos `FActorHandle::AddForceAtLocation`의 `AddVelocity`(VelocityChange)는 지점 속도 변화를 맞추는 충격량을 계산해 **각속도도 함께 준다**(`CalculateForceForAccelAtPoint`) → 지점 임펄스로 비틀림·젖힘이 생긴다.
- 컨트롤 강도 단위는 Hz(ω₀ = 2π×강도). 스파이크 30Hz면 ω₀ ≈ 188rad/s라, 임계 감쇠에서 최대 변위 ≈ v₀/(ω₀·e)다. 가슴 선속도 250cm/s(레버 약 40cm → 약 6rad/s)는 관절당 1° 안팎이다. "움찔은 있지만 밀리지 않는다"는 평가와 맞는다. 강도를 낮추는 후보 3이 변위를 직접 키우는 레버다.
- RBWC 림 셋업은 컨트롤을 림 이름(`Torso`), 종류(`ParentSpace`), 둘의 조합(`ParentSpace_Torso`) 세트에 넣는다(`PhysicsControlOperatorNameGeneration.cpp`). `ControlAndModifierParameters`는 세트 이름을 펼치고, 핀을 비우면 원래 값으로 돌아간다.

### 3. 구현

- C++: 애님 인스턴스 부모 변경, `QueueHitReact(밀 방향, 맞은 지점, 피해, 근접)`, 방향성 모드 비트 CVar, `HitControlParameters` 핀, 랙돌 중 피격 버림. `PlayHitReact` 시그니처 확장(블루프린트 호출처 없음), 큐 핸들러가 `Location`·`RawMagnitude`·`NormalizedMagnitude`를 넘긴다. `FLNPApplyDamageGECommand`의 `bSuppressHitReact` → `ELNPHitReactKind`(근접 호출 2곳 Melee, 화염·화상 3곳 None, 나머지 기본 Ranged). 스파이크 CVar `LNP.Anim.PhysHitReactSpikeABP`와 `BeginPlay` 교체 코드 삭제.
- 빌드: 게임 타깃(에디터 켠 채) 통과 → 사용자 에디터 종료 → 에디터 타깃 `-NoHotReloadFromIDE` 통과(153초) → 재시작.
- ABP(MCP): 두 표시 ABP `set_parent` → `create_node`(`Animation|Dynamics|RigidBodyWithControl`) → 스파이크 노드의 `Node` 구조체를 `ProgrammaticToolset` 스크립트로 통째 복사(배열 포함 한 번에 성공, 필드 차이 0) → 왼손 IK·`ComponentToLocal` 사이에 연결, 변수 getter 3개(`Variables|LNP|PhysicalHitReact|Get*`)를 `Alpha`·`ControlAndModifierParameters`·`ForcesAndTorques`에 연결 → 경고를 오류로 취급한 컴파일 2/2 통과 → 저장. 스파이크 사본은 참조자 0 확인 뒤 폴더째 삭제.
- 자동 PIE 스모크: 경고는 기존 Iris `BroadcastLoadedModulesUpdated` 1건뿐(이전 세션 로그에도 있음). SlateInspector로 상태 표시줄 Cmd에 CVar를 넣고 `GameplayCueToolset.ExecuteCueOnSelectedActor`로 플레이어에 HitReact 큐를 직접 실행 → `[PhysHitReact] BP_LNPPlayer_C_0 mode=15 strength=1.50 push=V(-1.00,-0.00,-0.03) … pointBody=spine_03`. 큐 → `PlayHitReact` → 힘 구성까지 이어진다(근접 1.5배, 접평면 투영, 가장 가까운 바디 선택). CVar는 기본값으로 되돌렸다.

### 4. 방향성 비교 ① — 몽타주 위 (사용자 PIE)

- 모드 1·2·4·9·10·12를 차례로 바꾸며 승격 적·플레이어 피격을 봤다. 로그상 6개 모드 모두 피격이 들어갔고, 모드 4·12의 지점 바디는 `spine_03` 25·`upperarm_l` 15·`upperarm_r` 8·`head` 5·`spine_04/05` 각 2로 갈렸다. 세기는 1.0(원거리 20)·1.5·3.0(근접 40)이 섞였다.
- 사용자: "큰 차이는 못 느끼겠다. 어느 방향으로 맞았냐에 따라 다른 모션이 재생되는 느낌은 모든 모드에서 있다. 전체적으로 자연스럽다." → 방향별 차이는 4방향 몽타주에서 나온 것으로 봤다. 30Hz 컨트롤에서 물리는 몽타주 위의 작은 떨림이라 모드끼리 구분되지 않는다.
- 사용자 제안으로 몽타주를 끄고 물리만 보기로 했다 → 모드 비트 16(히트리액트 몽타주 생략)을 `PlayHitReact`에 추가(Live Coding, 기존 CVar를 `FindConsoleVariable`로 읽어 헤더 변경 없음).

### 5. 방향성 비교 ② — 물리 단독

- 첫 시도에서 사용자가 "몽타주가 꺼진 게 맞나, 피격 모션이 항상 똑같다"고 했다. 자동 PIE로 큐를 직접 실행해 모드 28은 `hit react montage skipped`, 모드 12는 `played`임을 확인했다. 항상 같은 모션은 방향이 없는 경직(Light) 몽타주일 가능성이 높아, `LNP.Anim.PhysHitReactLog`가 켜지면 히트리액트·경직 몽타주 재생을 각각 한 줄씩 남기게 했다(`PlayHitReact`, 경직 큐). 피격 간격을 두고 다시 봤다.
- 재시도 로그: 플레이어 피격 125회 중 히트리액트 몽타주 생략 124(1회는 자동 확인), 경직 0. 모드 17·26·28, 이어서 과장값(강도 배율 0.1·젖힘 20·지점 800).
- 사용자: "가벼운 피격은 몽타주를 대체할 수 있겠다. 확실히 움찔거린다. 다만 맞은 부위·방향을 확실하게 체감할 정도는 아니다."
- 강도 낮추기가 실제로 걸리는지 `LogPhysicsControl` Verbose로 확인: `ParentSpace_Torso` 세트는 있다. 그런데 척추만이 아니라 팔·목·다리 컨트롤까지 담는다(pelvis에서 시작하는 Torso 림이 하위 전부를 가진다). 비트 8은 사실상 온몸 강도 배율이다.
- 기본값 결정: 모드 12, 강도 배율 0.1, 지점 임펄스 800(사용자가 마지막에 본 값). 에디터 세션에는 콘솔로 같은 값을 넣었다(정적 CVar 기본값은 Live Coding으로 바뀌지 않는다).

### 6. 확인 묶음 (사용자 리슨 2인 PIE, 약 13분)

- 앞 4.5분 모드 0(물리 끔), 뒤 8.8분 모드 12. `np.PrintReconciles 1`.
- 보정: 물리 끔 375회(84/분), 켬 531회(60/분). 물리가 보정을 늘리지 않는다(코스메틱·비복제 구조와 일치). 1A의 "켬/끔 비교 안 함" 숙제를 닫는다.
- 모드 12 구간 물리 반응 플레이어 246·승격 적 33, 히트리액트 몽타주 512·경직 13. 사망 4회 모두 10초 뒤 리스폰, 리스폰 폰(`C_2`·`C_3`)에서도 반응 기록.
- 사용자 육안: 근접 콤보·가드·패링 중 피격, HitStop 박자, 적도, 승격 적 랙돌 뒤 재사용, 캡슐·Mover, 왼손 그립 — "전부 이상 없음".
- 경고: `LogPhysicsControl`·ensure 0. 별건으로 14:48:28 게스트 롤백 직후 `LogMover: Ignoring the provided UpDirection (0,0,0)`가 `BP_LNPEnemy_C_21`에 3프레임 20회. 오늘 앞선 로그 3개에는 없다. 이 변경은 `VisualMesh` 포즈만 바꾸고 Mover에 닿지 않아 무관하다고 판단했다 — 적 Actor 재시뮬레이션의 Up 공급 쪽 후속 조사 거리다.

### 7. 커밋 뒤 빌드 재확인

- 커밋 b1039cc 뒤 깨끗한 트리(변경 파일 0)에서 게임 타깃 `Build.bat`(LootNPop 에디터가 켜져 있어 게임 타깃) 성공, 263초, 경고 0. 에디터 타깃은 같은 C++로 세션 중 `-NoHotReloadFromIDE` 빌드(153초)와 Live Coding 3회를 통과했다.

## 2026-10-22 — 3-2 결정 기록

- 사용자 결정: 결정 C는 "물리로 히트리액트 몽타주 완전 대체를 목표로", 결정 D는 "3-2에서 방향성 보강 시도"(AN-023). 이 세션은 3-1까지이므로 실행은 3-2 세션이 한다.
- 3-1 세션이 3-2 첫 실험(골반 밀림 비트 32, 척추만 무르게 비트 64)을 설계·구현해 게임 타깃 컴파일(74초)까지 통과시켰으나, 범위를 지키려고 코드는 되돌렸다. 설계 요지는 `phases/Phase03_PhysicsHitReact.md` "단위 3-2"에 옮겼다.

## 2026-10-22 — 3-2 첫 실험 구현·비교(116 선호 선정, 3-2 전체는 진행 중)

- 깨끗한 작업 트리에서 시작. LootNPop MCP(8000)로 두 표시 ABP의 RBWC 존재와 기존 핀 연결을 확인했다. 두 에셋 저장 성공 후 `Saved/AnimOverhaul/Phase3-2/backup/`에 사본을 뒀다. GASP 에디터는 불필요하다.
- 엔진 `RigidBodyWithControl.cpp`의 `OffsetInWorldSpace`는 입력 포즈 위치에 월드 이동을 더하고 월드 회전을 앞에 곱한다. 이를 쓰는 `HitKinematicTargets` UPROPERTY, 피격 시점 방향·강도·좌우값 보관, 애님 시간 기반 골반 이동·젖힘·비틀림, 척추 컨트롤만 강도 낮추기(64)를 C++에 추가했다. 기본 모드 12 유지.
- 비교표의 오류를 수정했다. 92/124는 비트 8·64를 동시에 포함해 8 우선 규칙상 척추 단독 비교가 안 된다. 28/60/84/116으로 고쳤다.
- `git diff --check` 통과. **컴파일·ABP 변경·PIE는 아직 하지 않았다.** 새 UPROPERTY를 반영할 전체 빌드 전 사용자에게 에디터 저장·종료를 요청했다. 종료 확인 후 빌드, 재실행, 두 ABP의 골반 타깃 설정·핀 연결, 컴파일·저장, 사용자 비교로 이어간다. 3-2 완료·커밋 전이다.
- 사용자 에디터 종료 확인 후 권한 확장으로 `Build.bat LootNPopEditor Win64 Development D:/UnrealProjects/LootNPop/LootNPop.uproject -WaitMutex -NoHotReloadFromIDE` 실행. UHT(2개 생성 파일)·컴파일·링크 6/6 성공, `Result: Succeeded`, 63.92초. 사용자 에디터 재실행을 요청했다.
- 사용자 재실행 확인 후 MCP로 두 표시 ABP에 `GetHitKinematicTargets` getter 생성 → RBWC `KinematicTargets` 연결. 초기 모디파이어의 `pelvis` 항목에서 `kinematicTargetSpace`만 `OffsetInWorldSpace`로, `bEnableKinematicTargetSpace`만 true로 변경했다. 전후 `Node` 전체 구조체와 입력 핀을 대조해 두 필드·새 연결 이외 차이 0 확인. 컴파일(경고=오류) 2/2·두 에셋 저장 성공.
- 사용자에게 직접 PIE에서 `LNP.Anim.PhysHitReactLog 1`, 모드 28/60/84/116을 비교하고 플레이어·승격 적의 방향성·골반/다리 변형·선호 모드를 알려 달라고 요청했다. 경직 몽타주가 끼지 않게 피격 간격을 두도록 안내. **체감 판정·기본값 선정·몽타주 제거·적 세기 분리·URO/LOD 확인은 아직이며, 다음 구현 착수 전 사용자 확인을 받는다.**
- 첫 사용자 PIE에서 84·116에 `SetRecordParameters: Failed to find control with name pelvis_spine_01 / spine_01_spine_02` 경고가 반복됐다. 없는 컨트롤을 조용히 건너뛴다는 최초 설계의 가정이 틀렸다. 두 PA의 컨트롤 합집합을 그대로 공급한 것이 원인이다. 각 후보의 부모·자식 바디 존재와 `FindParentBodyIndex`가 실제 부모 바디와 일치하는지 검사해 유효한 이름만 공급하도록 수정했다. 플레이어의 `pelvis_spine_02`와 UEFN의 `pelvis_spine_01`·`spine_01_spine_02`를 서로 잘못 보내지 않는다.
- 헤더 변경 없이 Live Coding `Result: Success`·`Live coding succeeded` 확인. 사용자에게 84·116 피격 재확인(경고 소멸·체감 차이)을 요청했다. 수정 후 PIE 검증은 대기 중이다.
- 수정 후 사용자 PIE 로그에서 84 피격 59회·116 피격 135회 동안 `SetRecordParameters: Failed` 0건 확인. 사용자가 기존 히트리액트 몽타주 생략 여부를 재질문해 비트 16 처리 코드와 최신 84 로그의 `hit react montage skipped`를 확인했다. 경직 몽타주는 별도 경로로 유지되며, 사용자 체감 판정은 아직 대기 중이다.
- 사용자 요청: 물리 단독 비교 동안 경직 몽타주도 끄고 이번 세션 종료 때 복구. 경직 큐에 임시 CVar `LNP.Anim.PhysHitReactSkipStaggerMontage`(기본 true)를 추가해 Light·Heavy·Parried 몽타주 재생만 생략하고 로그도 `skipped`로 구분했다. 경직 어빌리티·판정·입력 제한은 변경하지 않았다. Live Coding 성공. **비교 후 임시 스위치·분기·변경 로그를 제거하고 Live Coding으로 복구할 의무가 남아 있다.**
- 사용자 최종 평가: **"116이 제일 나은거 같아."** 경직 몽타주를 끈 최종 비교 구간에서 116 피격 60회·경직 몽타주 생략 4회, 컨트롤 경고 0건을 로그로 확인했다. 체감 비교 결과 원본은 `research/PhysicsHitReactSpike.md` §8.
- 비교 종료 후 약속대로 경직 큐의 임시 CVar·생략 분기·변경 로그를 제거했다. `LNPGameplayCueNotify_Stagger.cpp` 작업 전 대비 차이 0, Live Coding `Result: Success`·`Live coding succeeded` 확인. **경직 몽타주 복구 의무 완료.** HitReact 몽타주 경로·기본값 12는 아직 유지하며, 다음 구현(116 구성 기본 적용·HitReact 몽타주/Chooser 정리) 전 사용자 확인을 받는다. 적도·2인 회귀·골반/다리 변형에 대한 개별 통과 응답은 없으므로 확대 해석하지 않는다.

### 사용자 승인 후 — 기본 적용·HitReact 몽타주 제거

- 사용자 "응 진행해줘"로 후속 구현 승인. `PlayHitReact`의 4방향 태그 분류·몽타주 재생·생략 비트 분기를 제거하고 표시 애님 인스턴스 물리 예약만 유지했다. 피격자 HitStop 경로는 그대로다. 기본 모드는 100(4+32+64)이며 선택한 116에서 이제 불필요한 몽타주 생략 비트 16만 빠졌다. 관련 헤더 주석·Chooser 문맥 예제·큐 태그 설명을 수정했다.
- Chooser는 GUI 조작이 더 효율적이므로 사용자에게 직접 삭제를 요청했다. 삭제 전 저장·`Saved/AnimOverhaul/Phase3-2/backup/CHT_Montage.uasset` 백업 후 사용자가 HitReaction 10행 삭제·저장 완료. MCP로 4개 컬럼 모두 26→16행, 삭제 전의 유지 행 인덱스 `[0..7,18..25]`와 조건·컬럼 설정 완전 동일, `is_dirty=false` 확인. Stagger 3행·Dash·공격·패링·가드·재장전 행은 유지됐다.
- C++ Live Coding `Result: Success`·`Live coding succeeded` 확인. 왼손 IK 차단은 `DefaultSlot`·`FullBody`·`UpperBody`와 `DisableLHandIK` 커브로, 기존 가산 피격 슬롯은 차단 대상이 아니었음을 확인했다. 슬롯 그래프·소스 몽타주 에셋은 수정하지 않았다.
- 사용자에게 PIE에서 모드 100·로그 1을 명시 설정하고 일반 피격 물리 반응·경직 몽타주 복구·공격/재장전 왼손 그립을 확인하도록 요청 → **"문제 없어"** 응답. 최신 Live Coding 이후 모드 100 피격 74회·경직 몽타주 재생 2회·경직 생략 0회·HitReact 몽타주 로그 0회·PhysicsControl 경고 0건 확인.
- 이 구현을 마무리하고 남은 적 세기·URO/LOD 등의 다음 구현 전 다시 사용자 확인을 받는다. 3-2 전체 완료·커밋은 아직이며, 적도·2인·골반/다리 변형 확인도 마지막 검증 묶음에 남긴다.

### 중간 커밋 범위

- 사용자 요청으로 3-2의 골반·척추 방향성 보강, 기본 모드 100, HitReact 몽타주 호출·Chooser 10행 제거, 없는 컨트롤 경고 수정, 관련 문서·표시 ABP를 중간 커밋 범위로 정리했다. 경직 몽타주 임시 차단 코드는 제거된 상태다. `git diff --check` 통과. 검증 근거는 위 전체 빌드·ABP 컴파일·Live Coding·사용자 PIE 기록이다.
- 남은 작업: 적 메시의 반응 세기 분리 필요성 확인·필요시 조정, URO/LOD 평가 누락 때 1프레임 임펄스 소실 확인·필요시 수정, 새 기본값으로 적도·2인·랙돌/표현 풀·골반/다리 변형 최종 회귀, 마지막 빌드·문서/Roadmap 정리. 방향/무기별 곡선은 기존 반응이 부족할 때만 검토한다. 3-2 전체 완료로 표시하지 않는다.
