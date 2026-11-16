# Phase 16 로그 — Support 없는 Blocker 단차 금지와 ISM Support

> 상태: 진행 중
> 계획: `phases/Phase16_SupportlessBlockerStep.md`

## 2026-11-16 — 착수, 1단계 구현·픽스처

- 인계: 그래픽스 1-B5b(GX-014)의 Ridge WIP 바위 Static+Blocker 승격 뒤, 지상형 적의 바위 발 관통([그래픽스 실행서](../../Graphics/phases/Phase01_EnvironmentImport.md) §24).
- 원인 정밀화: exact 경로는 `ProbeGroundedAt`이 Blocker 윗면에서 이미 `Rejected`다. 관통은 캐시 경로에서만 생긴다(올린 측면 sweep → Atlas 지각 접지). Nav 베이크(지면+1cm 캡슐)와 배회 구간(+2cm 캡슐 sweep)은 낮은 Blocker를 이미 장애물로 본다.
- 사용자 결정: 판정 기준은 캐시 접지 후 겹침 검증(역할 조회), 위반 시 이번 프레임 정지, 검증은 픽스처 + WIP PIE. 대안과 기각 사유는 Phase 문서 §3.1에 있다.
- 구현:
  - `ULNPHitIdentitySubsystem::IsSupportlessBlocker`: Blocker 역할이고 Support 역할·Support face 표·patch face 표가 모두 없음
  - `ULNPMassWorldCollisionSubsystem::OverlapsSupportlessBlocker`: `LNPWorldExact` overlap-multi + 역할 필터, 새 query 분류 `GroundBlockerOverlap`(필수 분류)
  - `LNPEnemyExactMovement::OverlapsSupportlessBlocker`: 캡슐 반지름·반높이 −1cm(`BlockerOverlapSkin`)
  - `LNPEnemySurfaceMovement::StepGrounded`: 캐시 HighConfidence 뒤 위반이면 `Rejected`(제자리·속도0·handle 복원). 이동 없는 프레임은 생략, 출발 위치도 겹쳐 있으면 허용. `EnemyGround` 보고에 `blocker_rejects`
- 새 자동화 `LootNPop.SurfaceNavigation.EnemyMovement.SupportlessBlockerStep`(`Source/LootNPop/Tests/LNPEnemyBlockerStepTest.cpp`): 반지름 30000cm 내부형 구의 합성 지각 Atlas(캐시 경로)와 실제 exact 형상. 사례:
  1. 20cm Blocker 미통과·`Rejected` 발생·겹침 0
  2. 55cm(Moss 경계) 겹치기 전 정지
  3. 30cm Support+Blocker는 거부 0으로 통과
  4. 두께 5cm Support에 묻힌 캡슐도 Blocker 검출, Support만 있으면 거부 0
  5. 겹친 채 출발하면 빠져나감
  6. LootPod proxy 멈춤 거리 불변(구 중심에서 164cm ±3)·거부 0

  1번의 `Rejected`는 수정 전 캐시 경로가 낼 수 없는 결과라, 이 픽스처는 수정 없이는 통과하지 못한다.
- 검증:
  - 게임 타깃 Build.bat 성공. 첫 시도는 `OverlapMultiByChannel`이 inline allocator 배열을 받지 않아 실패했고, 일반 `TArray`로 고쳤다. adaptive unity 충돌을 피하려고 테스트 익명 namespace 이름을 파일 고유로 바꿨다.
  - Editor Build.bat 성공(사용자가 에디터를 닫음).
  - 새 픽스처 헤드리스 실행 **Success**. 로그 `Saved/Logs/BlockerStepTest.log`.
  - `Automation RunTests LootNPop.SurfaceNavigation` 전체 회귀: **136/136 Success**(기존 135 + 새 픽스처 1). 자동화 경고4는 외부 HTTP timeout3·B-option 안내1로, 기존과 같은 노이즈다. 로그 `Saved/Logs/SurfaceNavFull.log`.
  - 회귀가 다시 저장한 테스트 에셋3개(`SM_COptionSphereSculpt`·`SM_BOptionExtracted`·`DA_MinimalOctantSurfaceData`)는 git으로 되돌렸다.
  - 회귀에는 실제 production 옥탄트의 동굴 Shell 근처 이동이 없다. 그 동작 변화는 PIE와 gate에서 본다.
