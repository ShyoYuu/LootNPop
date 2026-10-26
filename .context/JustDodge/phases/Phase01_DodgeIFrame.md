# Phase 1 — 회피 무적

> 상태: 착수 전
> 선행: 없음
> 순서: 단위 0 → 1 → 2 → 3 → 4. 각 단위를 끝내고 검증한 뒤 사용자에게 진행 여부를 확인받는다.

## 배경

대시는 이동만 하고 피격 판정에는 아무 영향이 없다. 이 Phase에서는 대시 시작 직후 짧은 시간 동안 모든 공격의 피해를 무시하는 무적 창을 만든다. 저스트 회피(Phase 2)도 같은 상태 기록과 `Judge` 확장 위에 올라가므로, 이 Phase가 메인 트랙의 토대다.

## 목표와 범위

범위:

- `FLNPParryStateFragment`에 `DodgeIFrameExpiryTime` 추가, `ExecuteDash`에서 기록
- `Judge`에 `Dodged` 추가, 플레이어 판정 4개 경로와 클라이언트 예측 경로에서 처리
- 투사체 관통과 회피 대상 칸
- `LNP.Debug.Dodge` 디버그

범위 밖:

- 저스트 창·버프·큐 태그(Phase 2)
- 수치 확정(Phase 3)
- 비주얼(트랙 V)

## 단위 0 — 사전 조사

1. **피해 경로 전수 조사.** `Judge`를 거치지 않고 플레이어 ASC에 피해·경직·넉백을 주는 경로가 있는지 찾는다. 검색 대상은 `FLNPApplyDamageGECommand`, `ApplyGameplayEffect*`, `LNPPoise::`, `ApplyKnockback`, 화상 GE 등이다. 찾은 경로마다 회피를 적용할지 기록한다(예: 지속 화상 틱은 무적 대상인가).
2. **프래그먼트 접근 수단.** `ULNPCharacterMoverComponent`에서 플레이어 엔티티의 `FLNPParryStateFragment`를 얻는 방법을 정한다. `ULNPInputHandlerComponent::GetParryFragment()`(`LNPInputHandlerComponent.cpp:607-630`)를 빌릴지, 헬퍼를 공용 위치로 옮길지 고른다. 서버의 원격 폰(InputHandler가 없을 수 있음)에서도 동작하는지 확인한다.
3. **리스폰과 엔티티 수명.** 리스폰 때 플레이어 엔티티가 새로 생기는지, 재사용되는지 확인한다. 이에 따라 초기화 지점이 필요한지 정한다(`LNPPlayerCharacter.cpp:434` 사망 경로 주변).
4. **투사체 회피 대상 칸.** 투사체 프래그먼트(`LNPProjectileMassTypes.h:72~`)에 `FMassEntityHandle` 1칸을 둘 위치를 정한다. 패링 반사(`LNPProjectileProcessors.cpp:1055-1099`) 때 이 칸을 초기화할지 정한다. 반사된 탄은 원래 공격자를 노리므로 초기화하는 쪽이 자연스럽다.
5. **클라이언트 예측 경로 목록.** 플레이어를 대상으로 예측 임팩트를 내는 곳을 확정한다(`LNPProjectileProcessors.cpp:404-474`, `LNPWeaponTraceProcessors.cpp:299` 주변).

완료 조건: 위 다섯 항목의 결론과 근거를 `history/Phase01_Log.md`에 기록한다. 설계가 바뀌면 `design/Architecture.md`를 먼저 고친다.

## 단위 1 — 상태 미러

1. `FLNPParryStateFragment`(`HitDetection/LNPGuardParryTypes.h`)에 `double DodgeIFrameExpiryTime = -1.0`을 추가한다. 헤더 주석에 "방어 상태(가드·패링·회피)"로 의미가 넓어졌음을 적는다.
2. `ULNPCharacterMoverComponent`에 `IFrameDuration` UPROPERTY(기본 0.15)를 추가한다.
3. `ExecuteDash`의 `bIsResimulating` 게이트 뒤에서, 권위·오토노머스일 때만 `Now + IFrameDuration`을 기록한다(Architecture §2.2).
4. 단위 0-3의 결론에 따라 사망·리스폰 초기화를 넣는다.

완료 조건: 빌드 통과. 1인 PIE에서 대시할 때 서버 프래그먼트 값이 갱신되는지 확인한다(임시 로그로 확인하고 단위 3의 디버그로 대체).

## 단위 2 — `Judge`의 `Dodged`와 경로별 처리

1. `ELNPPlayerDefenseResult`에 `Dodged`를 추가하고 `Judge` 순서를 바꾼다: Parried → (피격 반경 밖이면 None) → Dodged → Guarded → Hit. `JustDodged` 자리는 Phase 2에서 채운다.
2. 근접 `JudgePlayerTarget`: `Dodged`이면 MarkHit 없이 아무것도 Push하지 않는다.
3. 투사체 직격: `Dodged`이면 `FinishHit`을 부르지 않고 회피 대상 칸에 기록한다. 이후 후보 루프에서 그 대상을 건너뛴다.
4. 스플래시·화염: `Dodged`이면 피해·넉백·경직·화상을 주지 않는다.
5. 클라이언트 예측: 로컬 플레이어의 무적 창이 열려 있으면 예측 임팩트를 생략한다.
6. `Judge` 결과를 `switch`로 처리하는 곳이 있다면 새 값이 빠진 곳이 없는지 컴파일러 경고로 확인한다.

완료 조건: D-01~D-06이 1인 PIE에서 통과한다.

## 단위 3 — 디버그

1. `LNP.Debug.Dodge` CVar를 만든다. 서버 로그 `[Dodge] IFrame ignored path=...`를 남기고, 로컬 폰의 무적 창 남은 시간을 화면에 표시한다.
2. 로그는 기본으로 꺼 둔다(적 밀집 상황에서 로그가 넘치지 않게).

완료 조건: CVar를 켜고 끄는 동작과 로그 문자열이 `design/Validation.md` §6과 일치한다.

## 단위 4 — 검증과 마무리

1. 리슨 2인 PIE로 D-07을 확인하고, 공통 회귀(`design/Validation.md` §2)를 돌린다.
2. D-01~D-08을 요인설계로 묶어 사용자 플레이 테스트를 1회 요청한다.
3. 커밋한 뒤 깨끗한 트리에서 Build.bat을 다시 돌린다.
4. 문서: `Current.md`를 Phase 2 기준으로 갱신하고, `Roadmap.md`의 상태를 갱신하고, 결과를 로그에 정리한다. 공통 설계 문서(Parry·HitDetection)는 Phase 3에서 한꺼번에 고친다.

## 완료 조건 체크리스트

- [ ] 단위 0: 피해 경로 전수, 프래그먼트 접근, 리스폰 초기화, 투사체 칸, 예측 경로 결론
- [ ] 단위 1: 프래그먼트 필드와 기록, 초기화
- [ ] 단위 2: `Dodged`와 4개 경로·예측 경로 처리, D-01~D-06
- [ ] 단위 3: `LNP.Debug.Dodge`
- [ ] 단위 4: D-07·D-08, 공통 회귀, 사용자 플레이 테스트, 클린 빌드

## 다음 Phase 인계 항목

- 투사체 경로의 패링 반경 계산 조건을 저스트 창까지 넓혀야 한다(Phase 2).
- 단위 0에서 찾은 "Judge 밖 피해 경로" 중 회피를 적용하지 않기로 한 것은 Phase 3 공통 문서에 명시한다.
