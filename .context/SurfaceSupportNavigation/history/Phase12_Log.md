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

## 원격 사격 후속 진단 (2026-10-05)

사용자가 다음 단위 진행과 에디터/MCP 연결을 승인했다. 실제 원격 사격 근거가 없던 경로를 먼저 진단했다. MCP로 `TestMap03`·PIE 정지·sphere/grid 기본 1·ProfileStride 0을 확인했다. Slate 콘솔 Type은 false를 반환했고 CVar가 바뀌지 않아 사용자에게 수동 설정을 요청했다. 사용자가 ProfileStride 64를 켜고 리슨 2P 게스트에서 사격한 뒤 PIE를 종료했다. 소스·에셋 변경이나 추가 빌드/cook은 하지 않았다.

### 표본과 분석 검증

`AnalyzePhase12RemoteShots.ps1`으로 실행 중인 에디터 로그를 공유 읽기로 고정 사본에 보존하고 분석했다. 원본은 `Saved/Profiling/Phase12/RemoteShots/Editor.log`, 결과는 `Summary.json`이다. 사본 SHA256은 `8C4617D02E64864BE436B4348EE34B7BB054060A55651FB6E84A3F39A6662CAC`다. UTC 로그 날짜는 10-04이며 사용자 시간대의 실행일은 10-05다.

- 서버 계측 145개 창, 합계 146.953534초·5,808프레임·투사체 프레임 331,962개·표본 5,187개다. 이는 사격 지속시간을 직접 잰 값이 아니라 초기화/호스트/NPC 사격도 포함한 계측 창의 합이다.
- 원격 표본이 양수인 95개 창은 합계 96.243364초다. 첫 창 시작 3.535010초·마지막 창 끝 147.353534초로 사이에는 원격 표본 0인 창도 있다. 60초 연속 사격을 보증하지 않지만 실제 원격 경로 비용 진단의 표본은 확보했다.
- 원격 표본 308개, 실제 History 조회/rewindApplied 397,229회다. **308은 고유 발사 수나 명중 수가 아니라 샘플링된 투사체 프레임 수**다. 평균 약 1,290회 History 조회/원격 표본으로 기존 전수 후보 경로가 관측됐다.
- 전체 sphere 검사 411,543회·제외 411,449회·정밀 캡슐 호출 94회다. 원격 양수 창에는 sphere 407,068회·제외 406,998회·정밀 70회가 있으나 호스트/NPC 작업도 포함하므로 원격 전용 명중률로 해석하지 않는다.
- 145개 창 모두 stage 10개·키 중복 없음, Targets calls=candidates, World calls=projectiles, History calls=rewindApplied, sphereTests=sphereRejected+Capsule calls를 검증했다. Join succeeded와 PIE 종료를 확인했고 분석 exit 0이다. assert/ensure/fatal/Unhandled Exception/GridMismatch 로그는 0이다. 검증 CVar가 꺼져 있어 GridMismatch 0을 전수 대조로 해석하지 않는다.
- 기존 production 스폰 부족 로그는 두 세트에서 적 요청 360 대비 359/357이다. 전량 배치 성능 Gate가 아니다. 종료 후 MCP에서 CompensateInterpolationLag=1·MeasureInterpolationLag=0·PIE 정지를 확인했다. 상세 계측 64는 0으로 복구를 요청한다.

### 비용 해석과 다음 단위

History raw 누적은 37.9895ms/397,229회(약 95.6ns/호출), Rewind raw는 61.5775ms/411,543회(약 149.6ns/호출), Targets raw는 93.3833ms/412,937회(약 226.1ns/호출)다. 이는 **stride64 표본의 중첩 범위·시계 비용을 포함한 시간**이며, 합산하거나 실제 전체 프레임 CPU·최적화 이득으로 환산하지 않는다. 캡슐 정밀 비용이 이 표본에서 큰 항목은 아니며, sphere 이전에 실행되는 전수 되감기 조회의 반복을 줄일 후보는 확보했다.

