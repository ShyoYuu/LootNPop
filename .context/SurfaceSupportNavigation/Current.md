# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 13 진행 중 — 단위 5 후속 묶음 2 완료, 다음 진행 확인 대기
> 마지막 갱신: 2026-10-04

## 현재 목표와 다음 행동

**후속 묶음 2 완료**다. 사용자 에디터/무거운 작업 종료 확인 후 같은 Phase 13 패키지의 자연 700 리슨 2P를 trace 없이 두 번 실행했다. 프레임 P95 14.70/14.95ms·경로 tick P95 0.753/0.611ms로 둘 다 frame/path Gate PASS다. 네 host/guest exit 0·probe 16/16 PASS·assert/ensure/fatal·Unknown/Envelope/LayerJump·Mass 순서 위반 0이다. Nav 요청/timing은 7,433/8,249행으로 제출 수·키 일치·중복/누락/시간 역전 검사를 통과했다. 원본과 `Summary.json`·`Validation.json`은 `Saved/Profiling/Phase13/NaturalRecheck`다. 실제 late join·지상/비행 화면·동일 게스트 NetID 48개(비행 6개 포함)의 제거/재추가도 통과했다. 에디터/게임/Insights는 종료 상태다. 이번 묶음에 gameplay·Content·Config 변경이나 새 빌드/cook은 없다. 아래 중간 진행 기록은 완료 전 경과다.

**다음 묶음은 production 성능과 충분한 배회 대역의 대역폭/총수 Gate 검증**이다. 이전 production 실패와 대역폭 미입증을 유지하며 Phase 13 완료·D-054·총수 상향을 선언하지 않는다. 사용자 진행 확인 전 다음 묶음에 착수하지 않는다.

사용자는 호스트·게스트 양쪽의 지상 배회·접지도 자연스럽다고 확인했다. 지상/비행 화면·실제 late join·동일 개체 bubble 제거/재추가 검증을 통과했다. PIE를 종료했고 `PIESession.log`·`PIEAudit.json`을 보존했다. 세션 assert/ensure/fatal·스케줄 순환 0이며 기존 Mover/CMC 로그는 남아 있다. 에디터 종료와 동일 측정 조건 확인 후 자연 700 trace 없는 두 반복을 진행한다. 아직 후속 묶음 2 완료나 다음 단위 착수 승인을 요청하는 단계는 아니다.

동일 게스트 bubble의 이탈 전/원거리/복귀 배열은 172/151/172개다. 이탈 때 제거된 NetID 48개가 복귀 때 모두 돌아왔고 48개 모두 ReplicationID가 변경됐다. 비행 config `DA9AB58F4275CA98C46515989CE22DE7` 6개도 포함된다. 사용자는 게스트가 호스트 곁으로 복귀하며 NPC가 등장하는 동안 튐이 없었음을 확인했다. 원본과 항목별 비교는 `Saved/Tests/Phase13_Unit5_Network/{ClientBeforeExit,ClientFar,ClientReturned}.txt`·`BubbleReentrySummary.json`이다. 실제 제거/재추가 검증은 통과이며 지상 배회·접지 관찰을 요청했다. 자연 700 측정은 여전히 미실행이다.

사용자가 실제 late join 세션의 호스트·게스트 양쪽에서 비행 드론 배회가 자연스럽고 이동–정지–이동 중 어색함·튐·후퇴가 없음을 확인했다. 비행 화면 관찰은 통과로 기록한다. 지상 화면 관찰과 동일 게스트/NetID의 bubble 제거·재진입은 아직 미확인이다. 게스트만 관찰 Pod에서 약 260m 이상 이탈하고 복귀 전 멈추도록 요청했으며, 제거 snapshot을 확인한 뒤 복귀를 진행한다. 이 확인만으로 자연 700·대역폭 Gate나 Phase 13 완료를 선언하지 않는다.

