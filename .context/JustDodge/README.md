# 저스트 회피 작업 안내

> 상태: 활성
> 역할: 이 작업의 진입점과 문서 라우팅
> 마지막 구조 갱신: 2026-10-26

## 1. 목적

이 폴더는 대시에 **회피 무적**과 **저스트 회피**를 붙이는 작업을 여러 세션에 걸쳐 관리한다.

- **회피 무적:** 대시가 시작된 직후 짧은 시간(무적 창) 동안 플레이어 피격 판정을 무시한다. 모든 공격에 적용된다.
- **저스트 회피:** 공격이 닿기 직전에 대시하면(저스트 창 안에서 공격이 패링 반경에 들어오면) 짧은 공격속도·이동속도 버프를 받는다. 연속으로 성공해도 중첩되지 않고 시간만 갱신된다.
- **비주얼:** 성공한 순간 "슈슉" 하고 사라졌다 나타나는 연출(순보), 잔상, 화면 연출, 버프 오라를 넣는다. 비주얼은 맨 마지막의 **독립 트랙(V)**으로 진행하고, 일부 항목은 그래픽스 작업으로 넘긴다(J-012).

범위 밖은 다음과 같다.

- 사운드: 프로젝트에 사운드 에셋이 아직 없다.
- 공중 대시: 의도된 제한이다([TechDesign_CharacterMovement.md](../TechDesign_CharacterMovement.md) §5.3).
- 적의 회피.
- 버프 남은 시간 HUD.

상세는 `Roadmap.md` §5에 있다.

## 2. 세션 시작 시 필수 문서

1. `README.md`
2. `Current.md`
3. `Decisions.md`

이 세 문서를 읽은 뒤에는 아래 라우팅 표에서 지금 작업에 필요한 문서만 더 읽는다. `history/` 폴더와 현재 단계에서 먼 `phases/` 문서는 기본으로 읽지 않는다.

## 3. 문서 라우팅

| 작업 | 추가로 읽을 문서 |
|:---|:---|
| 단계 조정·Phase 전환·그래픽스 이관 항목 | `Roadmap.md` |
| 현재 Phase 구현 | `Current.md`가 가리키는 `phases/PhaseNN_*.md` |
| 프래그먼트 필드·Judge 확장·경로별 처리·버프 GE·지연 분석 | `design/Architecture.md` |
| 비주얼 트랙(V) — 순보·잔상·오라, 색 규약, 그래픽스 경계 | `design/VisualDesign.md` |
| 검증 시나리오·PIE 절차·로그 판정 | `design/Validation.md` |
| 패링·가드 판정, 상태 미러 패턴, RTT 보정 | [TechDesign_ParrySystem.md](../TechDesign_ParrySystem.md) |
| 플레이어 판정 경로 4곳, `Judge` 단일화의 이유, Lag Compensation | [TechDesign_HitDetection.md](../TechDesign_HitDetection.md) §3, §5, §7.5 |
| 대시 실행 흐름(InputCmd·Mover), MoveSpeed Modifier | [TechDesign_CharacterMovement.md](../TechDesign_CharacterMovement.md) §4.1, §5 |
| 스탯 합/곱 채널, AttackSpeed 소비처 | [GameDesign_Ability.md](../GameDesign_Ability.md) §3, §5, [TechDesign_Ability.md](../TechDesign_Ability.md) |
| 경직 누적(회피 시 누적 안 함) | [TechDesign_Poise.md](../TechDesign_Poise.md) |
| 과거 경위·회귀 원인 조사 | 해당 `history/PhaseNN_Log.md` |

## 4. 아키텍처 한 장 요약

`ExecuteDash`(Mover 시뮬레이션, 리시뮬레이션이 아닐 때)
→ `FLNPParryStateFragment`에 무적 만료 시각과 저스트 창 만료 시각을 기록한다(미러).
→ 서버 판정 프로세서 4곳(근접·투사체 직격·스플래시·화염)이 모두 부르는 `LNPGuardParry::Judge`가 `JustDodged` / `Dodged`를 판정한다.
→ `Dodged`면 피해를 주지 않는다. 투사체는 관통한다.
→ `JustDodged`면 `FLNPJustDodgeCommand`가 버프 GE를 적용하고 성공 큐를 실행한다.
→ 비주얼은 GameplayCue 태그 2개(`GameplayCue.LNP.JustDodge.Success`, `.Buff`) 뒤에 둔다.

상세는 `design/Architecture.md`가 소유한다.

## 5. 핵심 용어

| 용어 | 의미 |
|:---|:---|
| 무적 창 | 대시 실행 시점부터 `IFrameDuration`(초안 0.15초) 동안. 피격 반경 안의 공격을 무시한다 |
| 저스트 창 | 대시 실행 시점부터 `JustDodgeWindow`(초안 0.12초) 동안. 이 안에서 공격이 패링 반경에 들어오면 저스트 회피다 |
| `Dodged` | `Judge`의 새 결과. 무적으로 무시한 피격 |
| `JustDodged` | `Judge`의 새 결과. 저스트 회피 성공 |
| 연출 경계 | 메인 트랙과 비주얼 트랙을 잇는 GameplayCue 태그 2개. 메인 트랙은 빈 큐 에셋까지만 연결한다 |
| 순보 | 저스트 회피 성공 시 캐릭터가 잠깐 사라졌다 나타나는 핵심 연출 |

