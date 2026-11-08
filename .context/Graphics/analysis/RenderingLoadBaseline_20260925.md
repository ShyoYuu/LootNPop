# 선행 렌더링 부하 측정 (2026-09-25)

> 상태: 측정 사실 보존 · 후보 미검증
> 갱신: 2026-11-08
> 목적: 그래픽스 작업 이전에 잰 적 다수 장면의 렌더링 부하를 이 폴더로 옮겨, 렌더링 최적화 병행 항목(Roadmap §4 트랙 O)의 출발점으로 쓴다

## 1. 한 줄 결론

서버 CPU는 적 1000마리에서도 여유가 있었다(렌더를 뺀 호스트 P95 6.1ms). 렌더링 호스트가 느린 원인은 GPU였고, 적 수에 비례해 늘어나는 GPU 비용 중 **확인된 가장 큰 몫은 Lumen의 동적 메시 처리(표면 캐시 재캡처·재조명)**였다. 그림자·베이스 패스·VFX의 증가분은 작았다.

## 2. 측정 조건 — 현재 기준선과 다른 점

| 항목 | 2026-09-25 측정 | Phase 0 기준선 |
|:---|:---|:---|
| 계기 | Surface Support·Navigation Phase 3 부하 기준선(`-LNPLoadBaseline=N` 하네스) | 그래픽스 벤치마크 하네스 |
| 맵·장면 | `TestMap03` -Z 극 PlayerStart 주변 링(1,500~약 7,000cm)에 적 N마리. 거의 모든 적이 한 시야에 몰린 **최악 조건** | Acropolis 고정 경로 배회 관찰 |
| 네트 구성 | 리슨 서버 호스트(렌더) + 게스트 `-nullrhi` 2P | 단독 실행 |
| 해상도 | 1280×720 창 모드 | 2560×1440, TSR 100% |
| 기타 | `-corelimit=4`, warm-up 10초·capture 30초, PureEntity 90%/ActorPromoted 10% | 180초 워밍업·60초 경로·3회 반복 |
| 공통 | 같은 머신(RTX 4060 Laptop, 로그 어댑터로 확인), 패키지 Win64 Development, UE 5.8 | 같음 |

프로젝트 `RendererSettings`(Lumen GI·반사, 메시 DF 생성, `r.Lumen.TraceMeshSDFs=0`, VSM, `r.RayTracing=True`, Substrate)는 2026-06-18 이후 바뀌지 않았다. 두 측정의 렌더러 설정은 같지만, 당시 Lumen이 하드웨어 레이 트레이싱 경로로 돌았는지의 실효값은 기록되지 않았다. 장면·해상도·네트 구성이 달라 **두 측정의 수치를 합치거나 직접 비교하지 않는다.** 이 문서의 수치는 가설의 근거로만 쓴다.

## 3. 호스트 프레임

| 조건 | 호스트 P50 / P95 | 비고 |
|:---|:---|:---|
| 적 1·발사체 0 (대조군) | 8.8 / 10.4ms | GPU 8.8ms와 거의 같음, 이미 GPU 한계 |
| 적 300·발사체 500 | 14.2~14.4 / 26.0~29.7ms | 2회 측정 |
| 적 1000·발사체 0 | 26.5 / 37.7ms | |
| 적 1000·발사체 500 | 28.7 / 37.0ms | 발사체 500발의 영향은 거의 없음 |
| 적 1000·발사체 0, 호스트 `-nullrhi` | 5.2 / 6.1ms | 렌더를 뺀 서버 CPU 비용 |

## 4. GPU 패스 분해 — 적 300마리 vs 대조군 (프레임당 평균)

| 패스 | 대조군 | 300마리 | 차이 |
|:---|---:|---:|---:|
| GPU 프레임 전체 | 8.8ms | 16.9ms | **+8.1ms** |
| Lumen 장면 갱신(`LumenSceneUpdate`·`MeshCardCapture`·`BuildCardUpdateContext`) | 0.1ms | 2.2ms | +2.1ms |
| Lumen 장면 조명(`LumenSceneLighting`) | 0.9ms | 2.9ms | +2.0ms |
| 후처리 + TSR | 2.1ms | 2.8ms | +0.7ms |
| VFX(`FXSystemPreRender`) | 0.1ms | 0.2ms | +0.1ms |
| 베이스 패스·그림자(`ShadowDepths`)·Nanite | 약 0.7ms | 약 0.9ms | +0.2ms |
| (참고) Lumen 스크린 프로브·간접광(`DiffuseIndirectAndAO`) | 약 3.0ms | 약 3.0ms | 0 — 해상도 비례, 적 수 무관 |

- 증가분 +8.1ms 중 표로 설명되는 몫은 약 4~5ms다. **나머지 약 3~4ms의 출처는 찾지 못했다.**
- P95가 P50의 약 2배라 간헐적 스파이크가 있다. 원인은 보지 않았다.
- 1080p 이상은 재지 않았다. 대조군 8.8ms 중 해상도에 비례하는 패스(스크린 프로브·TSR·후처리)가 커서, 목표 해상도에서는 **적 수보다 기본 장면 비용이 먼저 문제**가 될 수 있다.