사용자가 다음 단위를 승인했고 Codex 재시작 후 MCP 연결을 복구했다. Phase 12 참고 수치/해석 한계는 `Roadmap.md`에 연결했다(Phase 12 착수 아님). 자연 700 실행기는 에디터 실행을 감지해 측정 시작 전에 중단됐으며 원본 실행은 없다. 현재 TestMap03을 기존 리슨 서버/단일 프로세스/플레이어 1명 설정으로 floating PIE 시작 후 약 50초 뒤 내장 `Add Client`로 실제 게스트 late join을 실행했다. Iris 서버/클라이언트와 Join succeeded를 확인했다. 사용자에게 지상/비행 종류별 이동·정지/접속 직후 튐·후퇴/접지·고도 관찰을 요청한 상태다. 설정·에셋·gameplay 변경은 없다. 내부 bubble은 MCP 일반 속성에 노출되지 않지만 엔진 읽기 전용 `obj dump`로 서버/게스트 배열과 NetID를 읽을 수 있다. 초기 자료는 `Saved/Tests/Phase13_Unit5_Network/{BubbleInitial,ClientInitial}.txt`이며 아직 같은 NetID의 제거/재진입을 검증한 것은 아니다. 이어서 동일 게스트 bubble 재진입을 관찰·확인하고 PIE 종료/에디터 종료 요청 뒤 자연 700 두 반복을 진행한다. 이번 묶음이 끝날 때 사용자에게 다음 진행을 확인한다.

2026-10-04 사용자의 이어서 진행 승인으로 단위 5 후속 비용 대조 묶음을 완료했다. 기존 실행기에 패키지·출력 폴더·반복 시작 번호 선택과 실행별 Nav 요청/timing CSV 보존을 더하고 전용 분석기를 구현했다. 이전/현재 패키지의 지상 700 추격을 같은 조건에서 순서를 뒤집어 두 쌍 측정했다. trace 없는 프레임 P95는 이전 12.50/14.40ms, 현재 14.10/14.13ms이며 네 실행 모두 프레임·경로 Gate PASS다. trace 대조 한 쌍과 두 Insights export도 정상 종료했다. 여섯 리슨 2P의 host/guest exit 0·probe 48/48 PASS·assert/ensure/fatal 및 불변 조건 위반 0이다.

이번 trace의 신규 활동/배회 관리는 호출당 합계 0.073ms, 위치 이력 증가분은 0.009ms다. 이동/경로/인지에는 큰 증가가 없고 trace 평균 승격 Actor는 이전 1.8/현재 9.3으로 다르므로 청크 증가에 프레임 차이를 귀속하지 않는다. 앞선 실패를 재현하지 못해 gameplay 수정은 추가하지 않았다. 자연 700·production·충분한 배회 대역과 네트워크 최종 검증은 남아 있으므로 Phase 13 완료나 총수 상향으로 보고하지 않는다. 원본·제한은 `history/Phase13_Log.md`와 `Saved/Profiling/Phase13/GroundAttribution/Summary.json`을 따른다. **다음 묶음은 자연 700의 trace 없는 재검증과 실제 late join·동일 게스트 bubble 재진입·지상/비행 배회 화면 검증**이다. 사용자 진행 확인 전 착수하지 않는다. 에디터/게임/Insights는 종료 상태이며 이번 묶음에 gameplay·Content·Config 변경이나 새 빌드/cook은 없다.

아래는 앞선 단위 5 계측 묶음의 결과이며 이번 추격 재측정과 구분한다.

2026-10-04 사용자의 이어서 진행 요청으로 단위 5에 착수했다. 에디터 종료 확인 후 production Pod 귀속 측정 모드와 항상 활성 대조 조건, 활동 대역 1Hz 표본, 복제 Dirty 사유·bubble 추가/제거와 게스트 연결 전체 수신량 계측을 구현했다. 에디터 전체 빌드·Development BuildCookRun exit 0(198.52초), 선별 회귀 실검증 39개·freshness 3개 성공, 비용 재생 1개 SKIP이다. 두 자동화 보고서에 각각 외부 HTTP 확인 타임아웃 경고 1개가 있으며 테스트 오류/실패/assert/ensure는 0이다. 패키지 일반 귀속 1P exit 0·네 probe PASS도 확인했다. 패키지는 `Saved/SurfaceNavigationPhase13Package`다.

trace 없는 production 1,801개 귀속 스폰의 프레임 P95는 24.71/24.25ms, 같은 스폰의 항상 활성 대조군은 32.35/31.95ms다. 휴면·지상 구간 소비와 복제 갱신을 확인했지만 양쪽 프레임 Gate는 실패했고 연결 전체 대역폭 감소도 입증하지 못했다. 비행 200은 P95 5.57/4.95ms로 프레임·경로 Gate를 통과했다. 지상 700 자연/추격은 초기 네 실행과 trace 없는 재검증 네 실행 모두 프레임 Gate를 넘었다. 같은 조건의 이전 Phase 8 패키지 추격 대조 한 번은 16.39ms로 통과했지만, 새 패키지 trace 실행은 14.71ms여서 실행 간 변동과 추가 비용의 귀속을 더 확인해야 한다. 사용자는 다른 무거운 작업 없음·같은 전원/성능 조건을 확인했다.

