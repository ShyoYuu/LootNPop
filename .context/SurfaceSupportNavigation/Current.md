# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 7c 완료 — 다음 단계 진행 확인 대기
> 마지막 갱신: 2026-10-03

## 현재 목표

Phase 7c 단위 5와 Phase 전체를 완료했다. 전체 빌드·자동화 85/85·Development BuildCookRun·최종 1P/2P·Meadow 700 반복 Gate·공동 비교와 문서 인계를 통과했다. 사용자가 지시한 **단위 5 전체 완료 뒤 진행 확인**을 기다린다. Phase 8·13 등 다음 단계는 아직 승인받지 않았다. 에디터 상태만으로 구현 방법을 선택하지 않는다. 라이브 작업이 효율적이면 MCP 연결을, 전체 빌드가 필요하면 에디터 저장·종료를 즉시 요청한다.

Phase 7a·7b와 7c 단위 0~4는 완료했다. 접힌 sheet 자동 분할·source별 해상도·복수 portal·Spawn headroom과 회귀 공동 콘텐츠를 구현했다. 사용자가 PIE에서 NPC 초기 묻힘 소실, 상층 인식/공격 유지, 계단·나선 추격, 공동 드론 교전, 수직 통로 그래플 지상 복귀를 확인했다. 완료 이력은 `history/Phase07_Log.md`·`history/Phase07c_Log.md`, 단위 계획은 `phases/Phase07c_VolumetricTerrainBaking.md`가 소유한다.

## 단위 5 구현과 확인된 결과

- `-LNPLoadBaselineVolumetric`과 `Scripts/Profiling/RunVolumetricBaseline.ps1`: 회귀 옥탄트의 공동·복층·두 긴 경사로에서 같은 seed/적 수로 exact-only와 cache-first를 비교한다. production pool 에셋은 바꾸지 않는다. 정적 fixture의 이동 패널 probe는 SKIP하고 Meadow에서 실제 패널을 검사한다.
- 구면 공동 바닥과 평면 입구 경사로의 작은 틈을 EntranceShell 양면 Blocker로 닫고 회귀 SurfaceData를 재베이크했다. 8-slot 실제 이탈 선분 회귀를 추가했다.
- 반복 이탈이 벽 밖 총구에서 시작되는 것을 추가 재현했다. 몸 중심→총구 첫 exact hit 1cm 앞에서 발사하는 `LNPProjectileMotion::ClipMuzzleToWorld`을 Actor/순수 엔티티 서버·Ghost·궤도 가이드에 공통 적용했다. 수정 전 8-slot 실패, 수정 후 전체 자동화 85/85·오류 0과 최종 공동 exact/cache 두 반복 모두 host/guest 이탈 0을 확인했다.
- 현재 데이터는 DataVersion 6/schema 8. 회귀 17 Layers·Nav 73,302 cells·66 portals, 8-slot Layer 136·component/group 41·binding 88·seam 4,428이다. serialized 3.23MiB, decoded resident 8.82MiB, Nav resident 6.03MiB다. 공동 m=1의 exact 오차는 지각 합격 기준을 통과했다.
- 총구 수정본 최종 패키지의 Meadow 700 자연/합성 각각 두 반복은 frame P95 13.16~14.03ms·path P95 최대 0.713ms로 모두 통과했다. 사용자가 다른 게임 종료를 확인한 뒤 측정했다. 공동 426 exact/cache 두 반복도 frame/path PASS, cache hit 97.50~97.63%·exact CPU P95 약 4.2→2.2ms다. 패키지 1P와 에디터 리슨 2P도 같은 snapshot·probe와 이탈/Unknown/Layer 오류 0을 확인했다. Editor frame P95는 성능 Gate가 아니다.
- 공동은 고정 영역·seed 1·현재 탐욕 배치·200cm 간격에서 426마리까지 배치한다. 500 요청은 426/500 shortfall로 중단했다. 두 모드 모두 426에서 frame/path Gate 여유가 있어 CPU 한계는 ≥426만 확인했다. Meadow의 수용량과 직접 비교하지 않는다.

## 바로 다음 작업

1. 사용자에게 다음 단계 진행 여부와 Phase 13 활동 대역·구간 배회 우선/Phase 8 동적 연결 중 착수 순서를 확인한다. 승인 전에는 실행 계획 구체화나 구현에 착수하지 않는다.
2. 진행을 승인하면 README·Decisions·Roadmap의 해당 범위와 실제 코드를 읽어 첫 구현 단위 계획을 정한다. 완료 증거는 `history/Phase07c_Log.md` 최종 Gate 표, `Saved/Profiling/Phase07c/Unit5_RunSummary.json`·`Unit5_FinalGateChecks.json`·`Unit5_PackageArtifact_Final.json`이다. 실행 후 해시와 CSV 무결성도 확인했다.

## 검증 주의와 문서 진입점

- fixture 패키지에는 `-map=/Game/Maps/TestMap03+/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression`와 `-CookDir=<프로젝트>/Content/Maps/SurfaceNavigation/Fixtures`가 모두 필요하다. soft path만으로 cook을 보장하지 않는다.
- 전체 자동화가 재저장하는 테스트 에셋 세 개는 `Saved/Tests/Phase07c_Unit5_AssetBackup/Manifest.json`의 백업으로 복구하고 SHA256을 확인한다. 최신 85/85 실행 뒤 복구 일치를 확인했다.
- 수정 전/CPU 간섭 표본은 `Saved/Profiling/Phase07c/PreMuzzleFix`에 보존했다. 원인·수치·실패 로그는 `history/Phase07c_Log.md`를 따른다. 초기 단발 이탈 선분은 exact 검사에서 hit였으며, 이후 벽 밖 총구 이탈을 별도로 재현했다. 모든 이탈을 같은 원인으로 단정하지 않는다.
- 기존 Mover 초기화·out-of-band 경고와 Mass CharacterMovementComponent 추출 오류는 남아 있다. assert/ensure/crash/identity 오류와 구분하며 게임 로그 전체 오류 0으로 보고하지 않는다.
- 필수 문서: `README.md`, 이 파일, `Decisions.md`. 상세 설계는 README 라우팅을 따른다. 다음 단계 우선순위는 `Roadmap.md`의 7b→7c→13→14 권장 순서와 Phase 8 동적 연결 요구를 사용자와 확인한다.
