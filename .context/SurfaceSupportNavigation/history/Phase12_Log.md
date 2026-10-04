# Phase 12 작업·검증 기록

## 2026-10-04 — 단위 1 완료: 선분–캡슐 판정 정확성

### 요청과 범위

사용자는 캐시 도입과 별개로 `ProjectileHitDetection` 최적화를 요청했으며, 먼저 `SegmentHitsCapsule`의 중심 최근접점·원기둥 판정 오류를 고치도록 지정했다. 구현 단위마다 검증 후 계속 진행할지 확인한다. 이번 단위는 공유 기하 판정과 호출부·회귀 테스트이며 성능 최적화나 계측은 다음 단위다.

### 구현

- 투사체 선분과 캡슐 축 선분의 최소 거리 제곱을 비교한다. `FMath::SegmentDistToSegmentSafe`의 제한 구간 최소화를 참고하되 런타임 함수는 직접 구현했고 정규화·제곱근을 사용하지 않는다.
- 축 반길이는 `max(CapsuleHalfHeight - CapsuleRadius, 0)`이다. 반지름 인자를 원래 반지름·추가 판정 반경으로 분리해 축을 유지한 채 양 끝 반구도 팽창시킨다.
- 점 선분·구형 캡슐·평행 선분·양 끝점 제한을 처리한다. 외적 제곱 형태의 분모를 사용해 근평행 선분에서 길이 제곱의 곱을 빼는 상쇄를 피한다. 반환점은 투사체 선분 위의 축 최근접점이며 표면 진입점은 아니다.
- 서버·클라이언트 코스메틱·패링·보간 지연 진단·조준점 공유 호출과 기존 월드 차폐 테스트 인자를 수정했다. 대상 순회·되감기·월드 query 순서는 이번 단위에서 수정하지 않았다.

### 검증

