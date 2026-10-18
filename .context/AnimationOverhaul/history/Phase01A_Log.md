# Phase 1A 작업 로그

> 소유 정보: Phase 1A(물리 히트리액트 스파이크)의 날짜별 수행 내용·검증 결과·시행착오. 현재 유효한 결론은 `research/PhysicsHitReactSpike.md`가 소유한다.

## 2026-10-18 — 스파이크 수행

### 수행

1. 필수 문서와 TechDesign_CharacterMovement §9(랙돌), CombatAnimation §6.4, Poise 문서를 읽었다. 코드에서 `EnterRagdoll`/`ExitRagdoll`/`TickRagdollGravity`/`PlayHitReact`/`ApplyHitStop`을, MCP로 `BP_LNPPlayer`의 `VisualMesh`를 확인했다(`ABP_Retarget`, `NoCollision`, 태그 `RTG_Lyra_to_MS`). `ABP_Retarget`은 `RetargetPoseFromMesh → Output` 두 노드뿐이다.
2. 엔진 소스 `Plugins/Animation/PhysicsControl`로 두 방식을 비교했다. PhysicsControl 플러그인은 `.uproject`에 없었지만 기본 활성 `ControlRigModules`가 의존해 이미 로드돼 있었다. RBWC 기본 중력이 월드 -Z(`MovementComp->GetGravityZ()` 또는 `World->GetGravityZ()`)라는 점, `ForcesAndTorques` 핀이 세트·본 이름 단위 VelocityChange를 받는다는 점을 확인했다. GASP `PCA_SandboxCharacter`는 8001에서 읽기 전용으로 읽었다(`physicalAnimation`: 부모 공간 각 15Hz·감쇠비 3, 월드 공간 3Hz, 중력 배수 0).
3. RBWC 노드 방식으로 결정했다(근거는 research §1). C++ 훅(`ULNPPhysicalHitReactAnimInstance`, `PlayHitReact` 임펄스, 애님 클래스 교체 CVar)을 쓰고 게임 타깃 `Build.bat`로 컴파일을 확인했다.
4. `ABP_Retarget`을 `/Game/Characters/PhysHitReactSpike/ABP_Retarget_PhysHitSpike`로 복제했다. MCP로 변환 노드 2개와 RBWC를 넣고 노드 값을 썼다. 신규 UCLASS라 Live Coding을 쓸 수 없어, 마스터 세션에 알린 뒤 사용자가 에디터 풀빌드·재시작을 했다. 이어서 부모 변경과 핀 연결을 했다.
5. 1인 PIE 스모크(MCP로 직접 실행): CVar로 애님 클래스 교체 확인, `GameplayCue.LNP.Character.HitReact` 직접 실행, `LNP.Debug.KillPlayer` 랙돌·리스폰. 첫 실행에서 `Failed to find modifier with name Leg_L/Leg_R` 경고를 보고 본 이름으로 바꿨다(research §5.1).
6. 사용자 2인 PIE 1차: "Actor NPC와 플레이어 모두 술 취한 것처럼 흐느적거린다, 피격과 무관하게 달리고 점프하고 훅 앵커만 타도 물속 해파리 같다." `LogPhysicsControl` Verbose로 컨트롤이 정상 생성됐음을 확인했다. 원인은 알파 상시 1로 판단했다(research §5.2). 피격 구간 알파(유지 0.15초 + 페이드 0.35초)와 각강도 30Hz로 바꿨고, 헤더 멤버 추가라 사용자가 다시 풀빌드했다.
7. 사용자 2인 PIE 2차: 해파리 현상 사라짐. 호스트·게스트 모두 적에게 맞으면 움찔하며 반응한다. 승격 적도 반응하지만 플레이어보다 약한 느낌이다. 적도 문제 없음, 튐·되감김 없음, 사망 랙돌 → 리스폰 정상, 공격자 HitStop 정상. 공격 반대쪽으로 밀리는 방향성은 부족하다.

### 결과

- 완료 조건 전 항목 충족. 적 풀 재사용은 로그(`BP_LNPEnemy_C_0` NetID 910 → 2775)로만 확인했다. 재검증 세션의 `LogPhysicsControl` 경고·ensure는 0건이다.
- 기존 몽타주 경로(`LNPGameplayCueNotify_HitReact` → `PlayHitReact`)는 그대로이고, 물리는 그 위에 덧붙였다. 스파이크 ABP를 켜지 않으면 동작 변화가 없다.

### 시행착오·주의

- `Saved/Logs`의 `LogMass: Error: Trying to extract CharacterMovementComponent from BP_LNPPlayer_C_0 failed`, `LogMover: Simulation start location disagrees` 경고는 10-16 로그에도 있던 기존 노이즈다.
- 1차 2인 세션의 `np.PrintReconciles` 보정 70건은 받은 값과 로컬 값의 `MoveInput`부터 달랐다(입력 스트림 89프레임 오프셋). 스파이크와 무관하다고 판단했지만 켬/끔 비교는 하지 않았다.
- 사용자 결과 메시지가 본문 없이 온 적이 있다. 이때는 로그로 세션 구간(`PIE: Server/Client logged in`)을 먼저 찾고, 화면 판정 항목은 다시 요청했다.
- MCP 배열 크기 변경 제한, `bEnableControls` 기본 false, Verbose 전용 생성 로그는 research §5.3.