다음 제안은 단위 3c의 히스토리 결과 재사용이다. 한 processor 실행 동안 같은 적의 **정확히 같은 QueryTime**에 대한 GetInterpolatedLocation 결과만 재사용한다. 현재 위치를 기준으로 과거 피격 후보를 제외하지 않고, 발사자별 ping·viewer 거리 보간·IsWanderingAt 분기를 그대로 계산한다. 배회 평가에 쓰는 Surface snapshot과 수명은 같은 실행 범위로 제한한다. 현재 계측에는 반복 키/재사용 적중률이 없으므로 구현 후 Off/On·누락/출력 대조·메모리/키 비용을 측정해야 하며 이득을 미리 확정하지 않는다.

이번 진단은 완료했다. 단위 3c 구현 전에 사용자 진행 확인을 받는다. 실제 원격 사격의 정답 명중/패링/월드 차폐·배회 구간·지연 주입 회귀, trace 없는 반복 성능, 월드 query/cache 판단 및 최종 통합 검증은 남는다. Phase 13의 최종 Gate는 계속 보류다.

## 단위 3c — 히스토리 조회 결과 재사용 구현·자동 회귀 (2026-10-05, 실측 대기)

사용자가 진행을 승인하고 상세 계측을 0으로 복구했다. 캐시 계측 구조 크기가 바뀌어 전체 빌드를 위해 에디터 저장·종료를 요청했고 사용자가 종료를 확인했다. 구현은 `FCollectedEnemy`마다 `FLNPProjectileHistoryCache`를 하나 두는 방식이다. 마지막 QueryTime/역사 위치 한 쌍만 보관하고 정확히 같은 double 시각일 때만 재사용한다. 다른 적·다른 시각·다음 실행은 공유하지 않는다. 별도 TMap이나 장기 캐시는 만들지 않았다. viewer 거리·ping·배회 여부를 기존대로 먼저 계산하며, 현재 위치 기반 후보 제외는 추가하지 않았다. 플레이어·보간 보정의 비교 계측은 직접 조회를 유지한다.

`RewindCache`는 실측 전 기본 0, `ValidateRewindCache`는 기본 0이다. 후자는 모든 cache hit의 역사 위치를 같은 snapshot의 새 조회와 정확히 대조한다. `ProjectileRewindCacheProfile`의 requests/hits는 상세 계측이 켜져 있을 때 전체 조회를 센다. validationQueries/errors는 모든 검증 hit를 센다. History.calls/rewindApplied는 실제 표본 계산 횟수이며 캐시 재사용은 여기에 가산하지 않는다. 이 전수 계수와 stride 표본 시간을 섞어 실제 CPU로 환산하지 않는다.

### 확보한 검증

- 게임·에디터 전체 빌드/cook/package 성공. 최초 실행은 167.41초였고 전체 조회 계수 변경을 최종 패키지에 반영한 재빌드/cook은 74.97초·exit 0이다. 로그는 `Phase12_Unit3c_BuildCookRun.log`와 `_Final.log`다. fixture LVI 2개·조건부 패치 맵·TestMap03 및 fixture 폴더를 명시적으로 cook했다.
- 최종 패키지는 `Saved/SurfaceNavigationPhase12RewindPackage`, exe SHA256 `517B885C2D8939FCD142E1786880F8587538AE83D3C8A4286ED40786AE618F3C`다.
- 관련 자동화 10개 succeeded/failed 0/notRun 0·프로세스 exit 0. 새 `LNP.HitDetection.RewindCache.HistoryAndCapsuleEquivalence`는 두 적의 선형/비행 배회 이력, 최고령/최신 clamp, 미세 시각 차, 비연속 재조회, 다음 실행의 이력 변경, 캡슐 결과·출력점·첫 타겟 순서를 대조했다. 연속 동일 시각 8회 hit·다른/재방문 시각 12회 실제 계산을 확인했다. 기존 SegmentCapsule 3개·SpatialGrid 2개·WorldCollision 2개·DormantConsumers·WanderReplication.Rewind도 통과했다. 결과는 `Saved/Tests/Phase12_Unit3c`, 로그는 `Saved/Logs/Phase12_Unit3c.log`다.
- 캐시 On·상세 계측 Off인 패키지 리슨 2P N700 1회와 분석을 통과했다. host/guest exit 0·probe 8/8 PASS, frame P95 10.34ms·frame/path PASS·UnknownHits/EnvelopeEscapes 0이다. 원본/요약은 `Saved/Profiling/Phase12/Rewind/Smoke`, 실행/분석 로그는 `Phase12_Unit3c_Smoke.log`/`_SmokeAnalysis.log`다. 실제 게스트 사격이 없는 편성이므로 원격 캐시 분기·성능 이득의 증거로 쓰지 않는다. Phase 13 최종 Gate로 확대하지 않는다.
- 기존 원격 진단 사본을 확장 분석기로 다시 읽어 exit 0을 확인했다(`Rewind/AnalyzerRegression`). 분석기는 반복 PIE의 마지막 리슨 세션만 선택하고 캐시 계수/검증 오류를 함께 검사한다. 기존 baseline 사본·요약은 덮어쓰지 않았다.