- 미검증: Ridge WIP PIE, Phase 13 Production gate 비용 비교, 동굴 Shell 근처 동작 변화, 리슨 2P(D-031). 커밋하지 않았다.

## 2026-11-16 — Ridge WIP 첫 PIE, 정지·추격 중단 조사

- TestMap03·1인·EnemyDensity1. 그래픽스 §22의 기존 검사용 풀을 메모리에서만 임시 선택했다. 실제 WIP는 slot5/6이며 8 validated slots·179 bindings·seam 높이 차0·차단 node/edge0을 확인했다.
- 사용자가 slot5 수동 바위 구역에서 확인했다. **바위 발 관통은 관찰하지 못했다.** 일부 NPC는 벽/구석에서 플레이어 쪽으로 회전하지만 정지했고, 다른 NPC를 제거해 수를 줄인 뒤에도 움직이지 않았다. 바위를 사이에 두고 지상 원거리 NPC와 거리를 벌렸을 때 우회 추격을 멈춘 경우도 있었다.
- 종료 전 보고 로그는 보존됐다. Reset 뒤 steps272,586·cache_hits224,267·exact_fallbacks48,319·cache_rate82.27%·blocker_rejects3,871. 전체 개체 합계라 특정 정지 NPC의 원인을 확정하는 근거는 아니다.
- 사용자가 PIE를 종료한 뒤 IsPIERunning=false와 원래 풀 복원 getter를 확인했다. SaveConfig 미호출, 설정2개·정식 풀·WIP·SurfaceData·검사용 풀의 SHA256 불변을 확인했다. 근거는 `Saved/Profiling/Phase16/RidgeWipPIE_20261116/`의 `FirstPIE.log`·`RuntimeInit.log`·`SelectionState.json`이다.
- 코드 조사 후보: 접지 거부가 추격 경로 재요청으로 연결되지 않는 경우, 경로 투영/도달 가능성 또는 슬롯 대기, 사거리 안 지상 원거리 NPC의 사선 차단 후 재배치 부재. 첫 관찰만으로 동일 원인이나 Phase16 회귀라고 확정하지 않는다.
- 다음 확인은 기존 `LNP.SurfaceNav.NavReport 0`의 근처10개/18m 이내 NPC 상태·endpoint·pathStatus·waypoint 로그와 `EnemyGround.Report`를 재현 중 대조한다. 새 진단 장치나 수정 코드를 만들기 전에 수동 재현을 요청한다. 1단계 PIE 통과·비용 비교·커밋은 보류하며 다음 구현 단위로 넘어가지 않는다.

### 두 번째 PIE — 벽 근처 지상 근접 NPC의 NoNode

