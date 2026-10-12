# Phase 9 성능·관통·메모리 Gate V011

> 상태: 성능 Gate(frame·경로) 통과, 관통 검사4회 완료(휴면 접안 겹침은 사용자 수용 한계 D-078), 메모리 기준선 기록 완료
> 날짜: 2026-10-12
> 범위: 단위5의 D-076 편성 새 옥탄트 seed 성능·관통·메모리 Gate와 exact P95 초과 원인 구분

## 1. 측정 조건

Meadow 기준선(Phase14 Final7)과 같은 harness·인자로 측정했다: `Scripts/Profiling/RunPhase13Gate.ps1 -Cases Production`, Development 패키지 리슨2P, host/guest `-nullrhi -corelimit=4`, `-LNPLoadBaselineSeed=1`, 30초 capture. seed1의 2P 배치는 Bastion0/3·Meadow1/5/6·Hollow2/4/7(bindings269)이다.

- 기준선 Final7도 헤드리스 측정이다. GPU 비용은 Phase 3 사용자 결정으로 Gate 밖이며 렌더링 측정은 하지 않았다. 이전 인계의 "고정 해상도 패키지"는 이 harness를 뜻하는 것으로 정리한다.
- 편성은 정식 `DA_MassSpawnConfig`(D-076)다. 수동 Pod 앵커가 설계 편성을 쓰므로 요청 적은 2,434(Final7 3,211 배치)이고 측정 중 활성은 약 435(Final7 약 530)다. 비용은 활성 수가 정한다(D-076).
- 측정 패키지: V010 패키지(exe `12CF8712…0667`, 현재 HEAD 소스)와 V011B 패키지(V010 + harness 관통 집계 한 줄, exe `732B90BD…56AA3`). 게임 로직은 같다.

## 2. 성능 Gate

| 실행 | frame P50/P95(ms) | 경로 tick P95(ms) | exact P95(ms) | exact 질의/frame P50 | 지상 cache 적중 |
|:---|:---|:---|:---|:---|:---|
| V010 성능1 | 8.18/9.17 | 0.021 | 3.527 | 809 | 100.00% |
| V010 성능2 | 6.98/7.76 | 0.017 | 2.748 | 785 | 100.00% |
| V011B 성능1 | 7.17/7.65 | 0.014 | 2.877 | 844 | 100.00% |
| V011B 성능2 | 7.21/9.13 | 0.015 | 3.611 | 878 | 99.95% |
| Final7(Meadow) 1·2 | 9.96/10.71, 9.95/10.67 | 0.033 | 4.132, 4.30 | 1,127 | 99.95% |

- **Gate(frame P95 ≤16.67ms·경로 tick P95 ≤1.5ms) 4회 모두 통과.** UnknownHits/EnvelopeEscapes0, probe PASS, 적2434/2434.
- 전원 활성 참고(V010 `AlwaysActive` 2회, Gate 아님): 2,434기 활성 frame P95 19.88/20.20ms, exact P95 18.6/18.8ms(질의 약3,300/frame), 지상 cache 99.85~99.89%. D-076의 활성 약2,000기 16.1ms 경계를 넘는 수에 맞는 증가다. 동시 활성 상한 약2,000 판단은 유지한다.

### 벽보행 교전 거리 상한(V011C)

벽보행 교전 거리(`DA_Enemy_PureEntity_Crawler01` `MovementConfig.AttackRange`)는 플레이 테스트로 정한다(현재 800). 튜닝 범위 전체를 미리 덮도록 인지 거리(`VisionDistance` 5,000)와 같은 5,000을 상한으로 임시 패키지를 만들어 측정했다. 에셋은 측정 뒤 800으로 되돌렸고 원본 SHA256과 일치한다. 발사체(`DA_NPC_Pistol`)는 1,000cm/s·수명60초라 5,000에서도 닿는다.

| 실행 | frame P50/P95(ms) | 경로 tick P95(ms) | exact P95(ms) | 질의/frame P50 | 교전 벽보행 평균 | 투사체 평균 |
|:---|:---|:---|:---|:---|:---|:---|
| AttackRange 5,000 #1 | 8.36/9.11 | 0.019 | 3.278 | 854 | 18.6 | 402 |
| AttackRange 5,000 #2 | 8.44/9.28 | 0.017 | 3.148 | 861 | 20.3 | 408 |
| AttackRange 800(V011B) | 7.17~7.21/7.65~9.13 | ≤0.015 | 2.877~3.611 | 844~878 | 약24 | 280 |

- **Gate 통과.** 투사체가 약1.45배로 늘었지만 frame P95는 800의 측정 범위 상단과 같다. 0~5,000 사이의 튜닝은 재측정 없이 진행할 수 있다. 인지 거리나 교전 개체 수를 늘리는 변경은 이 결과로 덮지 않는다.
- 근거: `Saved/Profiling/Phase09/GateV011C`, 패키지 `Saved/SurfaceNavigationPhase9V011CPackage`(Development 전체 cook 1,229).

