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

## 2026-11-16 — 새 세션: 실험 정리, 단위1 원인 진단

- 비채택 분리력 제한 실험 제거: `StepGroundedWithSeparation`·프로세서 호출·`nav_separation_retries`·`SeparationNavBoundary` 검사. 이동 프로세서 호출은 D-082 이전과 동일하고, D-082 Blocker 거부·`SupportlessBlockerStep` 픽스처는 보존했다. 게임 타깃 빌드 Success(`Saved/Logs/Phase16_Cleanup_GameBuild.log`).
- NoNode NPC 위치를 slot 회전으로 옥탄트 local로 환산하면 Run2 NPC729/731(slot4)과 Run3 NPC594(slot5)가 `LVI_Ridge_SurfaceProbe` local 약 (19.8~20.0k, 15.5~15.7k, 22.2~22.5k)의 같은 지형이다. NPC733·730은 다른 두 지점이다.
- 진단 명령 `LNP.SurfaceNav.ExplainNavGap <Level> <x> <y> <z> [r]`(에디터 전용, 에셋 불변)을 추가했다. 저장된 SupportPayload와 source LVI로 베이크와 같은 node 판정을 다시 계산하고, 후보마다 탈락 사유와 실제 SurfaceSupport 바닥·NPC 반지름35 캡슐 여유를 남긴다. Editor 빌드 Success.
- 결과(`Saved/Profiling/Phase16/NavGap/ExplainNavGap_Run1.log`): 다섯 지점 모두 바닥은 지형 메시(LNPStaticTerrain)이고 NPC 발밑 경사 dot0.74~0.97로 보행 가능하다. 반경400cm 후보 탈락 사유는 **Atlas 꼭짓점 NeedsExact가 대부분**(731: 8/9, 729: 8/8, 594: 7/8, 730: 9/10)이며, 그중 실제 바닥이 평탄(dot≥0.97)하고 NPC 캡슐이 서는 후보가 다수다. 가장 가까운 node는 731 380cm·594 345cm·729 없음으로, 런타임 투영 반경300cm에서 NoNode가 그대로 재현된다. 733은 NeedsExact5 외에 지형 바닥만 겹치는 clearance 탈락3(10cm 들면 통과)이 있다.
- 구조(수정 전): Nav node 위치는 payload에 저장하지 않고 런타임 그래프 구성 때 `QueryLayer`로 다시 만든다. `QueryLayer`는 세 꼭짓점 중 하나라도 NeedsExact면 보간을 거부하므로, Atlas의 정밀 경고(이웃 법선 25° 초과·가파른 이웃 선분)가 붙은 보행 바닥은 Nav에서 통째로 빠진다. 저폴리 능선의 꺾임 양쪽 띠가 여기에 해당한다.

## 2026-11-16 — 단위1 구현: Nav 전용 보간(D-084)

- 사용자 결정: Nav 전용 보간으로 진행(node 위치 저장 방식은 기각).
- 구현: `LNPSupportAtlas::QueryNavLayer`(세 꼭짓점 Valid·Walkable이면 NeedsExact여도 보간, 엄격 보간 성공 시 동일 결과). 소비처는 `LNPNavBaking::Build` node 생성(지각 이음매만 엄격 유지), `LNPNavGraph` 런타임 graph 구성, `LNPNavQuery` node 지면점, 베이커 Conditional Patch의 base node 위치다. 접지·스폰 앵커·이음매 대조·patch 겹침 검사는 `QueryLayer`를 유지한다. 베이커 schema 9→10.
- 새 자동화 `Nav.NeedsExactWalkableCoverage`: 합성 지각의 NeedsExact 보행 띠에 node 생성, 비보행 꼭짓점 인접 칸·지각 이음매는 제외, 엄격 성공 칸의 결과 동일, 런타임 graph 재구성 성공. 수정 전에는 띠 node가0이라 실패한다.
- 검증: Editor 빌드 Success. 헤드리스 `Nav.Baking`·`Nav.NeedsExactWalkableCoverage`·`Nav.MultiplePortals`·`Runtime.SupportPrecision` 4/4 Success(`Saved/Profiling/Phase16/NavGap/NavUnitTests.log`).
- WIP 재베이크(`BakeWip_NavInterp.log`, 원본 DA 백업 `NavGap/Backup/`, SHA256 341106d2…→a021e50c…): Support 표본 수 동일(Valid259,282·NeedsExact35,165), Nav cells73,835→75,950(+2.9%)·components109→115·portals350→441, spawn 후보 동일. commandlet 베이크는 `load_map` 뒤에 해야 한다(안 하면 스폰 포인트 transform이 원점으로 읽혀 실패).
- 다섯 정지 지점 재진단(발 위치 기준, `ExplainNavGap_Run3_Feet.log`·`Run4_Connectivity.log`): 최근접 node 594 110cm·733 117cm·731 188cm·730 202cm·729 280cm로 모두 투영 반경300cm 안이고, 모두 새 보간 node다. 이 node들은 저장 payload에 있고 주 정적 component(59,232 node)에 edge로 연결된다.
- 미검증: WIP 실제 PIE 군집 추격, 정식 옥탄트·fixture 재베이크와 전체 SurfaceNavigation, Production 비용. 잔여 탈락 사유 `ClearanceFloorOnly`(지형 바닥만 겹침, 10cm 들면 통과)는 지점마다1~3개 남아 있으며 이번 단위에서 바꾸지 않았다.

