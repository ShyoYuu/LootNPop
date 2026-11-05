# 저스트 회피 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: 메인 트랙 완료(Phase 1~4, 2026-11-05). 다음은 비주얼 트랙 1차(V1a) — 착수 전
> 마지막 갱신: 2026-11-05

## 현재 결정

사용자가 정한 것:

- 저스트 회피는 근접 공격과 투사체 직격에만 발동한다(J-002).
- 무적 중인 플레이어를 지나는 투사체는 관통한다(J-003). 단, 피격 유예로 소급 회피된 탄은 관통하지 않는다(J-019).
- 반경은 기존 패링 반경을 쓴다(J-004).
- 버프는 AttackSpeed·MoveSpeed 곱연산 +20%, 2.5초, 시간만 갱신한다(J-005). 사용자가 천천히 상향을 검토한다.
- 연출은 서버 확정 뒤에만 낸다(J-007).
- 화상 지속 틱은 무적으로 막지 않는다(J-013). PvP 근접 예측 HitStop 어긋남은 허용한다(J-014).
- 원격 게스트 피격 효과는 피격 유예 버퍼로 보류하고, 늦게 도착한 대시가 소급 회피한다(J-015~J-019, J-010 대체).
- 비주얼은 메인 트랙 뒤의 독립 트랙으로 둔다(J-012). 그래픽스와 무관한 1차(V1a·V2·V3a·V4a)는 그래픽스 Phase 0보다 먼저 하고, 2차(V3b·V5·V6·V4b)는 그래픽스 게이트 뒤에 한다(J-020).

## 메인 트랙이 남긴 토대

- 판정: `ExecuteDash`가 무적·저스트 창을 `FLNPParryStateFragment`에 기록한다(원격 게스트는 `E − U`부터). `Judge` 순서는 Parried → JustDodged → (반경 밖 None) → Dodged → Guarded → Hit이다. 성공은 `FLNPJustDodgeCommand`가 버프 GE와 `.Success` 큐로 처리한다.
- 수치: Mover 컴포넌트의 `IFrameDuration` 0.15·`JustDodgeWindow` 0.12. 프로젝트 설정 `Combat|Just Dodge`는 버프 2.5초·0.2·0.2와 피격 유예 `HitGraceInputAllowance` 0.045·`HitGraceJitterMargin` 0.04·`HitGraceMax` 0.25다.
- 연출 경계: 큐 태그 `GameplayCue.LNP.JustDodge.Success`·`.Buff`. 성공 큐 에셋의 부모는 `ULNPGameplayCueNotify_JustDodgeSuccess`다. V1a가 이 클래스에 순보를 붙인다. 소유 화면의 누름 → 큐 지연은 RTT +100에서 p50 약 150ms, PktLag 50(두 창)에서 약 225ms다. V-05에서 다시 본다.
- 프로젝트 공통 문서: [GameDesign_JustDodge.md](../GameDesign_JustDodge.md), [TechDesign_JustDodge.md](../TechDesign_JustDodge.md).

## 바로 다음 행동

- V1a(순보 핵심) 실행서 `phases/PhaseV1a_*.md`를 쓰고 사용자 확인을 받는다. 설계 원본은 `design/VisualDesign.md` §2.1이다.
  - 1차의 제약(`Roadmap.md` §4 비주얼 트랙): Niagara는 로컬 스페이스 버스트만, 캐릭터 오버레이 머티리얼 금지, 화면 연출 임시 구현 금지.
  - 실행서에서 정할 것: 보라색 구체 값, 숨김·복원을 맡을 캐릭터 함수 이름, 부재·나타남 길이.
- 1차 순서는 V1a → V2 → V3a → V4a다. V3a 착수 전에 ADS FOV와 FOV 펀치의 우선순위를 사용자와 정한다.
- 2차 단위(V3b·V5·V6·V4b)는 [Graphics/Current.md](../Graphics/Current.md)에서 게이트(단위 N·3-2·3-3)가 끝났는지 확인한 뒤 시작한다.
- 수치 상향(버프)은 사용자가 플레이하면서 정한다. 프로젝트 설정 값이라 코드 변경이 없다.

## 인계 근거

- 검증 절차와 로그 문자열: `design/Validation.md`. PvP 경로는 `LNP.Team.Set 1 2`를 빼먹지 않는다(§6). PIE를 새로 시작하면 다시 넣어야 한다.
- 지연 에뮬레이션: `net.PktLag=N`은 입력한 창의 송신에만 걸린다. 호스트·게스트 두 창 모두에 넣어야 RTT가 약 +2N이 된다.
- 단일 프로세스 PIE 로그에서는 폰 이름이 월드마다 다르거나 겹친다. 집계 스크립트는 시각과 역할로 짝짓는다.
- 지연 중 게스트 리스폰 때 Iris `Rejected RPC (0:0) ... missing object` 에러가 2~4줄 난다. 서버가 파괴된 옛 폰으로 날아오던 게스트 이동 입력 RPC를 버리는 것이다. 영향이 없고 피격 유예와도 무관하다(`history/Phase04_Log.md` 2026-11-05).
- 빌드·PIE 검증 방법: 프로젝트 공통 워크플로를 따른다. 커밋한 뒤에는 `-DisableAdaptiveUnity`로 전체 unity 구성을 한 번 확인한다(adaptive unity가 수정 중인 파일을 unity에서 빼기 때문이다).
