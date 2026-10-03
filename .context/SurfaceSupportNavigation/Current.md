# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 8 완료 — Phase 13 착수 확인 대기
> 마지막 갱신: 2026-10-03

## 현재 목표와 다음 행동

사용자가 승인한 순서는 **Phase 8 → Phase 13**이다. Phase 8 단위 1~5와 최종 Gate를 완료했다. 구현 단위마다 완료·검증 뒤 진행 확인을 받는 규칙에 따라 여기서 멈추고 Phase 13 착수 여부를 확인한다. 아직 Phase 13 구현이나 실행 문서 작성에 착수하지 않았다.

승인되면 `Roadmap.md`와 활동 대역·구간 배회 복제 관련 D-067~D-069, `design/MovementIntegration.md`를 읽고 현재 Mass 배회·복제 소비자를 확인해 Phase 13 실행 계획과 첫 구현 단위를 구체화한다. 에디터 상태만으로 방법을 선택하지 않는다. MCP가 효율적이면 즉시 연결을 요청하고, 전체 빌드가 필요하면 즉시 에디터 저장·종료를 요청한다.

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
- 필수 문서는 `README.md`, 이 파일, `Decisions.md`다. 완료 계획은 `phases/Phase08_ConditionalPatchAndOverlay.md`, 다음 단계 개요는 `Roadmap.md`다. Phase 7c 완료 이력은 `history/Phase07c_Log.md`를 따른다.