- 실제 WIP slot4·8 validated slots·277 bindings·seam 높이 차/차단0. 사용자는 멈춘 개체가 모두 지상 근접이며 바위보다 벽 근처에서 재현된다고 구분했다. 재현 중 여러 번 NavReport·EnemyGround.Report를 남겼다.
- NPC729·731·733은 `DA_Enemy_PureEntity_Melee01`, slot4/layer0·surfaceValid1·airborneSpeed0·`endpoint=2(NoNode)`·`pathStatus=6(NoNode)`·waypoints0이다. NPC731·733은 05:14:19~05:16:12 UTC 사이 위치가 출력 정밀도에서 동일했다. NPC730도 다른 벽에서 NoNode로 보고됐다.
- 플레이어 node가 유효한 05:14:19(node1249635)·05:16:12(node1249821)에도 이 NPC의 endpoint는 NoNode였다. 이 경우는 `ResolveEndpoints`의 시작 후보가 없는 흐름으로 좁혀진다. 후보가 있는 근접 NPC가 Alert로 남는 것은 슬롯 수 외에 도달성 필터로도 생긴다.
- 05:14:34~05:16:21 동안 steps199,044→333,047인데 `blocker_rejects`는279로 고정됐다. 이 구간 정지를 새 Blocker 거부 반복으로 설명할 근거는 없다. 처음부터 Nav 밖에 있었는지, 이동 후 이탈했는지, 표면 정체성·베이크/투영 문제인지는 미확정이다.
- `LNPEnemyPathProcessor`는 시작 node가 없으면 현재 위치를 steering point로 삼고 즉시 반환한다. `ResolveEndpoints`도 시작 후보가 없으면 접근점을 만들지 않는다. 따라서 현재 위치에서 Nav 시작점을 못 찾으면 목표/슬롯 수가 바뀌어도 스스로 빠져나오는 경로가 없다.
- 근거: `Saved/Profiling/Phase16/RidgeWipPIE_20261116/RuntimeInit_Run2.log`·`Run2_Reports.log`. PIE와 임시 풀은 조사 중 유지하며 복원 상태는 `SelectionState_Run2.json`이 소유한다. 다음은 기존 DrawNav로 재현 위치의 Nav 공백 여부를 수동 확인한다. 원인 확인 전 스냅 반경 확대·순간 이동·재베이크를 적용하지 않는다.
- 후속 수동 확인: DrawNav에서 **NPC 주변만 비고 평지에는 점·선이 보임**. Cache0 비교(05:20:22~05:22:09 UTC)에서도 NPC731/733은 layer0·surfaceValid1·NoNode를 유지했고, 위치 변화는 각각 약0.002cm/1.1cm뿐이다. 사용자는 이동하지 않았다고 확인했다. 따라서 정지 후 캐시만 꺼서는 복구되지 않으며, 이 비교로 Nav 이탈 원인이 캐시와 무관하다고까지 확정하지 않는다.
- 두 번째 PIE 조사 종료: Cache1 getter 확인 → StopPIE → IsPIERunning=false → 원래 풀 getter 복원. 보호 파일6개 SHA256 불변. `SelectionState_Run2.json=restored`, 최종 로그 `Run2_Final.log`, 요약 `Run2_Verdict.json`에 보존했다. C++·지형·베이크 변경 없음.
- 다음 제안 단위(사용자 진행 확인 전): 지상 NPC가 Nav 시작점을 처음 잃는 시점의 계측으로 **스폰 시점 부재 / 일반 이동 / 분리력 / 공중 착지**를 구분한다. 이전 유효 node와 이전/현재 위치·표면·이동 결과를 남기고 같은 WIP에서 재현한다. 원인이 확인된 뒤 실패 재현 검사와 최소 수정을 결정한다. 원거리 사선 재배치는 별도로 남기며 아직 구현하지 않는다.
- 추가 사용자 관찰: 해당 NPC들은 원래 정상 이동·플레이어 추격을 했으며, 군집 추격 중 서로 밀어내다 벽 근처로 밀려났을 가능성을 제시했다(분리력 원인은 사용자 추측). 따라서 스폰 시점 Nav 부재보다 **정상 추격 중 Nav 이탈**을 우선 조사한다. 코드에서도 지상 일반 이동 속도에 분리력을 더한 뒤 Support/exact로 접지하지만 최종 위치의 Nav 시작점 유효성은 검사하지 않는다. 분리력으로 Nav 범위 밖에 밀린 뒤 NoNode 정지로 이어지는 고정 재현 검사를 우선 제안하며, 실제 사례의 이탈 원인이 입증된 것으로 확대하지 않는다.