- `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 전체 빌드 성공(exit 0, 83.70초). 실행 전 에디터·게임 프로세스 없음 확인. 프로젝트 규칙에 따라 처음부터 권한 확장 실행했다.
- `UnrealEditor-Cmd -unattended -nullrhi` 선택 자동화 4개 PASS, warning 성공 0·실패 0·미실행 0, 프로세스 exit 0.
  - `LNP.HitDetection.SegmentCapsule.Regression`: 17개 기하 사례. 비스듬한 축 교차, 원기둥 모서리 오탐, 위/아래 반구·추가 반경, 평행·점·구형·끝점·근평행을 검사한다. 이동·회전 및 선분 반전, 반환점의 선분 포함·miss 출력 불변도 검사한다. 정확한 접선은 회전 반올림 영향이 없는 정수 좌표에서 검사한다.
  - `LNP.HitDetection.SegmentCapsule.EngineReference`: seed 12004로 10,000개 무작위 사례를 생성해 엔진 함수와 hit 여부·반환점을 대조한다. 엔진 함수가 근사하는 근평행 조건은 무작위 대조에서 제외하고 명시적 회귀 사례로 검증한다. 기준 함수의 정규화·제곱근은 테스트에만 있다.
  - `LootNPop.SurfaceNavigation.WorldCollision.EarliestHit`: 월드에 잘린 선분의 앞 적 hit·뒤 적 miss.
  - `LootNPop.SurfaceNavigation.WorldCollision.ProjectileArc`: 기존 exact 탄도 회귀.
- `git diff --check` 통과. Content·Config 변경 없음. 자동화 결과는 `Saved/Tests/Phase12_Unit1/index.json`, 로그는 `Saved/Logs/Phase12_Unit1.log`다.

### 해석과 다음 단위

이번 결과는 공유 수학 판정과 기존 자동화의 통과다. 실제 플레이·패키지 리슨 2P·성능 이득 검증으로 확대하지 않는다. 형상과 조준 반환점이 바뀌었으므로 최적화 비교 기준선은 수정본으로 다시 만든다. Phase 13 최종 Gate는 계속 보류한다.

사용자 진행 확인을 받은 뒤 단위 2에서 대상 수집·후보 제외·되감기·캡슐 판정·월드 query 내부 비용을 분해하고 정확성 수정본 기준선을 보존한다. 이후 sphere 선행 검사·공간 질의·계산 순서 최적화를 검토한다.

## 2026-10-04 — 단위 2 완료: 내부 비용 계측과 수정본 기준선

### 구현과 실행 조건

사용자의 계속 진행 확인과 동일 전원·성능 설정·다른 무거운 작업 종료 확인을 받았다. 에디터·게임·Insights 종료 상태에서 빌드했고 에디터 상태로 방법의 우선순위를 정하지 않았다. 이 단위는 계측·측정이며 후보 제외·기하·되감기 결과와 팀별 차단 규칙은 바꾸지 않았다.

- `LNPProjectileHitProfile.h`와 프로세서 인스턴스별 누계를 추가했다. `LNP.Projectile.ProfileStride=0`이 기본이며 상세 시계·카운터·로그를 끈다. 진단에서는 지속되는 투사체 순번으로 64/128개당 하나를 선택해 1초 창으로 누적한다. 수집·월드 query는 전수, 후보·제외·되감기·히스토리·캡슐은 선택된 투사체만 측정한다. 시계 간격도 참고치로 기록한다.
- 서버·클라이언트 수집 및 투사체 청크에 큰 범위 trace를 추가했다. 개별 타겟의 trace 이벤트는 없다. 새 자식 범위가 생겼으므로 **processor exclusive는 이전 구조의 exclusive와 직접 비교할 수 없다**. 전체 inclusive·새 청크 범위와 그 exclusive를 구분한다.
- `RunPhase12Baseline.ps1`, `AnalyzePhase12Baseline.ps1`을 추가하고 기존 `RunPhase13Gate.ps1`에 기본 0인 stride 인자만 더했다. 기존 인구·Nav·trace 분석을 재사용한다.
- 게임·에디터 빌드/cook 성공(BuildCookRun exit 0, 176.13초). 캡슐 회귀·엔진 함수 대조·월드 차폐/탄도 자동화 4개 PASS(실패·미실행 0, exit 0). 로그는 `Saved/Logs/Phase12_Unit2_BuildCookRun.log`, `Phase12_Unit2.log`, 보고서는 `Saved/Tests/Phase12_Unit2/index.json`이다. cook에는 기존 경고가 포함됐다.
- 패키지: `Saved/SurfaceNavigationPhase12BaselinePackage`. 바이너리 SHA256 `FE6D154287511572E95664BEFDC3311D1B0D3557DAC4001A32084A3A4C88BF64`.
- 이전 Phase 13과 같은 리슨 2P·seed 1·nullrhi·corelimit 4·10초 준비/30초 capture·투사체 목표 500·cache/lateral/parallel On·기존 lock probe On이다. trace 없는 순서는 자연 700→800→800→700, 추격도 동일하다. trace는 자연 700→800→추격 800→700(stride 64), 뒤에 자연 700의 stride 0·128 진단 대조를 추가했다.

### trace 없는 수정본 기준선

| 조건 | frame P95(ms), 두 반복 | 경로 Gate |
|:---|:---|:---|
| 자연 700 | 26.24 / 28.65 | 두 번 PASS |
| 자연 800 | 28.84 / 29.09 | 첫 PASS / 둘째 FAIL |
| 추격 700 | 25.66 / 26.22 | 두 번 PASS |
| 추격 800 | 28.94 / 29.63 | 두 번 FAIL |

여덟 번 모두 frame FAIL이다. 정확성 수정 전과 형상·명중/소멸 조건이 다르고 계측 코드 구조도 추가됐으므로 이전 대비 차이를 한 연산이나 기하 수정만의 비용으로 단정하지 않는다. 다음 변경은 수정본을 대조군으로 같은 조건에서 교차 반복해 판정한다.

### 내부 비용 분해

| 상세 trace 조건 | processor inclusive(ms/호출) | 투사체 청크 exclusive(ms/frame) | 수집 Enemy/Player(ms/frame) | 월드 wrapper(ms/frame) | 후보/샘플 투사체 |
|:---|---:|---:|:---|---:|---:|
| 자연 700 | 14.345 | 13.199 | 0.0361 / 0.0020 | 1.127 | 700.07 |
| 자연 800 | 16.658 | 15.447 | 0.0452 / 0.0020 | 1.184 | 800.10 |
| 추격 700 | 10.610 | 9.772 | 0.0305 / 0.0018 | 0.819 | 699.99 |
| 추격 800 | 12.031 | 11.181 | 0.0312 / 0.0011 | 0.833 | 796.94 |

수집·청크는 trace capture 전체, 월드·후보는 활동 표본 사이에 완전히 포함된 28개의 1초 상세 창에서 얻었다. 각 창의 타겟 scope 수 = 후보 수, 월드 wrapper 수 = 투사체 수와 실제 샘플 비율을 검증했다. 분모가 다르고 범위가 중첩돼 있으므로 표를 합산하지 않는다.

- 후보의 **99.716~99.751%**가 적 팀 투사체와 적 캡슐의 쌍이다. 부하 투사체는 `Enemy` 팀·되감기 0초로 생성되며 서버는 발사자만 제외하고 같은 적 팀도 캡슐에 닿으면 투사체를 파괴한다. 클라이언트의 같은 팀 제외와 다르다. 서버에서 팀 쌍을 바로 건너뛰는 것은 기존 차단 규칙 변경이므로 이번 단위에서 도입하지 않았다.
- 원격 발사 샘플·실제 히스토리 되감기 호출은 **모두 0**이다. 이 편성은 원격 플레이어 사격의 히스토리 조회·배회 구간 되감기 비용을 대표하지 않는다. 되감기 최적화 이득은 아직 미확정이다.
- 캡슐 scope는 호출당 raw **37.87~43.81ns**, 시계 간격 참고치는 **15.15~17.00ns**다. 제외·되감기 0초 분기 raw는 시계 비용과 가까워 미세한 차이로 우선순위를 정하지 않는다. 캡슐 검사 횟수를 줄이는 것을 우선 검토할 근거는 거의 전수인 후보 수와 큰 청크 exclusive 비용이다.
- 상세 `Targets`는 하위 연산의 시계·scope bookkeeping까지 포함한다. 샘플 값을 전수로 확대하면 실제 부모보다 클 수 있다. 예컨대 자연 700 캡슐 raw 환산은 15.20ms/frame으로 부모 inclusive 14.36ms/frame보다 크다. **환산을 실제 CPU 점유율이나 절감 가능 시간으로 해석하지 않는다.** 시계 간격을 일률적으로 빼 보정한 수치를 실측처럼 제시하지 않는다.
- 자연 700 진단의 processor inclusive는 stride 64에서 14.345ms/호출, Off에서 10.291, stride 128에서 12.951이다. 각 실행 frame P95는 28.83/20.97/27.77ms로 변동이 있다. Off 대조는 원래 trace 없는 Gate보다도 낮으므로 차이를 순수 계측 오버헤드로 단정하거나 성능 이득으로 쓰지 않는다. stride 128에서도 후보 699.36개·히스토리 호출 0·캡슐 raw 41.92ns의 경향은 유지됐다.

### 검증 증거와 한계

- 14회 host/guest **28개 exit 0**, 동일 바이너리 해시, probe **112/112 PASS**, 실행당 실제 700/800 전량·30개 활동 표본·무귀속 Active를 확보했다. assert/ensure/fatal·불변 위반은 0이다. 기존 headless Mover 입력 fault와 Mass CharacterMovementComponent 추출 오류는 남아 있다.
- 기본 12회 분석은 Nav CSV 행 수·고유 키·순서·timing 짝과 동일 편성·trace 구간·export를 검증했다. 진단 2회도 종료·편성·probe·불변 조건·export·샘플 창 완결성을 확인했다. 상세 계측은 5개 실행에서 각각 28개 유효 창을 확보했다.
- 분석 exit 0. 원본·`Summary.json`·`ProjectileSummary.json`은 `Saved/Profiling/Phase12/Baseline`에 있다. 실행/분석 로그는 `Saved/Logs/Phase12_Unit2_BaselineRun.log`, `Phase12_Unit2_Analysis.log`다. `git diff --check` 통과, Content·Config 변경 없음.
- headless 리슨 부하 검증이며 실제 플레이·원격 사격·클라이언트 Ghost 고부하·late join 회귀의 대체가 아니다. 최종 기능/성능 Gate와 Phase 13 보류는 유지한다.

### 다음 구현 단위 제안

사용자 진행 확인 뒤 첫 최적화 단위는 **되감긴 캡슐 위치를 기준으로 보수적인 sphere 1차 검사와 투사체 선분 공통 계산 재사용**이다. 기존 캡슐 정밀 판정과 hit 순서·월드 차폐·팀별 차단을 유지하고 후보 누락 회귀·캡슐 호출 수·trace 없는 교차 반복으로 검증한다. 현재 위치만으로 되감기 대상을 제외하지 않는다. 공간 분할과 되감기 순서/재사용은 이후 단위에서 별도로 검토한다.

## 단위 3a — sphere 1차 검사·선분 공통 계산 재사용 (2026-10-04)

사용자가 단위 2 뒤 진행을 승인하고 기존 공간 그리드 재사용을 요청했다. `LNPEnemySpatialGrid`/비행 그리드를 확인했다. 서버 이동 전 위치·Dying 제외·근사 구면 조회·클라이언트 미구축·되감기 경계 문제가 있어 현재 데이터에 바로 연결하지 않는다. 기존 셀 연속 저장/재구축 구조의 활용과 판정 전용 데이터·보수적 조회·기존 순서 복원은 다음 단위 3b에서 확정한다.

이번 변경은 `FPreparedSegment`(시작/끝/방향/길이 제곱/역수)를 월드 clip 뒤 투사체마다 구성하고, 같은 선분을 서버·클라이언트·패링·보간 진단 정밀 판정에서 공유한다. 캡슐을 감싼 `max(HalfHeight, Radius) + HitRadius` 구의 선분 거리 제곱으로 먼 후보를 제외한다. 작은 여유는 broad phase에만 두고 정확한 캡슐 판정은 그대로 유지한다. 되감기 계산 뒤의 동일한 중심을 사용하며 팀·자기 자신 제외·첫 대상 순서·월드 차폐를 바꾸지 않는다. `LNP.Projectile.SphereBroadPhase=0`은 같은 바이너리의 기준 경로다(기본 1).

에디터/게임 빌드·cook/package 성공(127.15s), 관련 자동화 5개 PASS/failed 0/notRun 0. 기존 17개 명시 사례에서 준비 선분의 결과/출력과 sphere 누락을 추가 검증했고 엔진 대조 10,000개에도 sphere 검증을 넣었다. 추가 10,000개는 점 선분·반지름보다 짧은 캡슐·긴 축 방향·큰 패링 반경을 포함해 broad+exact 결과와 출력이 exact와 동일함을 확인했다. 원격 패키지 사격 자체를 이 테스트로 검증한 것은 아니다.

패키지는 `Saved/SurfaceNavigationPhase12SpherePackage`, 자동화는 `Saved/Tests/Phase12_Unit3`, 빌드/테스트 로그는 `Saved/Logs/Phase12_Unit3*`. `RunPhase12Sphere.ps1`은 동일 패키지 자연 700·추격 800의 trace 없는 Off→On→On→Off 8회, 상세 계측 Off의 trace 대조 4회, On stride64 샘플 2회다. 분석기는 `AnalyzePhase12Sphere.ps1`, 원본은 `Saved/Profiling/Phase12/Sphere`. 다른 두 인구/이동 조합의 반복과 최종 통합 Gate는 후속에 남긴다.

검증 완료: 14회 host/guest 28개 exit 0, probe 112/112 PASS, 모두 동일 패키지 SHA256 `6ADCEE3BD543ED9A2AE9B70C79D8E53F76BD3B5A9F789089688CCF46977D6EA0`, 요청 700/800 전량·30개 무귀속 Active 표본·Nav CSV 행 수/키/순서/대응 timing·sphere 호출 불변식·설정 검증, 분석 exit 0. assert/ensure/fatal·UnknownHits/EnvelopeEscapes/불변 위반 0. 기존 headless Mover/CMC 입력 경고는 유지하며 실제 플레이 검증과 구분한다. 모든 실행 프로세스는 종료했다.

| trace 없는 조건 | Off frame P95 두 반복(ms) | On frame P95 두 반복(ms) | 평균 감소 | On frame/path |
|:---|:---|:---|:---|:---|
| 자연 700 | 20.45 / 21.82 | 14.14 / 14.45 | 32.4% | frame 두 PASS, path 두 PASS |
| 추격 800 | 22.15 / 23.24 | 16.97 / 15.77 | 27.9% | frame 첫 FAIL·둘째 PASS, path 두 PASS |

상세 계측 Off trace의 processor inclusive 호출당 시간은 자연 700 11.037→4.626ms(-58.1%), 추격 800 11.578→5.172ms(-55.3%). trace의 프레임은 Gate 반복에 섞지 않는다. 두 반복 평균 감소는 이 부하의 관측치이며 일반 전투/플레이 성능을 보장하지 않는다. 이전 단위의 별도 패키지 실행보다 같은 바이너리 Off/On을 성능 대조의 우선 근거로 쓴다.

On stride64 샘플의 유효 창은 각각 28개다. 자연 700 후보 11,382,447쌍 중 sphere 11,379,749회·제외 11,379,415회·정밀 334회(제외 99.9971%), 추격 800 후보 11,771,279쌍 중 sphere 11,766,867회·제외 11,766,589회·정밀 278회(99.9976%). 자기 자신 제외 뒤 sphere에 진입하며, sphere 검사 수=제외 수+정밀 호출 수가 일치한다. 원격 발사/실제 되감기는 여전히 0이므로 실제 원격 사격 비용은 미확정이다. 캡슐 정밀 시간은 샘플/시계 비용을 포함해 CPU 점유율로 환산하지 않는다. 후보 순회는 여전히 전수이므로 공간 필터는 후속 단위 3b로 남는다.

단위 3a는 완료했다. 사용자 요청에 따라 다음 구현 전 진행 여부 확인 대기. Phase 13 최종 Gate는 계속 보류다.

## 단위 3b — 서버 현재 위치 캡슐 공간 필터 (2026-10-04)

사용자가 적용안을 승인했다. `FLNPEnemySpatialGrid` 값 타입에 기존 축소 행 저장·재구축·이웃 조회를 옮기고 기존 지상/비행 subsystem은 위임하도록 했다. 기존 `ForEachNeighbor` 계산은 유지한다. 투사체 processor가 독립된 `HitGrid`와 후보 배열 메모리를 갖고, 서버 수집 캡슐 전체를 수집 인덱스 순으로 첫 적용 가능한 탄에서 프레임 1회 구축한다. 지상/비행·Actor/PureEntity·Dying 등 수집 집합은 변경하지 않았다.

현재 위치 적용 조건은 원격 발사 아님·clamp한 ping 되감기 0이다. 원격/되감기 있는 탄은 전수 경로다. 반사 후 새 발사자/되감기 플래그를 다음 판정에 사용한다. 플레이어·패링·클라이언트·스플래시·조준 루프는 그리드 연결 범위에서 제외했다. `LNP.Projectile.SpatialGrid` 기본 1/0 대조, `ValidateSpatialGrid` 기본 0이다.

투사체용 `GatherSphereCandidates`는 선분 중점과 `길이/2 + 최대 캡슐 extent + HitRadius`로 조회한다. 구면 cap은 asin 각반경·보수적 전체 경도 범위·극 전체 경도·wrap으로 계산하며 float 셀 좌표 반올림 여유를 둔다. 원점 항목은 항상 포함한다. 조회 구가 원점을 포함하거나 예상 방문 셀 수가 전체 엔티티 수 이상이면 false를 돌려 전수 검사한다. 각 셀/엔티티는 한 번씩만 방문하므로 중복이 없고 수집 인덱스 정렬로 기존 첫 피격 순서를 유지한다. query 반경 산출/각변환의 제곱근은 투사체당이며 정밀 캡슐 함수의 무제곱근 성질은 유지한다.

첫 빌드는 분리 헤더의 Mass/EntityHandle include 누락과 테스트 난수 double→float aggregate 변환으로 실패했다. 수정 뒤 에디터/게임 빌드·cook/package 성공(149.64s), 자동화 8개 succeeded/failed 0/notRun 0. 추가 두 회귀는 1,000개 구 조회의 포함·정렬/중복·극/이음매/고도/원점/fallback/재구축, 300개 선분의 정확한 캡슐 hit 포함과 첫 대상/좌표를 전수 대조하고 원격 zero ping/양수 되감기·긴/점 선분을 검사한다. 기존 Activity.DormantConsumers가 지상/비행 그리드 조회도 확인한다. 결과는 `Saved/Tests/Phase12_Unit3b`, 빌드/테스트 로그는 `Saved/Logs/Phase12_Unit3b*`다.

패키지는 `Saved/SurfaceNavigationPhase12GridPackage`. `RunPhase12Grid.ps1`은 sphere/선분 재사용을 켠 같은 패키지에서 그리드만 자연 700·추격 800의 Off→On→On→Off 8회, 상세 계측 Off trace 4회, On stride64 샘플 2회, 별도 전수 누락 검증 1회로 비교한다. 원본은 `Saved/Profiling/Phase12/Grid`, 분석은 `AnalyzePhase12Grid.ps1`이 기존 sphere 대조의 공통 검증을 재사용한다. `ProjectileGridProfile`은 샘플 투사체의 전체/후보 수·fallback과 모든 검증 query의 hit 포함을 기록하며 GridBuild 전체·GridQuery 샘플 시간을 추가했다. 검증 시간은 성능 대조에 섞지 않는다.

검증 완료: 15회 host/guest 30개 exit 0, probe 120/120 PASS. 동일 패키지 SHA256 `CE92FF129DD0E4E972B1E66519A3A42243F066F3ACD8A019C1318324D6DB9C0A`, 모든 요청 700/800 전량·30개 무귀속 Active 표본, Nav CSV 행/키/순서/대응 timing, CVar 설정·유효 창 28개·그리드/샘플 호출 불변식·sphere 검사=제외+정밀 횟수 검증 및 분석 exit 0. assert/ensure/fatal·UnknownHits/EnvelopeEscapes·후보 누락 0. 기존 headless Mover/CMC 입력 fault는 유지한다. 실행한 에디터·게임·Insights는 모두 종료했다.

| trace 없는 조건 | Grid Off frame P95 두 반복(ms) | Grid On frame P95 두 반복(ms) | On frame/path |
|:---|:---|:---|:---|
| 자연 700 | 13.05 / 19.82 | 12.93 / 12.17 | 두 반복 모두 PASS/PASS |
| 추격 800 | 21.01 / 21.52 | 14.54 / 14.13 | 두 반복 모두 PASS/PASS |

자연 700 Off 첫/둘째는 Nav 소비 P50/P95 1.181/1.522→1.881/2.413ms 등 다른 비용도 변동한다. Off 분산과 실행별 Nav/Actor/네트워크 궤적 차이를 그대로 보존하며 프레임 평균 감소를 그리드만의 순수 이득으로 단정하지 않는다. On은 네 반복 모두 Gate를 통과하지만 이 두 조건을 Phase 13의 전체 인구/활동/비행/원격 사격 Gate로 확대하지 않는다. 추격 800 SampledOn은 frame 15.73ms PASS이나 path FAIL이다. 상세 계측/trace 실행은 trace 없는 Gate에 섞지 않고 경로 불안정 기록도 남긴다.

상세 계측 Off trace의 투사체 processor inclusive는 자연 700 6.360→1.542ms/호출(-75.8%), 추격 800 7.300→1.795ms/호출(-75.4%). 구축/조회/정렬을 포함한 부모 시간이다. 기존 별도 패키지 수치보다 이번 동일 바이너리 그리드 Off/On을 우선 비교 근거로 쓴다. 샘플에서는 자연 700 그리드 후보 평균 15.314/700(-97.812%), 추격 800 27.356/800(-96.580%)이며 fallback 0이다. 후보 계수는 전부 반환된 적 후보이며 실제 첫 hit로 중단한 순회 횟수·플레이어 후보가 섞인 Targets 계수와 구분한다.

GridBuild 전체 계측은 0.0611/0.0671ms/frame, GridQuery의 샘플 raw는 585/926ns/query(조회 구 계산·정렬 포함). 시계/계측 오버헤드를 포함한 참고치이며 부모 범위와 합산하지 않는다. SampledOn의 sphere는 줄어든 후보 안에서 다시 약 99.873%/99.925%를 제외했다. 원격 발사/실제 히스토리 되감기 샘플은 0이므로 이 경로의 CPU 이득은 주장하지 않는다. 원격/양수 되감기·긴/원점 조회 fallback은 자동화로 확인하며 실제 게스트 발사/반사 플레이는 후속 통합 검증에 남는다.

별도 Validation의 유효 창 28개는 그리드 조회 전수 대조 1,139,222회에서 정밀 적 캡슐 hit 후보 2,613쌍을 모두 포함했다. 이는 피해 적용 수나 고유 적 수가 아니다. validationErrors 0이고 실제 실행 전체에도 ProjectileGridMismatch가 없다. 검증 비용이 있는 프레임 14.26ms는 성능 Gate 반복에 포함하지 않는다. 구조화 요약은 `Saved/Profiling/Phase12/Grid/Summary.json`, 실행/분석 로그는 `Saved/Logs/Phase12_Unit3b_GridRun.log`, `Phase12_Unit3b_Analysis.log`다.

단위 3b 완료: 서버 현재 위치 공간 필터는 채택한다. 다음 구현 단위 전 사용자 진행 확인 대기. 원격 되감기 경로의 실제 사격 계측/개선 여부 판단과 최종 통합 회귀·전체 조건 Gate는 후속이며 Phase 13은 계속 보류다.