### 바로 다음 검증

에디터 재실행 뒤 ProfileStride 64·RewindCache 1·ValidateRewindCache 1로 실제 리슨 2P 게스트 사격을 요청한다. 종료 후 로그를 별도 폴더에 보존하고 전체 재사용 대조 0 오류·실제 cache hit·원격 표본을 확인한다. 이어 validation을 끈 같은 바이너리의 Off/On 비용 대조로 채택 여부를 판단한다. 프레임 시간 성능 판정은 상세 계측을 끈 반복/trace 근거로 따로 확보한다. 원격 사격 대조가 아직 없어 단위 3c는 완료하거나 기본 켜짐으로 채택하지 않았다. 다음 구현 단위 진행 확인은 단위 3c 검증이 끝난 뒤 받는다.

### 실제 게스트 사격 캐시 재사용 대조 (2026-10-05)

사용자가 에디터를 다시 열고 요청한 CVar 3개를 설정한 뒤 게스트 사격·PIE 종료를 수행했다. 원본 설정 명령·실제 리슨/클라이언트 Join·PIE 종료를 확인했다. `AnalyzePhase12RemoteShots.ps1` 분석 exit 0, 보존한 원본/요약은 `Saved/Profiling/Phase12/Rewind/Validation`, 사본 SHA256 `748C5B9F01A46569BCEA0C87DAAE9DC2D2A796B1354DC49513635B647B61B3A1`다.

- 서버 창 233개/235.579461초·10,578프레임·투사체 프레임 표본 3,377개 중 원격 표본 647개다. 원격 양수 창은 176개/177.907168초다. 창의 합이며 60초 연속 사격/고유 발사 수로 해석하지 않는다.
- 전체 cache requests 51,943,699회·hits 29,238,995회, 재사용률 56.2898%다. 모든 hit를 같은 시각/snapshot의 새로운 GetInterpolatedLocation과 대조했다. validationQueries=hits이며 validationErrors=0이다. 이 계수는 stride64 샘플이 아니라 전체 조회/재사용이다.
- 실제 표본 History calls/rewindApplied 363,763회, sphere 818,899회·제외 818,853회·정밀 46회다. 233개 창의 stage 키/호출 불변식과 cache hits<=requests를 통과했다. assert/ensure/fatal/Unhandled Exception/GridMismatch/RewindCacheMismatch는 0이며 MCP로 PIE 정지를 확인했다. grid 검증은 켜지 않아 이번 실행은 그리드 전수 대조가 아니다.
- production 스폰 부족 로그는 요청 480→464·360→348·360→353이다. 앞선 원격 진단의 편성과 같다고 가정하지 않고 전량 배치 Gate로 쓰지 않는다.

재사용의 정확성과 실제 반복 키 비율은 확보했다. validation이 모든 cache hit에 새 조회를 실행하므로 기록된 비용을 캐시의 실제 CPU 이득으로 판정하거나 이전 진단과 직접 대조하지 않는다. 다음은 ProfileStride 0·ValidateRewindCache 0에서 Off/On 실제 사격의 CPU/frame trace다. `Trace.File [Path] [ChannelSet]`/`Trace.Stop` 문법은 UE 5.8의 TraceAuxiliary.cpp에서 확인했다. 우선 Off1 기록을 요청하고, 완료 후 보존/확인한 뒤 On 실행을 요청한다. 단위 3c는 아직 진행 중이며 기본 0을 유지한다.