### exact P95 >2ms의 분류

harness의 `exact=FAIL(P95<=2.0ms)`는 Phase 3 부하 시험(`phases/Phase03_MassWorldCollisionBaseline.md`)의 목표값이다. Phase 9 Gate 항목이 아니다.

- 같은 harness에서 Meadow만 있는 기준선 Final7도 4.13/4.30ms로 FAIL이었다. 새 옥탄트 혼합 풀은 2.75~3.61ms로 같거나 낮고 질의 수도 적다. 새 지형에 의한 회귀가 아니다.
- 지상 적은 Support cache 적중 100%라 exact를 거의 쓰지 않는다. exact 질의 대부분은 exact 표면만 따라가는 벽보행(D-066, D-075 개체당 약1회/frame)과 비행 이동에서 나온다. 설계상 비용이다.
- 이전 V008~V010 인계의 "exact P95 초과 원인 구분"은 이 분류로 닫는다. 2ms 목표를 다시 Gate로 삼으려면 별도 결정이 필요하다.

## 3. 관통 검사(4회, 벽보행·비행)

| 실행 | 검사 틱 | 비행 관통 | 벽보행 관통 개체·틱 | 그중 휴면 | 활성 관통 깊이 | frame P95(검사 비용 포함) |
|:---|:---|:---|:---|:---|:---|:---|
| V011B 1 | 1,807 | 0 | 293 | 290 | 0.110~0.119cm | 18.14 |
| V011B 2 | 2,154 | 0 | 343 | 338 | 0.133~0.174cm | 14.49 |
| V011B 3 | 2,171 | 0 | 346 | 343 | 0.149cm | 14.34 |
| V011B 4 | 2,154 | 0 | 341 | 339 | 0.157cm | 14.53 |
| Final7(Meadow) 1~4 | 1,728~1,778 | 0 | 1·2·0·0 | — | 0.11~0.16cm | — |

- **활성 개체 관통은 2~5 개체·틱, 깊이 0.11~0.17cm, 정적 지형 면이다.** 기준선에서 사용자가 수용한 일시 겹침과 같은 종류다(최대값이 0.01cm 크다). 압사0, UnknownHits0.
- **휴면 몫은 실행마다 같은 개체 하나다.** Hollow slot2의 Shuttle 패널이 접안하는 정적 벽면에 붙은 휴면 벽보행(entity 939, `Band=0`, 속도0, 운반 접촉 없음)이다. 이 개체의 표면 법선이 패널 면 법선과 같다(HitDotSurface 1.000). 패널이 접안할 때마다 개체 자리를 20.8cm 덮었다가 떠난다. 활성 개체는 패널 1,500cm 안에서 정지 겹침 검사(`ResolveStationaryOverlap`, D-077 압사)를 하지만 휴면 개체는 이동 프로세서에서 빠진다.
- 사용자 결정: **한계로 수용한다(D-078).** 휴면은 모든 플레이어에게서 약130m 밖이라 보이지 않는다. 깨어난 정지 개체는 같은 프레임 패널 근처 검사로 밀려나거나 압사한다. 이 검사 함수는 `SurfaceCrawl.CreviceContact` 정지 사례가 덮는다. 깨어나는 순간의 processor 수준 재현은 하지 않았다.
- harness는 이제 `dormantPenetratingEntityFrames`로 휴면 몫을 따로 보고한다(`LNPLoadBaseline.cpp`). 관통 Gate는 활성 몫으로 판정한다.
- 관통 검사 실행의 frame P95는 실행마다 약14.4ms 또는 약18ms 두 갈래로 나뉜다(V010 4회차 17.40, V011B 1회차 18.14). 같은 코드에서도 나오므로 측정 환경 편차로 본다. 검사 비용이 포함된 값이라 성능 판정에 쓰지 않는다(Final7과 같은 규칙).

### 기각한 첫 수정

처음에는 휴면 개체가 패널에 붙어 있다가 남겨진 것으로 보고 휴면 개체 운반을 넣었다(지상·벽보행 이동 프로세서에 휴면 전용 query). 재패키징 4회에서 같은 개체·위치·깊이로 그대로 관통해 원인이 틀렸음을 확인했다. `design/MovementIntegration.md` 벽보행 운반 절의 규칙대로 운반 접촉이 있으면 활동 대역이 강제 활성하므로 운반 누락은 애초에 없었다. 변경을 되돌렸고 근거는 `Saved/Profiling/Phase09/GateV011Fix`에 보존한다. 이 실행의 exact 증가(질의 +70~100/frame)는 같은 코드 편차와 구분하지 못했다.

## 4. 메모리 기준선

