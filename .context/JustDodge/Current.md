# 저스트 회피 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: 메인 트랙 Phase 3 수치 1차 튜닝·지연 측정·문서 마감 — 착수 전
> 마지막 갱신: 2026-11-03

## 현재 결정

사용자가 정한 것(2026-10-26):

- 저스트 회피는 근접 공격과 투사체 직격에만 발동한다(J-002).
- 무적 중인 플레이어를 지나는 투사체는 관통한다(J-003).
- 반경은 기존 패링 반경을 쓴다(J-004).
- 버프는 AttackSpeed·MoveSpeed 곱연산 +20%, 2.5초, 시간만 갱신한다(J-005).
- 비주얼은 마지막 독립 트랙으로 둔다. 화면 연출·림라이트·Niagara 정밀도 조사는 그래픽스 작업으로 넘긴다(J-012).

Phase 1(회피 무적)과 Phase 2(저스트 회피)는 2026-11-03에 끝났다. 결과는 `history/Phase01_Log.md`·`history/Phase02_Log.md`에 있다. Phase 3이 올라설 토대는 다음과 같다.

- `ExecuteDash`가 무적 창과 저스트 창(무적 창 이하로 묶음)을 `FLNPParryStateFragment`에 기록한다. 소비 플래그 `bJustDodgeConsumed`는 서버 성공 커맨드 `FLNPJustDodgeCommand`만 세운다.
- `Judge` 순서: Parried → JustDodged → (피격 반경 밖이면 None) → Dodged → Guarded → Hit.
- 버프 `ULNPGameplayEffect_JustDodgeBuff`. ⚠️ `MultiplyAdditive` SetByCaller에는 `1 + 배율`을 넣는다(`design/Architecture.md` §5.2).
- 연출 경계: 큐 `GameplayCue.LNP.JustDodge.Success`·`.Buff`와 빈 GCN 2개(`Content/GAS/GameplayCues/`). 비주얼 트랙은 여기서 시작한다.
- 튜닝 손잡이: Mover 컴포넌트의 `IFrameDuration`(0.15)·`JustDodgeWindow`(0.12), 프로젝트 설정 `Combat|Just Dodge`(2.5초, 0.2, 0.2).
- 디버그: `LNP.Debug.Dodge`(로그), `LNP.Debug.ShowDodge`(화면 문구 — 연한 보라 창, 짙은 보라 무적 차단, 자홍 `JUST DODGE`). 문자열은 `design/Architecture.md` §7.

## 바로 다음 행동

- `Roadmap.md` §4 Phase 3을 바탕으로 `phases/Phase03_*.md` 실행서를 쓰고 사용자 확인을 받는다.
- 실행서에 넣을 Phase 2 인계 항목:
  - J-03(화염·폭발 안에서 대시)은 실측하지 않았다. 플레이 테스트에 끼워 넣는다.
  - 버프 경계 보정은 `PktLag` 50/100ms에서 다시 본다(지연 없는 2인에서는 체감 없음).
- Phase 3에서 사용자 판단이 필요한 것(Phase 1에서 이어짐):
  - 화상 지속 틱은 회피하지 않는다. 그래서 화염을 대시로 피해도 계속 깎인다(실측상 사망의 주원인).
  - 근접 예측 HitStop은 고치지 않았다.
  - D-02(일찍 대시해 칼날 안에 남음)는 재현이 어려워 코드 근거로만 둔다.

## 인계 근거

- 검증 절차와 시나리오: `design/Validation.md`. PvP 경로는 `LNP.Team.Set 1 2`를 빼먹지 않는다(§6).
- 빌드·PIE 검증 방법: 프로젝트 공통 워크플로(Build.bat, Live Coding, Unreal MCP)를 따른다. 구조체 레이아웃·새 `.cpp`·새 CVar가 있으면 에디터를 닫고 풀 빌드한다. 커밋한 뒤에는 깨끗한 트리에서 Build.bat을 다시 돌려 확인한다.