### Off1 사격 trace 수집 중간 확인 (2026-10-05)

사용자가 Off 사격을 마쳤다고 알려왔다. ProfileStride/ValidateRewindCache/RewindCache=0 설정이 trace 시작 전에 반영된 명령과 MCP 현재 값을 확인했다. Trace.File 시작은 UTC 16:30:41.727, PIE 종료는 16:35:17.489이며 이 사이 계측 창은 약 275.76초다. 종료 후에도 Off1.utrace 크기가 증가하고 Trace.Stop 기록은 없어서 파일 종료/완전성은 아직 판정하지 않았다. 사용자에게 먼저 Trace.Stop을 요청한 뒤 같은 설정의 캐시 On 기록을 이어간다. 당시 로그 사본은 `Saved/Profiling/Phase12/Rewind/Cost/Off1_Editor.log`다. 사격 지속시간이나 유효 비교 구간은 trace 분석 후 확정한다.

### Off/On CPU trace 비용 대조와 단위 3c 판단 (2026-10-05)

Off의 Trace.Stop은 UTC 16:39:52.648, On의 시작/정지는 16:40:29.802/16:45:25.111이며 On PIE 종료는 16:45:27.360이다. 두 실행 모두 trace 전에 ProfileStride 0·ValidateRewindCache 0·RewindCache 각각 0/1을 설정했고 그 뒤 값 변경은 없었다. Off는 PIE 종료 뒤 에디터 대기를 더 포함한다. 당시 전체 로그는 Cost/On1_Editor.log에 보존했고 두 파일은 정지된 상태에서 SHA256을 기록했다.

- Off1.utrace SHA256: C7659C50108590EB45EA1F84A16CB087946AB0474EA6565685F752378C7DBCC5.
- On1.utrace SHA256: B6D07DC4328C5C4EE5A978B13B04CD6ADDA65D0E45D27BCADF4CC7FE14676E66.
- Insights export 두 프로세스 exit 0, 타이머 통계·이벤트 CSV를 보존했다. AnalyzePhase12RewindCost.py 분석 exit 0이며 응답 파일·원본·Summary.json은 Saved/Profiling/Phase12/Rewind/Cost에 있다. 타이머 필터 CSV에 함께 나오는 Game Frames를 프레임 분모로 사용했다.

분석 전에 첫 서버 실행에서 10초를 제외하고 마지막 5초도 제외하는 규칙 및 같은 길이 최대 180초/30초 블록을 정했다. processor 부모와 같은 thread·포함 구간의 Server 자식 이벤트로 서버/클라이언트를 분리했다. 부모에 포함되지 않은 Server 이벤트는 0이다. Off의 에디터 대기는 서버 실행 구간에서 빠진다. 비교 구간은 Off 668.507345~848.507345초, On 1256.765140~1436.765140초다.

| 항목 | Off | On |
|:---|---:|---:|
| 서버 투사체 processor 실행 수 | 8,103 | 8,432 |
| 서버 processor inclusive 평균(ms/실행) | 0.548340 | 0.363356 |
| 서버 processor inclusive P95(ms/실행) | 1.267500 | 0.784000 |
| ServerProjectiles 합/서버 실행 수(ms) | 0.457167 | 0.271332 |
| ServerGridBuild 합/서버 실행 수(ms) | 0.034588 | 0.013366 |
| 에디터 Game Frames 수 | 8,103 | 8,430 |
| 에디터 Game Frames P95(ms) | 26.6668 | 23.5972 |

평균 CPU 33.74%·P95 38.15% 감소는 관측값이다. 수동 사격량·NPC 상태가 같다는 보장이 없고 cache와 무관한 GridBuild도 달라 **그 감소 전체를 캐시의 순수 이득이나 원격 발사당 이득으로 확정하지 않는다**. Off의 production 스폰 부족은 480→477, On은 120→118·480→479·360→354였다. 초기 배치부터 다르다. 같은 실측 편성의 교차 반복·발사 수 정규화가 없다. 30초 블록 평균도 Off 0.395~0.802ms, On 0.257~0.461ms로 변동했다. 서버/클라이언트가 같은 에디터에서 실행된 Game Frames는 패키지 frame Gate를 대체하지 않는다. On의 구간 시작에 걸친 부모 2개는 완전한 Game Frame 분모 밖이며, per-frame 집계는 이를 제외했다. 중첩된 자식 시간을 부모와 합산하지 않는다.