### 옥탄트별 베이크(현재 저장본)

| 항목 | Meadow_00 | Bastion_00 | Hollow_00 |
|:---|:---|:---|:---|
| 지형 exact 삼각형 | 지각 237,841(+섬·동굴 source) | 283,736(지각 227,636 + 구조물 56,100) | 165,086(지각 94,224 + 코드 판 3,096 + Blender 67,766) |
| Support source / Layer | 11 / 11 | 100 / 203 | 12 / 24 |
| Support 샘플 | 373,747 | 1,813,344 | 2,154,508 |
| 지각 NeedsExact | 1.43% | — | 37.01% |
| Support payload | 2,630,943 B | 12,858,982 B | 15,157,488 B |
| Nav cells / edges | 67,729 / 188,120 | 151,117 / 420,226 | 185,568 / 541,895 |
| Nav local components / portals / seam | 91 / 24 / 1,058 | 345 / 2,553 / 1,107 | 25 / 1,155 / 1,107 |
| Nav payload | 547,584 B | 1,232,304 B | 1,503,892 B |
| Traversal payload | 14,978 B | 68,227 B | 37,707 B |
| Spawn 후보 / payload | 9,990 / 399,612 B | 13,119 / 528,020 B | 12,699 / 510,452 B |
| SurfaceData uasset | 3.64 MB | 14.86 MB | 17.24 MB |

- 출처: Meadow·Hollow는 최신 베이크 로그 줄, Bastion은 V003 `baked-header.json`, 삼각형은 V001/V003/V017/V020 기록. Meadow의 섬·동굴 source 삼각형은 따로 집계하지 않았다.
- Hollow 지각 NeedsExact가 높은 것은 내부를 Blender Support로 넘겨 지각이 제거된 표본을 포함하기 때문이다(V017). Bastion 지각 값은 수집하지 않았다.

### 런타임(2P seed1, host)

| 항목 | 혼합 풀(V010) | Meadow만(Final7) |
|:---|:---|:---|
| serialized(고유 definition 합) | 33.87 MiB | 3.43 MiB |
| decodedResident | 60.11 MiB | 7.62 MiB |
| navResident | 28.49 MiB | 4.61 MiB |
| runtime Nav layers / static components | 511 / 589 | 88 / 689 |
| seam links / blocked seam nodes | 4,281 / 147 | 4,036 / 392 |
| 게시 시 process physical | 647.27 MiB(시작 533.65) | 501.80 MiB(시작 473.96) |
| 로드 elapsed / validateBuild | 184.99 / 167.32 ms | 44.32 / 31.21 ms |

- 새 옥탄트로 SurfaceData 상주 메모리가 약8배(decoded 7.6→60.1 MiB) 늘었고 게시 시 증가분은 약114 MiB다. 대부분 Bastion·Hollow의 다층 Support 샘플과 Nav다. 32GB 개발 머신에서는 문제가 아니며, 플랫폼 예산이 정해지면 이 표를 기준으로 비교한다.
- serialized 33.87 MiB는 세 definition의 uncompressed payload 합(약35.5 MB)과 같다. 같은 definition의 slot 사본은 공유된다.
- **cache 적중률**: 지상 Support cache는 전체 실행에서 99.95~100%(실내·공동 포함)다. harness는 지역별로 나눠 집계하지 않으므로 실내·공동만의 값은 따로 없다. Phase7c의 공동 전용 측정(97.5~97.6%)이 지역 기준이다. 경로 cache 적중은 65~79%다.

## 5. 회귀·패키징

- 휴면 운반 시도본(V011): 전체 빌드 55.6초, 전체127검사 실패0(경고 포함4/487.8초), 게이트 BuildCookRun exit0/115.6초. 되돌린 뒤 에디터 재빌드.
- 최종 V011B(harness 집계 한 줄): 에디터 빌드 성공, 전체127검사 통과(경고 없는123·경고 포함4·실패/미실행0, 491.2초), 입력 에셋7개 해시 유지, 테스트 생성 에셋3개 HEAD 복구(dirty content 0), 게이트 BuildCookRun exit0/118.3초.
- 로컬 근거: `Saved/Profiling/Phase09/GateV011`(V010 패키지 성능·관통·AlwaysActive), `GateV011Fix`(기각 수정), `GateV011B`(최종), `Saved/SliceOctant/GateV011`·`GateV011B`(회귀 보고서·cook 판정·스크립트).

## 6. 남은 범위

- D-017 벽보행 교전: LoS 수정 `0ab5e4f` 당시 유사 지형 사용자 PIE로 확인됐다. 교전 거리는 상한 5,000까지 성능 확인을 마쳤으므로(위 V011C) 그 안의 튜닝은 재측정하지 않는다.
- Phase9 종료 정리(Roadmap·기준 설계 갱신)는 위 재검증 뒤 묶는다.