### 네 번째 PIE — Nav 전용 보간 WIP 수동 검증

- TestMap03·1인·밀도1·캐시1. 검사용 풀을 메모리에서만 선택했고 실제 WIP는 slot3/6이다. 8 validated slots·179 bindings·seam 높이 차/차단0(`NavGap/Run4_PIE.log`).
- 사용자 확인: NPC를 최대한 벽에 붙여 끌고 다녀도 정지가 **재현되지 않았다**. 벽에 붙었던 NPC는 잠깐 멈칫한 뒤 돌아서 추격한다. 적끼리 겹침은 이전과의 비교 기억은 없으나 서로 잘 안 겹쳐 수용 가능한 수준이다. 바위 근처에서 지상형 발이 일시적으로 겹쳤다가 바로 돌아나오며, 관통이 유지되는 현상은 없었다.
- 종료: StopPIE → IsPIERunning=false → 원래 `/Game/Maps/OctantPoolData` getter 복원. 보호 파일6개 해시는 재베이크한 WIP SurfaceData(a021e50c…)를 포함해 기대값과 같고 추적 Content/Config 변경0(`NavGap/SelectionState_Run4.json=restored`).
- 남은 것: 정식 옥탄트·fixture 재베이크(schema10)와 전체 SurfaceNavigation·Editor/Game 빌드·Production 비용, 단위2(플레이어 물리 지면/Nav 투영 분리·근접 접근/공격 위치). 벽에 붙은 플레이어 추격은 이번 PIE에서 따로 판정하지 않았다.

## 2026-11-16 — 단위1 마감: 정식 재베이크·고립 조각 제외·전체 회귀

- 사용자 결정: 단위1을 먼저 마감하고 문서 갱신 뒤 커밋, 단위2는 새 세션.
- 정식7(Meadow·Bastion·Hollow·Acropolis·Pyramid·Grove·Ridge)·fixture5(Crust·Regression·Conditional·Crawl·Island) SurfaceData를 schema10으로 재베이크했다(commandlet, 레벨별 `load_map` 뒤 `BakeOctant`). LVI 변경0.
- 첫 전체 회귀 136/137: `Nav.RegressionReachability`가 도달 그룹41→49(slot당+1)로 실패했다. `LNP.SurfaceNav.ListNavInterpComponents`(새 진단)로 엄격 보간 node가 없는 새 component를 세면 전체 369개였고 그중 338개가 node1~2개 잔재였다. 최근접 투영이 이런 조각으로 가면 NoNode 대신 도달 불가가 되므로, 베이크에서 엄격 node가 없는 같은 Layer 연결 조각을 버리도록 했다(D-084 보완, 보고서 `isolatedRejected`). 합성 검사 `Nav.NeedsExactWalkableCoverage`에 고립 패치 제외 사례를 추가했다.
- 최종 재베이크(`NavGap/BakeAll_Schema10_Drop.log`, WIP 포함13개). 이전 기준은 2026-11-08 D-081 재베이크, Grove·Ridge는 정식 통합 베이크다.