단위 3c의 구현·정확성·실제 재사용·비용 실험은 마무리한다. **기본 활성화는 보류하며 RewindCache=0을 유지한다.** 현재 위치 sphere/grid는 채택 상태를 유지한다. 통제된 되감기 부하 대조가 없으므로 최종 기본 성능 Gate에 캐시 이득을 더하지 않는다. 에디터의 실험용 RewindCache=1은 사용자에게 0 복구를 요청한다. 다음 단위 4의 월드 query 비용 검토와 기본 경로 통합 회귀/최종 패키지 성능 검증은 사용자 진행 확인 후 착수한다. Phase 13은 계속 보류다.

### 세션 종료와 새 세션 인계 (2026-10-05)

사용자 요청으로 다음 단위는 새 세션에서 진행한다. Current.md의 완료 이력 누적과 이미 끝난 사격/trace 요청을 정리하고, 단위 3c 결론·최신 패키지/분석 위치·단위 4 첫 행동을 남겼다. Phase 실행서에도 새 세션 착수 범위와 기본 측정 설정을 반영했다. Phase 12 전체와 Phase 13 최종 성능 Gate는 완료 처리하지 않는다.

마지막 에디터 확인 값은 ProfileStride 0·ValidateRewindCache 0·RewindCache 1이며 PIE/trace는 종료했다. RewindCache 0 복구를 사용자에게 요청했으나 완료 응답은 없다. 새 세션에서 실제 연결 상태와 CVar 값을 확인한다. 새 측정은 sphere/grid On·rewind cache Off인 채택 기본 경로를 명시한다.

이번 커밋 범위는 단위 3c 히스토리 재사용·전체 조회/검증 계수·자동화 회귀, 패키지 실행기 옵션과 실제 원격 사격/CPU 분석기, 관련 설계·측정·인계 문서다. 로컬 패키지/대용량 trace/측정 산출물은 포함하지 않는다. 문서 정리 이후 실행 코드는 변경하지 않으며 앞서 통과한 빌드/cook·자동화 10개·리슨 2P·실제 사격 대조 결과를 보존한다.

## 단위 4 — 최신 기본 경로 월드 query 검토·최종 패키지 반복 검증 (2026-10-05)

사용자가 새 세션에서 Phase 12 재개를 요청했다. README/Current/Decisions·실행서·RuntimeCollision/ValidationAndMigration과 실제 코드/실행기를 확인했다. 세션 시작 Git 작업 트리는 깨끗했다. MCP read-only 호출로 연결·PIE 정지, ProfileStride/ValidateSpatialGrid/ValidateRewindCache/RewindCache=0·SphereBroadPhase/SpatialGrid=1을 확인해 이전 세션의 RewindCache 복구 미확인 항목을 해소했다. 사용자 저장·에디터 종료 응답 뒤 성능 측정을 시작했다.

### 범위와 실행물

- 런타임 C++·에셋은 변경하지 않았다. 단위 3c 최종 빌드/cook 패키지 `Saved/SurfaceNavigationPhase12RewindPackage` exe SHA256 `517B885C2D8939FCD142E1786880F8587538AE83D3C8A4286ED40786AE618F3C` 일치를 확인하고 그대로 실행했다. 이번 단위에서 다시 빌드/cook했다고 보고하지 않는다.
- `RunPhase13Gate.ps1`은 두 projectile validation CVar를 Off일 때도 `=0`으로 명시한다. 최종 실행기는 ProfileStride=0·SphereBroadPhase=1·SpatialGrid=1·RewindCache=0을 명시하며 검증은 Off다. stride64 상세 계측만 별도 진단 그룹이다.
- 재현 실행기 `Saved/Profiling/Phase12/Final/Run.ps1`은 자연/추격 각각 700→800→800→700 Gate 8회, 같은 네 조건 ProfileStride 0 trace 4회, N700/N800Chase stride64 상세 계측 trace 2회다. 기존 Grid 패키지 실행기를 최신본으로 간주하지 않았다. 같은 seed가 동일 행동 상태를 보장하지는 않는다.
- `AnalyzePhase12Final.ps1`은 기존 GroundPopulation/Gate 분석기를 재사용해 실제 총수·활동 표본·같은 패키지·Nav 요청/timing 원본의 행/키/순서를 검증하고, host/guest의 기본 CVar·계측 Off·상세 계측 10개 stage 완결성/호출 수·샘플 비율을 확인한다. parser 검사와 실제 14회 산출물 분석 exit 0이다.
- 원본/요약은 `Saved/Profiling/Phase12/Final/{Gate,Trace,Sampled}`, 상위 `Summary.json`·`ProjectileSummary.json`과 `EvidenceSHA256.json`에 보존했다. SHA256 목록은 각 그룹의 로그·trace·CSV·응답/exit 파일을 포함한다. 로컬 대용량 산출물은 Git에 넣지 않는다.

