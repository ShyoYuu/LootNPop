# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 1 · 1-B0 선정 완료, 1-A Ridge 경로/출발 기준선을 새 세션으로 인계
> 마지막 갱신: 2026-11-13

## 0. 지금 상태

- **Phase 0 완료(2026-11-10, 사용자 마감 확인).** 네 개선(배회·그래플 앵커·GT 렌더 동기화·Nav 최근접)을 통합한 바이너리로 Low·Medium·High 각 3회가 재현성을 통과했다. 결과·전후 비교·GPU 패스 표·미관측 범위는 [Phase0 기준선 보고서](analysis/Phase00_BaselineReport.md), 경과는 [Phase0 로그](history/Phase00_Log.md)에 있다.
- **확정 기준선(3회 중앙값, ms):**

| 편성 | 초기 적 | Frame | GT | RT | GPU | 예산 대비 Frame |
|:---|---:|---:|---:|---:|---:|---:|
| Low | 1053 | 20.21 | 4.30 | 20.21 | 19.70 | 121.8% |
| Medium | 2223 | 20.26 | 5.53 | 20.25 | 19.75 | 122.0% |
| High | 3257 | 20.62 | 6.47 | 20.61 | 20.12 | 124.2% |

- 프레임은 GPU에 묶여 있다. 그래픽 큐에서 TSR 5.6ms(히스토리 200%), 비동기 Lumen 대기 약 5ms, 직접광 2.9ms가 크다. TSR 히스토리·Lumen 설정은 벤치마크 콘텐츠(Phase 1) 뒤 화질과 함께 다시 잰다([렌더 동기화 인계](analysis/RenderSyncOptimizationHandoff.md) §6.9). 해상도 하향은 최후의 카드다.

## 1. 바로 다음 작업

- **단위 N 완료(2026-11-10).** 원거리 Niagara 비표시의 원인은 float 정밀도가 아니라 `PointAttractionForce`(`Use Falloff=false`)의 거리 비례 힘이었다. 대체 중력(`GravityForce` + `normalize(Particles.Position) * 980.0`)을 세 방향에서 검증하고 GX-011로 확정했다. JustDodge V3b 게이트는 해제했다. 근거는 [단위 N 로그](history/UnitN_Log.md), 규칙은 [JustDodge VisualDesign](../JustDodge/design/VisualDesign.md) §4.
  - 후속 교체 완료(2026-11-10): 운영 `NS_Impact_Generic`(Sparks)·`NS_Explosion`(Sparks·Smoke)의 인력 모듈을 대체 중력으로 바꾸고 원거리 덤프로 폭주가 없음을 확인했다. 사용자 PIE 확인: 연기는 처음으로 보이게 됐고(자연스러움, 상승감은 없음), Impact 스파크는 콘 속도를 1150 → 300으로 낮춰 옛 크기에 맞췄다. 짧은 수명 탓에 스파크의 낙하는 체감되지 않는다.

- **O-2 완료(2026-11-10, GX-012 확정).** 기준선과 같은 바이너리로 Low·High × 렌더 해상도 100/77/67/58/50% + 드리프트 기준점 11회. GPU ≈ 고정 6.4ms + 화소 비례 13.3ms(네이티브), 적 +2204기의 GPU 기여는 노이즈 이하, 16.6ms 손익분기는 렌더 해상도 약 85%. [실행 문서](phases/UnitO2_ResolutionSweep.md), [로그](history/UnitO2_Log.md), [그래프](analysis/O2_ResolutionSweep.svg).

  - 확정 운영 조건(GX-012): 측정 기준은 네이티브 100%·TSR(Epic), 예산 미달 시 렌더 해상도 유지 후보(TSR 히스토리·Lumen, Phase 1 뒤) → 77% → 67%, 업스케일러는 TSR만.