| 에셋 | Nav cells 이전→이후 | components 이전→이후 | portals 이전→이후 | 버린 고립 node |
|:---|:---|:---|:---|---:|
| Meadow | 67,729→68,000 | 91→84 | 24→24 | 12 |
| Bastion | 150,908→162,745 | 346→248 | 2,553→2,753 | 111 |
| Hollow | 185,328→192,058 | 25→24 | 1,155→1,347 | 7 |
| Acropolis | 136,706→139,452 | 280→237 | 1,325→1,528 | 57 |
| Pyramid | 754,899→778,225 | 339→222 | 4,110→5,679 | 349 |
| Grove | 128,907→130,817 | 83→76 | 423→509 | 7 |
| Ridge | 73,836→75,888 | 109→70 | 350→424 | 64 |
| Fixture Crust/Regression/Conditional/Crawl/Island | 68,500/73,302/73,528/69,729/72,814→68,546/73,735/73,983/69,856/72,984 | 5/17/19/26/6→5/17/19/26/6 | 0/66/66/4/10→0/79/79/4/12 | 0/1/1/0/0 |

  Bastion·Hollow의 이전 수치는 외곽 엄폐 추가 전후 차이가 섞였을 수 있다. 모든 에셋에서 새 보간 전용 component는0이고, 끊겨 있던 기존 component가 이어지며 component 수가 줄었다. 베이크 경고는 기존 Pyramid Layer 표본 경고17건·Meadow 이음매 clearance49건(엄격 규칙 그대로)이다.
- Game 빌드 Success(`Saved/Logs/Phase16_NavInterp_GameBuild.log`). Production 비용(쿡 필요)과 리슨2P는 단위2 뒤 1단계 마감에서 한 번에 잰다.
- 고립 조각 제외 뒤 재실행에서 `Nav.RegressionReachability`는 통과했고 `Nav.ProductionScheduler`가 299/300 성공으로 실패했다. 스케줄러·직접 탐색 결과는 일치(불일치0)했다. 실패 쌍은 끊겼던 component가 이어져 같은 group이 된 긴 우회 경로였다(직선61.6m·비용331.7m·상한 없이 확장32,342로 성공, production 요청 상한30,000). 검사는 상한 때문에만 실패하는 쌍을 따로 세고, 그런 쌍이 상한 없이 풀리며 전체의1% 이하인지 확인하도록 바꿨다(상한 안 요청은 모두 성공 유지). 요청 상한 자체는 바꾸지 않았다.
- **최종 전체 SurfaceNavigation 137/137 Success**(`NavGap/SurfaceNavFull_Final.log`). 경고4는 기존 노이즈(HTTP timeout3·B-option 안내1). 회귀가 재저장한 테스트 에셋3개는 git으로 원복했다. Editor·Game 빌드 Success. 다섯 정지 지점은 최종 WIP에서도 최근접 node 110~280cm·주 component 연결(`ExplainNavGap_Run5_Drop.log`).
- 커밋 범위: 실험 제거, D-084 구현·진단 명령2종·검사, 정식7·fixture5 SurfaceData, 문서. WIP SurfaceData(Git 무시)는 최종 규칙으로 재베이크된 상태다(원본 백업 `NavGap/Backup/`). 단위2는 새 세션(Phase 문서 §3.7).

## 2026-11-16 — 단위2: 보행 anchor(D-085)

- 재현(`Saved/Profiling/Phase16/Unit2/`): TestMap03·1인·밀도1, 검사용 풀을 메모리에서만 선택(Ridge WIP slot4/7, 8 validated slots·264 bindings·seam 차단0). 사용자는 근접 적이 대체로 잘 추격했고(지형에 걸리면 잠깐 섰다가 돌아옴), **벽에 바짝 붙은 플레이어를 공격하지 못한 경우는 없었다**고 보고했다. 여러 번 시도해 한 번 근접 적(entity32)이 벽 밑에서 멈췄다.
- 진단: 플레이어 Nav 투영은 유효(node 1671952, 157cm). entity32는 플레이어에서 277cm 떨어진 벽 밑 경사 띠(발밑 dot0.83, 실제 보행 가능)에 Alert·NoNode로 서 있었고 49초 동안 위치 변화0.001cm였다. `ExplainNavGap`(slot7 R(P=180,Y=270) → local=(Wy,Wx,−Wz))에서 주변 칸은 꼭짓점 하나가 벽면(비보행)에 걸려 빠졌고 최근접 node가 308cm로 투영 반경을 8cm 넘었다. 단위1의 NeedsExact와 다른 원인(격자 해상도)이다. 흐름: 직선 보행 판정 시 MoveTarget을 향해 곧게 걷다 띠에 올라섬 → 시작점 NoNode → 도달성 실패 → 1.5초 뒤 슬롯 반납 → Alert 정지. 근거 `Run1_Reports.log`·`ExplainNavGap_Run1.log`·`RuntimeInit_Run1.log`. PIE 종료 뒤 원래 풀 getter 복원, 보호 해시6개 불변(`SelectionState_Run1.json=restored`).
- 사용자 승인 설계(D-085): 플레이어 물리 지면/Nav 분리, 실제 이동으로 검증된 보행 anchor로 Nav 밖 적의 복귀·Nav 밖 플레이어 접근, 도달성·슬롯에 같은 anchor 사용.
- 구현:
  - `LNPEnemyNavigation::WalkReaches`·`FindWalkAnchor`: 600cm 안 최대4개 node를 가까운 순으로, `StepGrounded`를 캡슐 반지름 걸음으로 반복해 같은 slot·Layer의 Grounded만으로 닿는지 본다(제자리걸음·다른 층 높이 차는 실패).
  - `ULNPPlayerNavProcessor`: 접지 시 `Surface`·`GroundPoint`는 항상 물리 바닥. 300cm 안 node가 없으면 node에서 플레이어 발 150cm 안까지 걸어오는 anchor(`bWalkAnchored`·`WalkAnchorPoint`, 0.5초/50cm 캐시)를 `Node`로 쓴다.
  - `ULNPEnemyPathProcessor`: 소비 조건을 `Surface` 대신 `Node`로 바꿨다(anchor 없는 물리 바닥은 기존 동작). 시작 node가 없으면 anchor(0.5초 캐시)를 도달성·시작 node·경로 요청 시작점으로 쓰고 anchor로 조향한다. 직선 보행 판정이어도 Nav 밖이면 현재 위치→MoveTarget 발 이동을 검증해(50cm, 0.5초 캐시) 통과할 때만 곧게 간다. 목표가 투영되지 않고 플레이어가 anchor면 anchor node를 목표로 한다.
  - `ULNPTargetingSubsystem::RebalanceSlots`: 플레이어 `GetNavQueryPoint()`, 적 시작이 NoNode면 경로 fragment의 anchor에서 다시 판정한다.
  - `NavReport`: 플레이어 `walkAnchored`, 적 `walkAnchor` 필드 추가. 분리력·이동 프로세서·베이크는 바꾸지 않았다.