### trace 없는 최종 기본 성능

| 조건 | frame P95 두 반복(ms) | frame/path |
|:---|:---|:---|
| 자연 700 | 15.10 / 13.63 | 두 PASS / 두 PASS |
| 자연 800 | 15.51 / 13.89 | 두 PASS / 두 PASS |
| 추격 700 | 12.95 / 13.24 | 두 PASS / 두 PASS |
| 추격 800 | 13.66 / 14.51 | 두 PASS / 두 PASS |

Gate 8회와 진단 6회는 host/guest 28개 exit 0·probe 112/112 PASS다. 실제 지상 700/800 전량·비행 0·무귀속 전량 Active·30 활동 표본, 동일 exe SHA256과 Nav CSV를 검증했다. assert/ensure/fatal/Unhandled Exception·UnknownHits/EnvelopeEscapes/LayerJumps/violations·ProjectileGridMismatch/RewindCacheMismatch는 0이다. 기존 Mover 초기화/out-of-band·CMC 추출 오류와 headless 입력 fault는 남아 있으며 새 불변 위반이나 이번 변경의 오류로 분류하지 않는다. trace/stride64의 프레임은 위 Gate 반복에 섞지 않는다.

### 현재 월드 query 비용과 판단

ProfileStride 0인 네 trace의 서버 processor inclusive 호출당 값은 자연 700/800 1.5366/1.6084ms, 추격 700/800 1.7048/1.8254ms다. 병렬 부모/하위 범위와 다른 소비자의 exact 비용은 중복 합산하지 않는다. 이전 단위 2의 월드 wrapper 수치를 현재 병목 근거로 사용하지 않았다.

`ProjectileMandatory`는 약 502.6~502.7 query/프레임, Report의 반올림 평균 1.52~1.56us이며 약 0.764~0.784ms/프레임의 query 비용이다. 해당 클래스의 lock probe 합은 약 0.031~0.032ms/프레임이다. 기존 `LNPLoadBaseline`은 코드로 LockProbe=1을 강제해 query 직전에 추가 읽기 락을 잡는다. 이번 분석도 실제 Report의 LockProbe=1을 확인했다. 기존 기준선과 같은 harness 조건이며 이 결과를 진단 Off인 일반 플레이 비용으로 확대하지 않는다.

stride64 N700/N800Chase는 각각 완결된 28개 창·2,399/2,082프레임·1,204,008/1,047,259 투사체 프레임·18,812/16,363표본이다. World wrapper는 전체 투사체를 계측하며 호출 평균 1.752/1.761us·약 0.879/0.886ms/프레임이다. 시계/scope 비용을 포함하고 Targets/Filter/Rewind/Capsule 등은 중첩·샘플 확장 추정이므로 합산하거나 순수 CPU로 단정하지 않는다. RemoteSamples/RewindApplied는 양쪽 0이다. 합성 현재 위치 부하이며 실제 원격 gameplay/되감기 검증은 별도다.

최신 기본 경로의 모든 trace 없는 frame/path Gate가 통과했고 월드 query 추가 최적화의 순이득은 입증하지 않았다. 이번 단위에서는 horizon/별도 hit 캐시를 추가하지 않고 서버·Ghost·탄도 가이드의 공통 exact segment·첫 월드 hit·차폐 경로를 유지한다. RewindCache 기본 활성화 보류도 유지한다. 이 판단은 모든 미래 편성의 병목 부재나 Phase 13의 비행/혼합/총수 합격을 의미하지 않는다.

