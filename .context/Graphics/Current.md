# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 0·단위 N·O-2 완료 · 다음은 Phase 1(벤치마크 콘텐츠)
> 마지막 갱신: 2026-11-10

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

1. [로드맵](Roadmap.md) 순서대로 Phase 1(벤치마크 콘텐츠). 착수 전에 사용자 확인을 받고 실행 문서를 `phases/`에 만든다. Phase 1 새 패키지에 적 0 대조군 진단 인자를 함께 넣는다(O-2 안 B 이월).

## 2. 측정할 때 지킬 것

- 성능 판정은 패키지 빌드에서만 한다. 정식 반복은 회차를 하나씩 따로 돌리고 회차 사이에 냉각 대기를 둔다([측정 설계](design/Measurement.md) "회차 사이 냉각 대기"). 이 노트북은 지속 부하 약 5분 뒤 GPU 전력이 내려간다.
- 진단(`-trace`, `-LNPGraphicsDiagnosticCVars`)은 `performance_eligible=false`로 기준선과 섞지 않는다. 트레이스 실행은 종료 코드 777003을 내지만 원인은 미확인이다.
- 현재 통합 바이너리: `Saved/Graphics/RenderSyncDiagPackage_20261108`(소스 = `29305b7`). 콘텐츠가 바뀌면 새 패키지로 다시 잡고 이 수치와 직접 섞지 않는다.
- 경로 영상은 `Scripts/Profiling/Graphics/RecordGraphicsBenchmark.ps1`(ffmpeg `D:\Tools\ffmpeg`)로 찍는다. 모니터 전체를 녹화하므로 사용자가 다른 창을 닫고 자리를 비운다고 확인한 때에만 실행한다.
- 에디터 빌드·Live Coding·쿡은 병행 세션이 있을 수 있으니 직전에 사용자에게 묻는다.

## 3. 새 세션 시작 절차

1. `README.md`·이 문서·`Decisions.md`를 읽는다.
2. Phase 1 실행 문서를 만들고 범위를 확인받는다.
3. 각 분석·구현·측정 단위가 끝나면 계속 진행할지 확인한다. 수동 PIE·GUI가 더 빠르면 바로 요청한다.