- 새 자동화 `EnemyMovement.WalkAnchorRecovery`(`LNPEnemyBlockerStepTest.cpp`, 이전 통로 픽스처 재사용: Nav |x|≤200, 적 x=520·플레이어 x=−520): 열린 띠는 기존 투영 NoNode(재현 조건) → anchor 발견·Reachable 회복·실제 걸어서 Nav 시작점 회복·마지막 접근 통과·플레이어 anchor, 벽(Blocker 높이300) 사이는 anchor·마지막 접근 모두 실패, 600cm 밖은 없음. 스텁(항상 실패) 상태에서 anchor·마지막 접근·플레이어 anchor 단언이 실패함을 먼저 확인했다(`WalkAnchor_Red.log`, 그때 플레이어 x=−480은 통로와 280cm라 재현 조건이 안 맞아 −520으로 고쳤다).
- 검증: Editor·Game 빌드 Success. `EnemyMovement` 7/7(`WalkAnchor_Green.log`). 전체 `LootNPop.SurfaceNavigation` **138/138 Success**(`SurfaceNavFull.log`), 경고는 기존 HTTP timeout·B-option 안내뿐. 회귀가 재저장한 테스트 에셋3개는 git으로 원복했다.
- 미검증: 수정본 Ridge WIP PIE, Production gate 비용(보행 검증의 접지 계측 포함분), 리슨2P. 보행 검증 걸음도 `EnemyGround` steps·cache 계측에 더해진다(드문 NoNode 개체만, 0.5초 간격).

### 수정본 PIE — 보행 anchor WIP 수동 검증

- TestMap03·1인·밀도1·캐시1, 검사용 풀을 메모리에서만 선택. 이번 배치의 Ridge WIP는 slot2 한 곳, 8 validated slots·264 bindings·seam 차단0(`Unit2/RuntimeInit_Run2.log`).
- 사용자 확인: 여러 번 시도했지만 벽 근처 근접 적 정지가 **재현되지 않았다**. 군집 겹침 악화·바위 관통 유지도 발견하지 못했다. 로그에 ensure·assert 없음(`Unit2/Run2_PIE.log`).
- 재현 빈도가 원래 낮았으므로(수정 전에도 여러 번 시도에 1회) 이 PIE는 재발 없음의 표본이지 부재의 증명은 아니다. 고정 회귀는 `WalkAnchorRecovery`가 소유한다.
- 종료: PIE 종료 확인 → 원래 `/Game/Maps/OctantPoolData` getter 복원, 보호 해시6개 불변·Content/Config 변경0(`Unit2/SelectionState_Run2.json=restored`).
- 단위2 완료. 남은 1단계 마감은 Production gate(새 패키지 쿡, 쿡 전 사용자 허가)와 리슨2P다.