1. **Phase 1 착수(2026-11-11, 사용자 작업 진행 요청).** [실행서](phases/Phase01_BenchmarkContent.md)를 만들었다. 첫 단위는 새 월드의 출발 기준선(Medium 3회 + 적 0 1회)이다. Phase 0은 Meadow 월드의 완료 기록으로 보존한다.
   - 적 0 인자 `-LNPGraphicsNoEnemies`·manifest/집계 보완, 현행 6종 풀에 맞춘 cook 목록을 준비했다. 집계 회귀/거부 검사와 cook root 존재 검사 8개 통과. **사용자 승인 후 Live Coding 컴파일 성공(2026-11-11)**. 정규 전체 빌드·패키지·실제 적 0 실행은 아직 미검증이다([로그](history/Phase01_Log.md)).
   - **대상 확정(2026-11-13, 사용자, GX-013): 기존 Ridge + Electric Dreams.** 새 옥탄트는 만들지 않는다. 1차 장식은 보행 충돌을 바꾸지 않는다(Ridge는 SSN Phase 15 경로 재측정 대상). Fab 재조사·라이선스(Standard·0원, AI 사용 불허 표시)·근거는 [후보 비교](analysis/Phase01_ContentCandidates.md) §5. 파라곤 종류/수는 미확정이다.
   - **카메라를 먼저 확정해야 한다.** 기존 코드는 Acropolis·slot 6만 허용한다. 시드 1 배정 예측은 Ridge를 slot 1·7에 둔다(실제 배정 미검증). 실제 배정을 확인하고 Ridge가 보이는 A/B/C를 정식 측정 전에 확정한다. 기존 경로를 그대로 돌려 콘텐츠 효과를 판정하지 않는다.
   - Electric Dreams 실물 검토(2026-11-12): [검토 보고서](analysis/ElectricDreamsReview_20261112.md). 조건부 적합, 대표 5메시 의존성 122개·원본 1.59GiB. 대상 반입·회전 시험·베이크·성능은 미실행이다.
   - **1-B0 완료(2026-11-13, 사용자 확인):** [선정 결과](phases/Phase01_RidgeZonePalette.md) §8. 대분지 대표 구역부터 Ridge 전체로 확장하며 사암 주조+바닥의 제한적 이끼·식생을 쓴다. 원본 지형 재질 파생본+메시 원본 MI 유지, 6종 조건부 팔레트를 채택했다. 의존성 104패키지·원본 1.663GiB, 텍스처 66개 중 VT 65개다. 원본 편집/저장·반입·PCG·베이크·빌드·성능 측정은 하지 않았다.
   - **바로 다음은 1-A:** 벤치마크 실제 시드/slot과 Ridge를 보는 A/B/C·연결 경로를 확정하고 콘텐츠 반입 전 출발 기준선을 확보한다. 일반 PIE slot 5에서 네 검토점은 사용자 통과했지만 벤치마크 배정/고정 경로 검증은 아니다. 이후 작은 반입 시험에서 지형 파생본·Cliff 접촉/노출·하상 접지·식생/바람을 검증해 팔레트를 최종 확정한다. 원자료는 `Saved/Profiling/Graphics/Phase01/RidgeZonePalette_20261113/`다.

## 2. 측정할 때 지킬 것

- 성능 판정은 패키지 빌드에서만 한다. 정식 반복은 회차를 하나씩 따로 돌리고 회차 사이에 냉각 대기를 둔다([측정 설계](design/Measurement.md) "회차 사이 냉각 대기"). 이 노트북은 지속 부하 약 5분 뒤 GPU 전력이 내려간다.
- 진단(`-trace`, `-LNPGraphicsDiagnosticCVars`)은 `performance_eligible=false`로 기준선과 섞지 않는다. 트레이스 실행은 종료 코드 777003을 내지만 원인은 미확인이다.
- 현재 통합 바이너리: `Saved/Graphics/RenderSyncDiagPackage_20261108`(소스 = `29305b7`, Meadow 포함 월드). 콘텐츠가 바뀌면 새 패키지로 다시 잡고 이 수치와 직접 섞지 않는다.
- 경로 영상은 `Scripts/Profiling/Graphics/RecordGraphicsBenchmark.ps1`(ffmpeg `D:\Tools\ffmpeg`)로 찍는다. 모니터 전체를 녹화하므로 사용자가 다른 창을 닫고 자리를 비운다고 확인한 때에만 실행한다.
- 에디터 빌드·Live Coding·쿡은 병행 세션이 있을 수 있으니 직전에 사용자에게 묻는다.

## 3. 새 세션 시작 절차

1. `README.md`·이 문서·`Decisions.md`를 읽는다.
2. **1-A는 새 세션에서 시작한다(2026-11-13, 사용자 요청).** [Phase 1 실행서](phases/Phase01_BenchmarkContent.md) §7의 인수인계/시작 프롬프트와 [1-B0 선정 결과](phases/Phase01_RidgeZonePalette.md) §8을 읽는다. 1-B0을 재선정하지 않고 실제 시드/slot·카메라부터 확인한다. 빌드/쿡은 실행 직전 병행 여부를 확인한다.
3. 각 분석·구현·측정 단위가 끝나면 계속 진행할지 확인한다. 수동 PIE·GUI가 더 빠르면 바로 요청한다.