## 5. 현재 기준선 자료와의 교차 확인 (2026-11-08)

당시 CPU trace(적 1000·발사체 0)의 서명은 다음과 같았다. 워커 `GPUBound_WaitingForGPUForOcclusionQueries` 평균 12ms/프레임, 렌더 스레드 `WaitForVisibilityTasks` 13.6ms, 게임 스레드 `Sync_RenderingThread` 12.8ms, 게임 스레드 자기 틱 약 9ms(병렬 틱 대기 포함).

Phase 0의 Medium CPU 진단(`Saved/Profiling/Graphics/Phase00/MediumCPUTrace_20261107_102250_f65aca/Exports/Measured_AllThreads.csv`, 같은 60초 구간·2983프레임)에도 같은 서명이 있다.

| 타이머 | 포함 시간 합 | 프레임당 |
|:---|---:|---:|
| `GPUBound_WaitingForGPUForOcclusionQueries_SeeGPUTrack` | 42.203966초 | 14.148ms |
| `WaitForVisibilityTasks` | 41.273144초 | 13.836ms |
| `Sync_RenderingThread` | 27.695381초 | 9.284ms |

엔진은 렌더 스레드가 가림 질의 결과를 받으려고 GPU를 기다리는 구간을 이 이름으로 표시한다. 두 측정 모두 "GPU 대기 → 렌더 스레드 가시성 대기 → 게임 스레드 렌더 동기화 대기" 순서로 이어지는 모양이다. 따라서 [GT 렌더 동기화 인계](RenderSyncOptimizationHandoff.md)의 GT 대기 9.27ms는 **GPU 처리량 부족이 역류한 대기일 가능성이 높다.** 다만 세 타이머는 서로 다른 스레드의 포함 시간이므로 인과를 확정하지 않았고, GPU 트랙(패스별 비용)은 이 진단에서 수집하지 않았다.

## 6. 렌더링 최적화 후보 (미검증)

1. **적 메시의 Lumen 기여 제한.** 표면 캐시에서 제외하거나 거리를 제한한다. 적 컴포넌트의 Lumen·DF 기여 플래그(`bAffectDistanceFieldLighting`, `bAffectDynamicIndirectLighting` 등), `r.LumenScene.SurfaceCache.*`·mesh card 예산을 본다. 현재 프로젝트 소스에는 이 플래그를 설정하는 코드가 없다. **ISM/ISKM 인스턴싱 적이 어떤 경로로 카드를 만드는지 먼저 확인한다.**
2. **목표 해상도의 기본 장면 비용.** 1440p 대조군(적 최소)의 GPU 비용을 업스케일 비율별로 다시 잰다.
3. **설명되지 않은 약 3~4ms와 P95 스파이크.** Insights GUI에서 GPU 트랙 계층으로 본다.
4. **비주얼 예산 재설정.** 위 측정 뒤 패스별 예산을 다시 잡는다. 당시 수치는 튜닝 전 엔진 기본값이라 한계치가 아니다.

후보별 수행 시점은 [로드맵](../Roadmap.md) §4 "렌더링 최적화 병행(트랙 O)"을 따른다.

## 7. 원자료와 재측정

- 원본 trace: `Saved/Profiling/Phase03/Pkg_GPU_300_P500.utrace`, `Pkg_GPU_1_P0.utrace`(GPU 포함). 그 밖의 `Pkg_*`, `Host_*`는 CPU 전용이다. 패키지는 `Saved/SurfaceNavigationPhase3Package`다. `Saved/`는 로컬에만 있다.
- 호스트 실행: `LootNPop.exe TestMap03?Listen -windowed -resx=1280 -resy=720 -corelimit=4 -ExecCmds="t.IdleWhenNotForeground 0, t.MaxFPS 0, r.VSync 0" -LNPLoadBaseline=300 -LNPLoadBaselineProjectiles=500 -LNPLoadBaselineQuit -trace=cpu,gpu,frame,bookmark,region,log -tracefile="<경로>.utrace"`. 20초 뒤 게스트를 `LootNPop.exe 127.0.0.1` + 같은 인자 + `-nullrhi`(trace 없이)로 붙인다. 대조군은 `-LNPLoadBaseline=1 -LNPLoadBaselineProjectiles=0`이다.
- 결과: 로그의 `[LoadBaseline]` 줄(P50/P95). capture 구간은 trace region `LNPLoadBaselineCapture`로 찍혀 있다.
- GPU 패스 내보내기 방법과 이벤트 중복 함정은 [측정 규약](../design/Measurement.md) §5 "GPU 패스 분해"를 따른다.
- 당시 에디터 `-game`에서 확인한 오염 요인(`LNP.Debug.DrawEnemyAction` 기본값 1, `-nullrhi`에서 비워지지 않는 수명 0 디버그 라인)은 패키지 판정 원칙(GX-001)의 근거로 남는다. `DrawEnemyAction` 기본값은 2026-11-08 현재도 1이다.