계측 구현 묶음은 완료했으나 단위 5 전체 Gate와 Phase 13은 미완료다. 새 총수 상한과 D-054는 바꾸지 않았다. 다음 구현은 지상 700 Gate 초과의 비용 분해·수정이다. 위치 이력/구간 데이터 증가로 최대 청크 개체 수가 86→51로 줄어든 것은 확인했으며 원인 확정이나 수정 근거로 확대하지 않는다. 실제 late join·동일 게스트 bubble 재진입·지상/비행 각각의 배회 화면·충분한 배회 대역의 대역폭/총수 검증도 남아 있다. 사용자 진행 확인 전 추가 구현에는 착수하지 않는다. 에디터와 검증 게임은 종료됐고 Content·Config 변경은 없다. 상세와 원본은 `history/Phase13_Log.md`, `Saved/Tests/Phase13_Unit5_Summary.json`이 소유한다.

사용자가 2026-10-03 Phase 13 착수와 단위 2 진행을 승인했다. 실행 계획은 `phases/Phase13_ActivityBandsAndWanderReplication.md`다. 단위 1에서 기존 Phase 8 Development 패키지로 지상 700·비행 100/300 리슨 2P의 변경 전 processor CPU와 exact 비용을 측정했다. 세 host/guest·세 Insights export exit 0, probe 24/24 PASS, Unknown/Envelope/LayerJump·Mass 순서 위반 0이다. trace 실행 프레임 P95는 19.60/5.58/9.54ms이며 정규 Gate나 최대 총수로 사용하지 않는다.

단위 1의 원본·분석·한계는 `history/Phase13_Log.md`와 `Saved/Profiling/Phase13/Before/Summary.json`이 소유한다. 병렬 이동 processor inclusive에는 대기가 포함되고 전체 exact에는 투사체도 포함되므로 이를 빼서 개체당 순수 CPU를 계산하지 않았다. 기존 harness 스폰은 ParentLootPod를 설정하지 않으므로 후속 대역 검증에 실제 Pod 귀속 시나리오가 필요하다.

단위 2에서 Pod 귀속 PureEntity 활동 대역·그룹 깨움·휴면 제외를 구현했다. 거리 판정 4Hz, 깨움 유지 10초, 피격·넉백·루팅·교전·Support/물리/사망 안전 조건을 적용한다. 무귀속·무효/Popped Pod·ActorPromoted는 항상 활성이다. 현재 에셋은 활성 경계 60m·복제 컬 120m·시각화 Off 240m이므로 각 멤버가 가시거리+5%(252m) 밖에 있을 때만 휴면을 허용한다. 에셋 가시거리는 유지했다. 단위 2 완료 시점의 Wander는 기존 서버 배회였으며, 활동 대역 관리는 매 프레임 두 개체 순회다.

단위 2 전체/증분 빌드 성공, 활동 자동화 4/4·관련 기존 회귀 실검증 29개 성공(비용 재생 1개 SKIP), 일반 스폰 TestMap03 `-game` 60초 exit 0을 확인했다. 마지막 활동 자동화 오류/assert/ensure 0, 실제 게임 assert/ensure/스케줄 순환 0이다. 기존 Mover/CharacterMovementComponent 로그는 남아 있다. 단일 전체 자동화 34/34 통과나 성능 개선/총수 상한 변경으로 보고하지 않는다. 증거·초기 실패 수정·제한은 `history/Phase13_Log.md`가 소유한다.

사용자가 2026-10-04 이어서 진행을 승인하여 단위 3 **서버 구간 배회**를 구현했다. 서버 PrePhysics에서 Pod 멤버별 부채꼴 후보를 검증하고 시작·도착·3초 정지 시각을 보간한다. 지상은 같은 Layer의 HighConfidence Support·overlay 포함 Nav 직선 경로·구간 생성 시 capsule sweep, 비행은 전체 구간 sphere sweep을 사용한다. 같은 Pod의 예약 선분 교차는 거부하고 겹친 출발점은 활성 분리로 복귀한다. generation·경유 Tile revision·피격/교전/물리 안전 조건이 바뀌면 취소한다. 동적 지지면이 있는 slot은 보간 중에도 exact sweep을 유지한다. 클라이언트는 아직 기존 위치 복제를 소비한다.