## 2026-11-16 — 군집 분리력 Nav 이탈 재현·수정 단위

- 사용자가 재현 검사 → 최소 수정 → 벽 근처 PIE 재검증 단위를 승인했다.
- 새 자동화 `EnemyMovement.SeparationNavBoundary`는 기존 `LNPEnemyBlockerStepTest.cpp`의 시험 월드/Support를 재사용한다. Support는 이어지지만 Nav는 좁은 통로에만 있는 합성 지형을 만들고, 옆 방향 추격 의도에 경계 바깥을 향한 분리력을 더한다. 실제 경로 소비자처럼 NoNode가 되면 의도를 지운다.
- 수정 전: 기존 `StepGrounded`가 정상 Nav에서 출발한 개체를 Nav 밖으로 밀어낸다. 새 실제 이동 호출의 기준 동작도 같게 둔 상태에서 검사를 실행했고 Nav 유지·경계 방향 추격·압력 해제 후 복귀 **3항목 실패**를 확인했다. `SeparationNavBoundary_Red.log`에 보존했다. 이는 분리력 가설의 고정 재현이며 첫 PIE의 실제 이탈 프레임을 캡처한 것은 아니다.
- 수정:
  - `StepGroundedWithSeparation`이 의도·분리력을 합쳐 기존 cache/exact 접지를 호출한다.
  - Grounded 결과가 정상 Nav 출발점의 기존 300cm 투영 시작점을 잃으면, 이전 Surface handle을 복원하고 분리력을 뺀 원래 의도로 한 번 재시도한다. 그 결과도 시작점을 잃으면 위치·속도·handle·identity를 복원하고 Rejected다.
  - 기존 경로의 `StartHintNode`가 같은 slot/Layer·막히지 않은 node이고 결과 발 위치의 300cm 안이면 거리 확인으로 통과한다. 이 증명이 안 될 때만 최근접을 찾으며, 정상적인 분리력 없는 이동은 추가 Nav 조회를 하지 않는다.
  - 이미 Nav 밖에서 출발했으면 안쪽으로 빠져나가는 이동을 새 검사로 막지 않는다. 자동 복구/순간 이동을 넣은 것은 아니다. 공중·이동 패널 규칙은 기존 경로가 소유한다.
  - `EnemyGround.Report`에 `nav_separation_retries`를 추가했다. `steps`는 재시도까지 포함한 접지 호출 수다.
- 현재 검증:
  - Live Coding 성공. 실제 PureEntity 이동 프로세서가 새 호출을 사용한다. 전체 Editor/Game 디스크 빌드는 이번 추가 수정 뒤 아직 실행하지 않았다.
  - 새 검사 + 기존 `SupportlessBlockerStep`: **2/2 Success, 오류/경고0**. 새 검사는 캐시1/0 양쪽의 Nav 유지·Nav 안 분리력 유지·경계 방향 추격·압력 해제 후 복귀·Nav 밖 출발의 탈출 허용을 확인한다. 정밀 바닥을 production slot 바인딩에 넣은 첫 보완 픽스처는 등록에 실패했고, 런타임 시험 source/face map으로 바로잡은 최종 검사다.
  - `SeparationNavBoundary_Final.json`에 최종 세부 결과를 보존했다. 전체 SurfaceNavigation 회귀를 실행 중이며, 이전 회귀가 재저장했던 에셋3개는 `RegressionAssets/`에 사전 복구 사본·SHA256를 확보했다.
