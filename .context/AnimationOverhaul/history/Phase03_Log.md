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