단위 3 전체 빌드 성공, 선별 자동화 보고서 36 Success 중 실검증 **35개 성공·비용 재생 1개 SKIP**, 테스트 오류/경고/assert/ensure 0이다. 일반 스폰 TestMap03 `-game` 60초 exit 0·assert/ensure/스케줄 순환 0이다. 기존 Mover/CharacterMovementComponent와 스폰 후보 부족 로그는 남아 있다. 상세 근거·제한은 `history/Phase13_Log.md`, 요약은 `Saved/Tests/Phase13_Unit3_Summary.json`이 소유한다. 성능 향상·새 총수 상한이나 패키지/리슨 2P·사용자 플레이 검증을 완료한 것으로 보고하지 않는다.

사용자가 단위 4까지 진행을 승인하여 **구간 복제와 클라이언트 소비**를 구현했다. PositionYaw 형제 필드에 1cm 좌표·1ms 시각·slot/Layer·고정 Yaw를 싣고 서버도 같은 양자화 값을 소비한다. 구간 중 위치 Dirty를 억제하며 HP/공격/피격은 계속 전달한다. 클라이언트는 동기화된 서버 시각으로 위치·이동/정지 위상을 평가하고 구간 취소 후 기존 보간으로 복귀한다. 위치 이력은 구간으로 되감으며 배회에는 기존 LOD 보간 지연을 추가하지 않는다.

단위 4 전체/최종 증분 빌드 성공, 선별 자동화 보고서 40 Success 중 실검증 **39개 성공·비용 재생 1개 SKIP**, 테스트 오류/경고 0이다. 같은 시각의 지상·비행 위치, 서로 다른 snapshot generation, 시간 wrap, dirty 억제·이벤트 전달, 중간 수신·late join 시각 소비, 활성 보간 복귀·애니 위상·구간 되감기를 자동화로 확인했다.

사용자가 TestMap03 리슨 2P에서 같은 Pod를 양쪽 원거리에서 관찰한 배회 이동/정지, 게스트 접근 후 다시 멀어짐, 인식 거리 밖 저격에 어색함이 없음을 확인했다. 저격 시 적이 공격자 쪽을 바라보고 잠시 뒤 배회하는 기존 동작도 유지됐다. MCP로 실제 Iris 게스트 접속·일반 스폰과 화면 렌더링을 확인했고, 세션 로그 assert/ensure/fatal·스케줄 순환 0, 양쪽 Mass 순서 검사 각각 208529/208544회·위반 0이다. 실제 bubble 제거/재추가와 내부 그룹 대역 전환을 계측한 것은 아니며, 지상·비행 각각의 화면 검증이나 실제 late join을 모두 확인했다는 뜻은 아니다. 이 항목과 패키지/cook/stale·성능·대역폭 Gate는 단위 5에서 확인한다. 상세는 `history/Phase13_Log.md`, 요약은 `Saved/Tests/Phase13_Unit4_Summary.json`이다.

단위 4를 완료했을 때 검증 PIE를 종료하고 임시 2P 설정의 플레이어 수를 원래 1명으로 복원했다. 단위 5의 현재 상태와 다음 행동은 이 문서 앞부분을 따른다.

## Phase 8 완료 결과

