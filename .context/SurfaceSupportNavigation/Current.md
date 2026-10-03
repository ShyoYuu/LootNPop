# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 8 단위 2 완료 — 단위 3 진행 확인 대기
> 마지막 갱신: 2026-10-03

## 현재 목표

2026-10-03 사용자가 **Phase 8을 먼저 완료한 뒤 Phase 13으로 진행**하는 순서를 승인했다. 구현 단위마다 완료·검증 후 멈추고 진행 확인을 받는다. 실행 계획은 `phases/Phase08_ConditionalPatchAndOverlay.md`다. 에디터 상태만으로 구현 방법을 선택하지 않는다. 라이브 작업이 효율적이면 MCP 연결을, 전체 빌드가 필요하면 에디터 저장·종료를 즉시 요청한다.

단위 1은 기존 Pod blocker와 활성 마커 node ref 목록의 Nav 점유 합성 기반이다. 전체 빌드 성공, Nav 자동화는 실검증 15개 성공·비용 재생 1개 입력 미지정으로 SKIP했다. 겹침 제거·중복 적용·slot/generation 검증과 Tile revision을 확인했다. 결과는 `history/Phase08_Log.md`가 소유한다. 단위 2의 Patch authoring·베이크·codec·freshness 입력을 완료했다. 현재 DataVersion 7·BakerSchemaVersion 9이며 SurfaceData 3개 재베이크, 전체 빌드, 전체 자동화 실검증 86개 성공·비용 재생 1개 SKIP(실패 0)을 확인했다. B-option 독립 에셋 검사는 기존 추출 에셋을 검증했다는 경고 1개다. 테스트 전용 에셋 3개는 원본 SHA256과 일치하도록 복구했다. Actor 상태·런타임 활성화 연결은 단위 3~4이며 현재 patch는 비활성 데이터로 로드한다. Phase 7c의 전체 빌드·자동화 85/85·Development BuildCookRun·최종 1P/2P·Meadow 700 반복 Gate·공동 비교는 완료했다.

Phase 7a·7b와 7c 단위 0~4는 완료했다. 접힌 sheet 자동 분할·source별 해상도·복수 portal·Spawn headroom과 회귀 공동 콘텐츠를 구현했다. 사용자가 PIE에서 NPC 초기 묻힘 소실, 상층 인식/공격 유지, 계단·나선 추격, 공동 드론 교전, 수직 통로 그래플 지상 복귀를 확인했다. 완료 이력은 `history/Phase07_Log.md`·`history/Phase07c_Log.md`, 단위 계획은 `phases/Phase07c_VolumetricTerrainBaking.md`가 소유한다.

## 단위 5 구현과 확인된 결과

- `-LNPLoadBaselineVolumetric`과 `Scripts/Profiling/RunVolumetricBaseline.ps1`: 회귀 옥탄트의 공동·복층·두 긴 경사로에서 같은 seed/적 수로 exact-only와 cache-first를 비교한다. production pool 에셋은 바꾸지 않는다. 정적 fixture의 이동 패널 probe는 SKIP하고 Meadow에서 실제 패널을 검사한다.
- 구면 공동 바닥과 평면 입구 경사로의 작은 틈을 EntranceShell 양면 Blocker로 닫고 회귀 SurfaceData를 재베이크했다. 8-slot 실제 이탈 선분 회귀를 추가했다.
- 반복 이탈이 벽 밖 총구에서 시작되는 것을 추가 재현했다. 몸 중심→총구 첫 exact hit 1cm 앞에서 발사하는 `LNPProjectileMotion::ClipMuzzleToWorld`을 Actor/순수 엔티티 서버·Ghost·궤도 가이드에 공통 적용했다. 수정 전 8-slot 실패, 수정 후 전체 자동화 85/85·오류 0과 최종 공동 exact/cache 두 반복 모두 host/guest 이탈 0을 확인했다.
- Phase 7c 최종 Gate 측정 데이터는 DataVersion 6/schema 8이었다(현재 재베이크는 7/9). 당시 회귀 17 Layers·Nav 73,302 cells·66 portals, 8-slot Layer 136·component/group 41·binding 88·seam 4,428이다. serialized 3.23MiB, decoded resident 8.82MiB, Nav resident 6.03MiB다. 공동 m=1의 exact 오차는 지각 합격 기준을 통과했다.
- 총구 수정본 최종 패키지의 Meadow 700 자연/합성 각각 두 반복은 frame P95 13.16~14.03ms·path P95 최대 0.713ms로 모두 통과했다. 사용자가 다른 게임 종료를 확인한 뒤 측정했다. 공동 426 exact/cache 두 반복도 frame/path PASS, cache hit 97.50~97.63%·exact CPU P95 약 4.2→2.2ms다. 패키지 1P와 에디터 리슨 2P도 같은 snapshot·probe와 이탈/Unknown/Layer 오류 0을 확인했다. Editor frame P95는 성능 Gate가 아니다.
- 공동은 고정 영역·seed 1·현재 탐욕 배치·200cm 간격에서 426마리까지 배치한다. 500 요청은 426/500 shortfall로 중단했다. 두 모드 모두 426에서 frame/path Gate 여유가 있어 CPU 한계는 ≥426만 확인했다. Meadow의 수용량과 직접 비교하지 않는다.

## 바로 다음 작업

1. 단위 3 진행 승인을 받은 뒤 활성 patch Support·Nav view, 명시적 Walk edge, 도달성·지역 revision·Mass phase 경계 snapshot 게시를 연결한다. 단위 2 때문에 요청했던 에디터 종료는 사용자가 완료했고 전체 빌드·재베이크·자동화를 마쳤다.
2. 단위 3 착수 전에 사용자 진행 확인을 받는다. Phase 7c 완료 증거는 `history/Phase07c_Log.md` 최종 Gate 표, `Saved/Profiling/Phase07c/Unit5_RunSummary.json`·`Unit5_FinalGateChecks.json`·`Unit5_PackageArtifact_Final.json`이다.

## 검증 주의와 문서 진입점

- fixture 패키지에는 `-map=/Game/Maps/TestMap03+/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression`와 `-CookDir=<프로젝트>/Content/Maps/SurfaceNavigation/Fixtures`가 모두 필요하다. soft path만으로 cook을 보장하지 않는다.
- 전체 자동화가 재저장하는 테스트 에셋 세 개는 실행 직전 백업으로 복구하고 SHA256을 확인한다. 최신 단위 2의 백업은 `Saved/Tests/Phase08_Unit2_TestAssetBackup/Manifest.json`, 복구 증거는 `Saved/Tests/Phase08_Unit2/TestAssetRestore.json`이다.
- 수정 전/CPU 간섭 표본은 `Saved/Profiling/Phase07c/PreMuzzleFix`에 보존했다. 원인·수치·실패 로그는 `history/Phase07c_Log.md`를 따른다. 초기 단발 이탈 선분은 exact 검사에서 hit였으며, 이후 벽 밖 총구 이탈을 별도로 재현했다. 모든 이탈을 같은 원인으로 단정하지 않는다.
- 기존 Mover 초기화·out-of-band 경고와 Mass CharacterMovementComponent 추출 오류는 남아 있다. assert/ensure/crash/identity 오류와 구분하며 게임 로그 전체 오류 0으로 보고하지 않는다.
- 필수 문서: `README.md`, 이 파일, `Decisions.md`. 상세 설계는 README 라우팅을 따른다. 이번 진행 순서는 사용자 승인에 따라 Phase 8→13이다.