## 6. 모든 구현이 지켜야 하는 불변 조건

- 회피 판정은 `LNPGuardParry::Judge` 한 곳에서만 한다. 경로별로 회피 분기를 복제하지 않는다(J-001, HitDetection §7.5).
- 판정 프로세서는 Worker Thread에서 Mass 프래그먼트만 읽는다. ASC 태그를 판정 근거로 쓰지 않는다.
- 회피 상태는 대시 실행과 같은 시뮬레이션 경로(InputCmd → `OnMoverPreSimulationTick` → `ExecuteDash`)에서만 생긴다. 입력 콜백에서 직접 쓰지 않는다(CharacterMovement §7.6의 함정).
- 리시뮬레이션 중에는 프래그먼트를 쓰지 않는다(`TimeStep.bIsResimulating`).
- 회피는 서버 권위다. 클라이언트는 예측 연출만 생략할 수 있고 피해 결과를 정하지 않는다(J-007).
- 월드 전체 시간을 늦추지 않는다(J-008).
- 메인 트랙은 비주얼 에셋 없이도 완결된다. 비주얼은 연출 경계 뒤에서만 붙인다(J-012).

## 7. 문서별 소유 정보

- `README.md`: 작업 범위, 불변 조건, 문서 라우팅
- `Current.md`: 지금의 상태와 바로 다음 행동
- `Decisions.md`: 모든 세션이 알아야 하는 확정 결정
- `Roadmap.md`: 두 트랙의 단계 순서, Phase 게이트, 그래픽스 이관 항목
- `design/`: 현재 유효한 상세 설계와 검증 규약
- `phases/`: 해당 단계의 실행 계획과 완료 조건
- `history/`: 끝난 작업, 측정 기록, 시행착오(첫 작업 세션에서 만든다)

정보의 원본은 한 곳에만 둔다. 다른 문서에는 요약과 링크만 둔다.

## 8. 문서 갱신 규칙

### 세션 시작

1. 필수 문서 세 개를 읽는다.
2. `Current.md`에서 현재 Phase와 다음 행동을 확인한다.
3. 라우팅 표에서 필요한 설계·Phase 문서만 읽는다.
4. 코드와 에셋의 실제 상태가 문서와 맞는지 확인한다. 문서의 파일:라인 인용은 작성 시점(2026-10-26) 기준이다.

### 작업 중

- 새로 생긴 영구 규약은 해당 `design/` 문서에 반영한다.
- 작업 전체가 지켜야 하는 결정이면 `Decisions.md`에도 한 줄을 추가한다.
- 실험 수치와 측정 근거는 현재 Phase 로그에 기록한다.

### 세션 종료

1. `Current.md`를 실제 상태로 갱신한다.
2. 수행 내용과 검증 결과를 현재 `history/PhaseNN_Log.md`에 날짜별로 추가한다(파일이 없으면 만든다).
3. 완료 조건을 충족했으면 Phase 문서의 체크리스트를 갱신한다.
4. 설계가 바뀌었으면 관련 `design/`과 `Decisions.md`를 먼저 고친다.
5. 다음 세션이 바로 실행할 수 있는 구체적인 작업 한두 개를 남긴다.

### Phase 종료

1. 완료 조건과 테스트 증거를 로그에 남긴다.
2. `Roadmap.md`의 상태를 갱신한다.
3. `Current.md`를 다음 Phase 기준으로 짧게 다시 쓴다.
4. 다음 Phase 실행 문서는 착수 직전에 만들거나 구체화한다.
5. 메인 트랙 Phase 3에서는 프로젝트 공통 설계 문서를 확정한다(`GameDesign_JustDodge.md`·`TechDesign_JustDodge.md` 신설, Parry·HitDetection·CharacterMovement 교차 링크, 문서 인덱스 3곳).

## 9. 문서 상태 표기

| 상태 | 의미 |
|:---|:---|
| 활성 | 매 세션의 현재 진입점 |
| 확정 | 바꾸기 전에 명시적인 재논의가 필요한 결정 |
| 기준 설계 | 구현이 따라야 하는 현재 설계 |
| 초안 | 구현·측정 결과에 따라 바뀔 수 있음 |
| 검증 필요 | 엔진 또는 프로젝트 실험이 남아 있음 |
| 완료 | 해당 Phase의 완료 조건과 검증을 통과 |
| 보관 | 최신 설계가 아니며 역사 추적용 |
| 폐기 | 사용하지 않으며 기각 이유만 보존 |

## 10. 컨텍스트 예산 원칙

- 필수 세 문서는 합계 약 4,000~6,000토큰 이하를 목표로 한다.
- `Current.md`는 완료 이력을 쌓지 않고 200줄 이하를 유지한다.
- `Decisions.md`는 결론 중심의 표로 유지하고 상세 근거는 링크한다.
- 먼 Phase는 `Roadmap.md`에 요약만 두고 착수 전에 상세화한다.
- 히스토리는 기본으로 읽지 않고 `rg`로 관련 날짜·키워드만 찾는다.

## 11. 현재 진입점

현재 작업 위치와 다음 행동은 `Current.md`를 따른다. 전체 순서를 다시 조정해야 할 때만 `Roadmap.md`를 읽는다.