### 관련 자동화와 남은 사용자 검증

같은 빌드의 UnrealEditor-Cmd로 자동화 12개 succeeded/failed 0/notRun 0/succeededWithWarnings 0·프로세스 exit 0이다. 기존 단위 3c 10개(SegmentCapsule 3·SpatialGrid 2·RewindCache 1·DormantConsumers·WanderReplication.Rewind·ProjectileArc·EarliestHit)에 WorldCollision.Api/DynamicMarkerHit를 추가했다. 에셋을 저장하는 테스트는 포함하지 않았다. 실행기/결과는 `Saved/Tests/Phase12_Unit4`, 로그는 `Saved/Logs/Phase12_Unit4.log`다.

패키지 측정과 자동화 종료 뒤 사용자에게 에디터 재실행·TestMap03 리슨 2P 실제 플레이를 요청했다. ProfileStride 64·RewindCache 0의 별도 진단에서 호스트/게스트 Actor/PureEntity 조준·피격, 게스트 이동/배회 적 사격 약 60초, 벽 뒤 직격 차폐(스플래시 구분), 투사체 패링/반사 지속 비행·피격·양쪽 Ghost 정리를 확인한다. 사용자 응답과 해당 로그/MCP 확인은 아직 없다. 자동화/합성 부하만으로 실제 패링·조준 gameplay를 확인했다고 하지 않는다.

단위 4와 Phase 12는 사용자 통합 검증까지 완료 처리하지 않는다. 검증 후 ProfileStride 0 복구·완료 인계 문서를 정리하고 다음 작업 진행을 확인한다. Phase 13은 별도 보류이며 D-054 총수/비행 200을 유지한다.

### 사용자 통합 플레이 확인·단위 4 및 Phase 12 완료 (2026-10-05)

사용자가 실제 호스트·게스트 양쪽에서 기본 조준·피격·월드 차폐·패링 넉백·패링 반사를 테스트했고 어색하거나 이상한 점을 발견하지 못했다고 확인했다. 화면의 통합 플레이 결과는 이 사용자 확인 범위로 기록한다. 모든 무기·모든 Ghost 수명주기·모든 지연/LOD 조합을 전수 확인했다고 확대하지 않는다.

MCP로 PIE 종료와 ProfileStride 64·sphere/grid 1·RewindCache/두 validation 0을 확인했다. `AnalyzePhase12RemoteShots.ps1 -OutputDirectory Saved/Profiling/Phase12/Final/Gameplay` 분석 exit 0이며 고정된 Editor.log/Summary.json을 보존했다. 원본 사본 SHA256은 `E657F316FC895F97F9E87241EBBCF22B0788290180259E81917633BA4174C255`다.

- 마지막 리슨 세션의 서버 창 461개·466.081832초·21,233프레임, 투사체 프레임 276,715개·stride64 표본 4,324개다. 창 시간 합이며 연속 사격 시간이나 고유 발사 수가 아니다.
- 원격 투사체 프레임 표본 398개, 원격 양수 창 161개·162.729801초다. 서버 히스토리 조회/rewindApplied 499,238회를 확보했다. 원격 양수 창에도 호스트/NPC 사격이 포함되므로 모든 비용을 원격 발사당 비용으로 정규화하지 않는다.
- sphere 510,361회·제외 510,291회·정밀 캡슐 70회다. stage 완결성·호출 불변식 통과, JoinedClient/PIEStopped true·RuntimeErrors 0이다. RewindCache Off여서 cache requests/hits/validation 계수는 모두 0이다. 이번 실행은 캐시나 그리드의 전수 대조가 아니며 앞선 단위의 대조 근거를 보존한다.
- 고정 로그 전체에 투사체 패링 성공 49회·근접 패링 성공 6회·PureEntity HP 기록 629개·GE HP 기록 321개가 있다. 이는 로그 이벤트 수이며 고유 투사체/적 수나 클라이언트별 피해 확정 건수로 해석하지 않는다. 실제 패링 이벤트와 피격 경로가 실행됐다는 보조 근거다.