- 전체 회귀 최종 결과: **137/137 Success, 실패0**. MCP의 실행 대기 호출은300초에 timeout됐으나 에디터의 기존 실행은 계속됐고, 종료 뒤 GetTestStatus/GetTestResults로 최종 결과를 회수했다. 경고4는 RHI 가상 리소스 예산1·기존 GroundedFixture의 SetByCode 우선순위 무시2·B-option 안내1이다. 새 검사(캐시1/0)는 기존 우선순위를 보존해 경고0으로 통과했다. 세부 결과 `SurfaceNavFull_Separation.json`·전체 로그 `SurfaceNavFull_Separation.log`.
- 회귀 재저장 에셋3개의 원복은 에디터 파일 잠금이 있어 종료를 요청했다. 복구 사본/원래 SHA256와 진행 상태는 `RegressionAssets/Before.json`·`RestoreState.json`이 소유한다. 종료 뒤 원복·정식 Editor/Game 빌드·관련 헤드리스 검사를 이어간다.
- 에디터 종료 후 테스트 에셋3개를 사전 SHA256와 동일하게 원복했다. `BP_Octant_COption`의 미저장 변경은 회귀용 Blueprint의 메모리 변경이라 사용자가 저장하지 않고 종료했다. Content/Config git 변경0, INI2개·정식 풀·WIP·SurfaceData·검사용 풀의 보호 해시6개 불변을 확인했다.
- 추가 수정의 정식 디스크 빌드도 **Editor/Game 모두 Success**. `SeparationGuard_EditorBuild.log`·`SeparationGuard_GameBuild.log`에 보존했다. 이어 정식 Editor 바이너리로 헤드리스 `EnemyMovement`를 실행해 **7/7 Success, 자동화 오류/경고0, 종료 코드0**을 확인했다. 기존 GroundedFixture도 이번에는 CVar 우선순위 경고 없이 통과했다. 로그/요약은 `EnemyMovement_Separation_Headless.log/.json`이다.
- TestMap03으로 에디터를 다시 실행했다. 다음은 수정본 Ridge WIP 군집 추격 수동 재검증이다.
- 수정본 세 번째 PIE를 시작했다. TestMap03·1인·밀도1·캐시1, 실제 Ridge WIP slot5/6·8 validated slots·195 bindings·seam 높이 차/차단0. `SelectionState_Run3.json=active_memory_override`이며, 검사용 풀을 메모리에서만 임시 선택했다. 보호 파일6개 해시 불변을 다시 확인했다. `RuntimeInit_Run3.log`에 초기화를 보존했고 사용자 군집 추격 검토를 요청한다. 종료 뒤 원래 풀 복원이 필요하다.
- **세 번째 PIE 수동 미통과:** 사용자는 벽 근처 정지가 여전히 발생한다고 보고했다. 이번1회 체감으로는 이전보다 줄었지만 확정은 아니라고 했다. 벽에 붙어 있는 플레이어를 근접 적들이 제대로 추격하지 못하는 문제도 지적하며 실제 보행 가능 경사의 Nav 커버리지를 검토해 달라고 했다.
- Run3 보고: NPC594(근접)는 player node1386341 유효 상태에서도 endpoint2(NoNode)·pathStatus0·waypoints0·Alert다. NPC592는 endpoint0(Reachable)이지만 이전 NoNode 상태의 접근 경로2개/waypoint2를 유지한다. `blocker_rejects=1,682`·`nav_separation_retries=196`. 재시도는 실제 작동하지만 이 수치만으로 정지 원인 해소를 보장하지 않는다. 로그 `Run3_WallStop.log`.
- 이번 분리력 검사는 정상 Nav→분리력→이탈이라는 합성 조건을 해결한 범위다. 실제 벽 정지 해결로 확대하지 않으며, 일반 의도만의 이동·배회·착지 등 다른 이탈 경로와 플레이어 벽 근처의 Nav 목표 문제를 더 조사해야 한다.
- 관련 코드 확인: 베이크는 경사(dot0.71) 외에 반지름50/반높이88cm 중력축 캡슐의 node overlap과 연결부 연속 sweep을 요구한다. 지각 격자는 목표200cm다. `ULNPPlayerNavProcessor`는 실제 바닥 identity를 찾았어도 300cm 안의 Nav node가 없으면 `Surface`까지 무효로 남기며 `GroundPoint`도 갱신하지 않는다. 가까운 벽에서 플레이어의 Nav 투영 실패가 근접 도달성/슬롯/접근점 경로에 함께 전파되는 구조를 별도 검토한다.
- 미검증: 수정본 Ridge WIP 군집 추격 PIE, Production gate 비용 비교, 리슨2P. 원거리 사선 차단 뒤 재배치는 이 단위에 넣지 않는다. 커밋하지 않았다.

