# 저스트 회피 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: 메인 트랙 Phase 2 저스트 회피 판정 + 버프 + 연출 경계 — 착수 전
> 마지막 갱신: 2026-11-03

## 현재 결정

사용자가 정한 것(2026-10-26):

- 저스트 회피는 근접 공격과 투사체 직격에만 발동한다(J-002).
- 무적 중인 플레이어를 지나는 투사체는 관통한다(J-003).
- 반경은 기존 패링 반경을 쓴다(J-004).
- 버프는 AttackSpeed·MoveSpeed 곱연산 +20%, 2.5초, 시간만 갱신한다(J-005).
- 비주얼은 마지막 독립 트랙으로 둔다. 화면 연출·림라이트·Niagara 정밀도 조사는 그래픽스 작업으로 넘긴다(J-012).

Phase 1(회피 무적)은 2026-11-03에 끝났다. 결과는 `history/Phase01_Log.md`에 있다. Phase 2가 올라설 토대는 다음과 같다.

- `FLNPParryStateFragment::DodgeIFrameExpiryTime`이 있다. `ExecuteDash`의 리시뮬레이션 게이트 뒤에서 권위·오토노머스일 때만 기록한다. 시뮬레이티드 프록시도 `ExecuteDash`를 첫 시뮬레이션으로 지나가므로 역할 게이트가 꼭 필요하다.
- 프래그먼트 접근은 `LNPGuardParry::FindFragment(const AActor*)`로 한다.
- `Judge` 순서는 Parried → (피격 반경 밖이면 None) → Dodged → Guarded → Hit다. `JustDodged`는 Parried 바로 다음, 반경 검사 앞에 들어간다(`design/Architecture.md` §3).
- 경로별 `Dodged` 처리, 투사체 `DodgedEntity`(반사 시 초기화), 클라 Ghost 예측 생략이 들어가 있다.
- 디버그 CVar 두 개(둘 다 기본 꺼짐):
  - `LNP.Debug.Dodge`: 로그
  - `LNP.Debug.ShowDodge`: 화면 문구. 연한 보라는 창 열림, 짙은 보라는 무적 성공이다. 사용자가 남은 작업 내내 쓰겠다고 했다. Phase 2의 성공 문구도 여기에 붙인다.

## 바로 다음 행동

- `Roadmap.md` §4 Phase 2를 바탕으로 `phases/Phase02_JustDodge.md` 실행서를 쓰고 사용자 확인을 받는다.
- 실행서에 넣을 Phase 1 인계 항목:
  - 투사체 직격 경로의 패링 반경 계산 조건을 `PS.bIsParrying || 저스트 창 열림`으로 넓힌다.
  - 클라 Ghost가 예측 임팩트를 생략할 때 남기는 로그를 넣는다. D-07 게스트 화면이 산탄 때문에 육안으로 판정되지 않아 부분 통과로 남았다.
- Phase 3에서 사용자 판단이 필요한 것:
  - 화상 지속 틱은 회피하지 않는다. 그래서 화염을 대시로 피해도 계속 깎인다(실측상 사망의 주원인).
  - 근접 예측 HitStop은 고치지 않았다.
  - D-02(일찍 대시해 칼날 안에 남음)는 재현이 어려워 코드 근거로만 둔다.

## 인계 근거

- 검증 절차와 시나리오: `design/Validation.md`. PvP 경로는 `LNP.Team.Set 1 2`를 빼먹지 않는다(§6).
- 빌드·PIE 검증 방법: 프로젝트 공통 워크플로(Build.bat, Live Coding, Unreal MCP)를 따른다. 구조체 레이아웃·새 `.cpp`·새 CVar가 있으면 에디터를 닫고 풀 빌드한다. 커밋한 뒤에는 깨끗한 트리에서 Build.bat을 다시 돌려 확인한다.