- 마커 로컬 Conditional Patch authoring·베이크·codec·freshness와 runtime Support/Nav/Walk edge 활성화를 구현했다. DataVersion 7·BakerSchemaVersion 9다. immutable snapshot은 Mass PrePhysics 경계에서 게시하며 지역 경로 무효화·도달성 변경·Home 재확인을 처리한다.
- 서버 스폰 `ALNPStatefulTerrain`의 Standing→Transitioning→Bridge 및 Intact→Destroyed, 복제 자세·collision·exact identity·DynamicSupport를 연결했다. 영구 동적 회귀 맵은 `/Game/Maps/SurfaceNavigation/L_ConditionalPatchRegression`, 전용 fixture는 `LVI_Octant_Fixture_Conditional`·`DA_OctantSurface_Fixture_Conditional`이다. 기존 production pool은 유지한다.
- 전체 자동화 실검증 89종 성공·비용 재생 1개 CSV 미지정 SKIP 이후, 저장 fixture와 파괴 기반 낙하 회귀를 추가했다. 최종 관련 자동화 7/7 성공·오류/경고 0이다. 전체 실행과 추가·선별 실행을 합친 실검증은 91종이며 단일 전체 실행 92/92로 보고하지 않는다.
- 사용자가 PIE Standing 접지·8초 회전 운반·다리 파괴 낙하, 별도 바닥 patch 접지·파괴 직후 입력 없는 낙하를 확인했다. 실제 `-game` 리슨 2P 운반, Bridge 이후 late join 보행·클라이언트 파괴 거부, 수정본 양쪽 파괴 낙하를 확인했다. movement-base 참조를 별도로 출력한 검증은 아니며 실제 운반 동작으로 확인했다.
- NPC 길 열림/닫힘은 저장 fixture의 여섯 상태별 실제 경로 요청기 성공/도달 불가와 8-slot endpoint·Support 회귀로 검증했다. 실제 NPC가 다리를 걷는 모습을 수동 관찰한 검증으로 보고하지 않는다.
- 최종 낙하 수정본 Development BuildCookRun exit 0(104.95초), cook 오류 0·기존 경고 13이다. 패키지 동적 맵 1P exit 0·네 probe PASS·Unknown/Envelope/Layer 0·순서 검사 716704회/위반 0이다.
- 최종 패키지 Meadow 700 자연/추격 각각 두 반복은 모두 frame/path PASS다. 자연 frame P95 14.00/12.90ms·path P95 0.600/0.346ms, 추격 frame P95 13.48/13.74ms·path P95 1.009/0.479ms다. 요청과 timing 행 수 일치, 중복·누락·시간 역전 0, host/guest 기능 검사·exit 0이다. 에디터와 무거운 작업 종료를 사용자가 확인한 뒤 측정했다.

상세 구현·수정 경위·검증 제한·증거 파일은 `history/Phase08_Log.md`가 소유한다. 최종 측정 원본은 `Saved/Profiling/Phase08/Unit5_Final700Summary.json`·`Unit5_Final700Artifacts.json`과 해당 host/guest 로그·CSV다. 낙하 수정 전 합격 데이터는 `PreDestroyedBaseFix`, 초기 성능 실패는 `PreHomeVersionFix`·`SurfaceGuardAttempt`, 대조군은 `Phase07cControl`에 보존했다.

## 수정한 회귀와 작업 상태

- 초기 World 복제·로드 크래시는 `new_level_from_template`으로 바꾸고 실제 에디터 재실행·저장·PIE·베이크까지 검증했다.
- 스폰 Home 연결성 버전 0이 초기 길 변경으로 해석되던 성능 회귀는 현재 snapshot 버전으로 초기화해 수정했다. exact 배치의 빈 SurfaceHandle도 처리한다.
- WP PlayerStart는 Actor `modify()`와 외부 패키지별 저장으로 새 게임 프로세스의 다리 위 시작을 보장했다.
- 정지 중 파괴 기반의 바닥 캐시가 남던 문제는 프로젝트 AsyncWalking 모드에서 충돌 없는 기반 해제·같은 틱 낙하 전환으로 수정했다. 사용자 재검증에서 수 초 공중 정지는 사라졌다. 파괴 상태 적용의 호스트/게스트 로그 차는 약 0.61초이며 네트워크 지연 0을 뜻하지 않는다.
- Phase 8 검증 종료 때 PIE를 종료했고 PIE용 적 밀도를 1로 복원했다. Phase 8 자동화 테스트 에셋 세 개는 원본 SHA256으로 복구했으며 이후 작업에서 해당 에셋 변경은 없다. 에디터의 현재 상태는 이 문서 앞부분을 따른다.

## 검증 주의와 진입점

- cook에는 두 회귀 게임 맵과 fixture LVI·fixture 폴더를 명시적으로 포함한다. soft path만으로 cook을 보장하지 않는다.
- 전체 자동화가 재저장하는 테스트 에셋은 실행 전 백업·실행 후 복구와 SHA256 검사가 필요하다. 단위 5 복구 증거는 `Saved/Tests/Phase08_Unit5_Final/TestAssetRestore.json`이다.
- 기존 Mover 초기화/out-of-band 경고와 Mass CharacterMovementComponent 추출 오류는 남아 있다. assert/ensure/crash/identity 오류와 구분하며 게임 로그 전체 오류 0으로 보고하지 않는다.
- 필수 문서는 `README.md`, 이 파일, `Decisions.md`다. 현재 계획은 `phases/Phase13_ActivityBandsAndWanderReplication.md`, 전체 단계 개요는 `Roadmap.md`다. Phase 8·7c 완료 이력은 해당 history 로그를 따른다.