## 2026-11-16 — 최종 사용자 결정·새 세션 인계

- 사용자는 분리력 제한을 좋은 해결책으로 보지 않으며, **"NPC들이 여럿 몰려있을때 NPC 끼리 겹치는 문제가 더 강해져선 안돼"**라고 결정했다. 분리력을 빼거나 약화·취소해 Nav 경계를 지키는 실험은 기각한다(D-083). 실제 군집 겹침이 증가했다고 계측한 것은 아니다.
- 사용자는 **1) 실제 보행 가능한 벽 주변 영역의 Nav 커버리지 수정,2) 플레이어 물리 지면/Nav 투영 분리와 근접 적의 도달 가능한 접근·공격 위치 처리 모두 필요**하다고 결정했다. 근본 해결을 목표로 하며 다음 구현은 새 세션에서 진행한다.
- 이번 인계에서는 문서와 검토환경만 정리했다. `StepGroundedWithSeparation`·호출·`nav_separation_retries`·전용 검사 기대값 등 비채택 실험은 소스/빌드 바이너리에 미커밋 상태로 남아 있다. 새 세션 첫 단위에서 해당 차분을 정리하고, 기존 D-082 Blocker 구현·픽스처와 다른 미커밋 변경은 보존한다. 이전137/137·7/7 결과는 비채택 구성을 포함하며 새 근본 수정의 검증을 대체하지 않는다.
- 세 번째 PIE 종료 → IsPIERunning=false → 원래 `/Game/Maps/OctantPoolData` getter 복원을 확인했다. `SelectionState_Run3.json=restored`·`Run3_Final.log`로 종료 상태를 보존했고 보호 파일6개 SHA256 불변을 재확인했다. 에디터는 TestMap03으로 열려 있으며, 새 세션은 실제 WIP slot을 새 로그에서 읽는다.
- 인계의 단일 실행 계획·수용 기준·새 세션 프롬프트는 `phases/Phase16_SupportlessBlockerStep.md` §3.5~3.7이다. 분리력 제한의 기각 근거는 `../DiscardedApproaches.md` Case09, 결정 원장은 D-083이다. 구현 단위마다 다음 진행을 확인하며 수동 PIE가 더 효율적이면 즉시 요청한다. 커밋·비용 비교·ISM Support 착수는 하지 않았다.

## 2026-11-16 — 사용자 요청의 작업 체크포인트 커밋

- 사용자가 지난 작업부터 남은 변경 전체를 커밋하고 최근 프로젝트 메시지 양식을 따르도록 요청했다. 한국어 `fix:` 제목과 변경·검증·남은 작업을 설명하는 불릿 본문으로 현재 상태를 보존한다.
- 범위는 D-082의 Blocker 비관통 구현·픽스처, 비채택 분리력 제한 실험의 소스/검사, D-083 기각 결정과 두 근본 수정 인계 문서다. 비채택 실험도 포함된 체크포인트이며 벽 정지 해결·Phase16 완료로 취급하지 않는다.
- 다음 세션 첫 작업인 비채택 실험 정리와 실제 보행 Nav 커버리지·플레이어 물리 지면/Nav 투영 및 근접 목표 수정은 유지한다. 현재 문서의 미커밋 상태 표현을 정리했으며, 앞선 로그의 미커밋 기록은 당시 상태다. 코드·에셋을 추가로 변경하거나 새 빌드를 실행하지 않았다.
