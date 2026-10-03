# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 13 진행 중 — 단위 1~2 완료, 단위 3 진행 확인 대기
> 마지막 갱신: 2026-10-04

## 현재 목표와 다음 행동

사용자가 2026-10-03 Phase 13 착수와 단위 2 진행을 승인했다. 실행 계획은 `phases/Phase13_ActivityBandsAndWanderReplication.md`다. 단위 1에서 기존 Phase 8 Development 패키지로 지상 700·비행 100/300 리슨 2P의 변경 전 processor CPU와 exact 비용을 측정했다. 세 host/guest·세 Insights export exit 0, probe 24/24 PASS, Unknown/Envelope/LayerJump·Mass 순서 위반 0이다. trace 실행 프레임 P95는 19.60/5.58/9.54ms이며 정규 Gate나 최대 총수로 사용하지 않는다.

단위 1의 원본·분석·한계는 `history/Phase13_Log.md`와 `Saved/Profiling/Phase13/Before/Summary.json`이 소유한다. 병렬 이동 processor inclusive에는 대기가 포함되고 전체 exact에는 투사체도 포함되므로 이를 빼서 개체당 순수 CPU를 계산하지 않았다. 기존 harness 스폰은 ParentLootPod를 설정하지 않으므로 후속 대역 검증에 실제 Pod 귀속 시나리오가 필요하다.

단위 2에서 Pod 귀속 PureEntity 활동 대역·그룹 깨움·휴면 제외를 구현했다. 거리 판정 4Hz, 깨움 유지 10초, 피격·넉백·루팅·교전·Support/물리/사망 안전 조건을 적용한다. 무귀속·무효/Popped Pod·ActorPromoted는 항상 활성이다. 현재 에셋은 활성 경계 60m·복제 컬 120m·시각화 Off 240m이므로 각 멤버가 가시거리+5%(252m) 밖에 있을 때만 휴면을 허용한다. 에셋 가시거리는 유지했다. Wander는 아직 기존 서버 배회이며 관리 자체는 매 프레임 두 개체 순회다.

단위 2 전체/증분 빌드 성공, 활동 자동화 4/4·관련 기존 회귀 실검증 29개 성공(비용 재생 1개 SKIP), 일반 스폰 TestMap03 `-game` 60초 exit 0을 확인했다. 마지막 활동 자동화 오류/assert/ensure 0, 실제 게임 assert/ensure/스케줄 순환 0이다. 기존 Mover/CharacterMovementComponent 로그는 남아 있다. 단일 전체 자동화 34/34 통과나 성능 개선/총수 상한 변경으로 보고하지 않는다. 증거·초기 실패 수정·제한은 `history/Phase13_Log.md`가 소유한다.

다음은 단위 3 **서버 구간 배회**다. 사용자 진행 확인 전에는 착수하지 않는다. 지상 Support/Nav·비행 sweep 구간 검증, 서버 시간 보간, 지형 변경·피격/전투 시 취소를 연결한다. MCP가 효율적이면 즉시 연결을 요청하고 전체 빌드가 필요하면 즉시 에디터 저장·종료를 요청한다. 에디터와 검증 게임은 종료된 상태다.

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
- 에디터와 검증 게임 프로세스는 종료됐다. PIE용 적 밀도는 1로 복원했다. 자동화 테스트 에셋 세 개는 원본 SHA256으로 복구했으며 이후 작업에서 해당 에셋 변경은 없다.

## 검증 주의와 진입점

- cook에는 두 회귀 게임 맵과 fixture LVI·fixture 폴더를 명시적으로 포함한다. soft path만으로 cook을 보장하지 않는다.
- 전체 자동화가 재저장하는 테스트 에셋은 실행 전 백업·실행 후 복구와 SHA256 검사가 필요하다. 단위 5 복구 증거는 `Saved/Tests/Phase08_Unit5_Final/TestAssetRestore.json`이다.
- 기존 Mover 초기화/out-of-band 경고와 Mass CharacterMovementComponent 추출 오류는 남아 있다. assert/ensure/crash/identity 오류와 구분하며 게임 로그 전체 오류 0으로 보고하지 않는다.
- 필수 문서는 `README.md`, 이 파일, `Decisions.md`다. 현재 계획은 `phases/Phase13_ActivityBandsAndWanderReplication.md`, 전체 단계 개요는 `Roadmap.md`다. Phase 8·7c 완료 이력은 해당 history 로그를 따른다.