기능 진단 종료 뒤 MCP Slate의 콘솔 입력으로 `LNP.Projectile.ProfileStride 0`을 실행하고 SearchCVars 실제 값 0을 확인했다. RewindCache와 두 validation은 0·sphere/grid는 1을 유지한다. 임시 Slate observer 두 개는 제거했다. 진단 원격 사격의 에디터 프레임/계측 CPU를 최종 패키지 Gate와 혼합하지 않는다.

단위 4와 Phase 12 완료 조건을 충족했다. 채택한 정확한 캡슐·sphere·서버 현재 위치 공간 필터는 유지하고, RewindCache 기본 활성화 및 월드 horizon/별도 hit 캐시는 채택하지 않는다. 최종 코드/패키지·자동화 12개·14회 리슨 2P·trace 없는 700/800 교차 Gate·실제 사용자 통합 플레이/원격 로그와 한계를 Current/Phase12/Roadmap 및 Phase13 실행서에 인계했다. 문서와 분석 실행기만 수정했고 런타임 바이너리/에셋은 단위 3c 최종 빌드/cook 그대로다.

Phase 13의 최종 성능·활동 대역 총수·production/항상 활성·비행 관통/혼합 부하와 D-054 검토는 남아 있다. Phase 13은 자동 완료하거나 재개하지 않고 다음 구현 단위 진행 확인을 받는다. 재개 첫 단위는 최신 기본 패키지의 Pod 귀속 production 대조이며 완료한 무귀속 지상 반복은 새 변경/실패 없이 반복하지 않는다.

### 완료 뒤 근접 되감기 입력의 워커 조회 제거 (2026-10-05)

사용자가 근접 판정 워커 안의 PlayerState 핑·IsLocallyControlled 조회를 원거리 발사 시점 캐시와 유사하게 고친 뒤 이번 세션 변경을 커밋하도록 요청했다. 소스 세 파일만 수정했다. `FLNPWeaponTraceFragment`에 CachedRewindSeconds/bInstigatorIsRemoteClient를 추가하고 게임 스레드 NotifyBegin의 서버 PlayerState가 있을 때 RTT/2(상한 200ms)와 원격 여부를 저장한다. 워커는 캐시만 읽으며 공격자 Mass→Actor→PlayerState 역조회와 그 때문에 필요한 include를 제거했다. PureEntity 적은 기본값 0/false를 사용한다. 근접 보간 지연 가산 Off·클라이언트 로컬 예측·판정 형상/순서 규칙은 유지한다. 핑은 판정 프레임마다 갱신하지 않고 스윙 시작 값으로 고정하는 요청한 동작 변경이다.

최초 전체 빌드는 MaxPingRewindSeconds 정의 헤더 누락으로 실패했다. include를 LNPPositionHistoryFragment.h로 바로잡은 뒤 `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 빌드 성공·exit 0이다. 빌드 로그는 `Saved/Logs/MeleeThreadCache_Build.log`에 보존했다. 관련 기존 자동화 12개 succeeded/failed 0/notRun 0/warnings 0·프로세스 exit 0이며 결과는 `Saved/Tests/MeleeThreadCache`, 로그는 `Saved/Logs/MeleeThreadCache.log`다. 근접 processor 소스에 GetPlayerState/GetPingInMilliseconds/IsLocallyControlled/ActorSubForRewind가 남지 않은 것과 diff 공백 검사를 확인했다. 이 자동화는 기존 판정/되감기 회귀이며 실제 NotifyBegin→원격 근접 공격의 새 플레이 증거로 확대하지 않는다.

이 후속 수정으로 game 패키지를 다시 빌드/cook하거나 실제 PIE 플레이를 다시 수행하지 않았다. 위 Phase 12 완료 패키지·성능·사용자 통합 플레이 근거는 후속 수정 전 코드다. Phase 13 재개 시 이 수정을 포함한 새 game 패키지를 빌드/cook하고 SHA256·실제 측정 편성/설정을 다시 기록해야 한다. 사용자의 작은 수정/커밋 요청 범위를 넘어 Phase 13 구현이나 추가 성능 측정을 시작하지 않는다.
