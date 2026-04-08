# LootNPop 개발 계획 (Development Plan)

## Phase 1: Foundation (핵심 시스템 & 구형 월드)
- [x] Mover 플러그인 기반 캐릭터 이동 구현 (C++)
- [ ] **World Partition 기반 거대 구체(Sphere) 월드 구축**
    - *전략: PCG를 활용하여 모듈형 구체 섹터 타일을 배치하고 스트리밍 가능하도록 설정.*
- [ ] **구형 지형 대응 커스텀 중력 시스템 개발**
    - *전략: `UCharacterMoverComponent` 상속. 구체 중심점을 기준으로 하는 기하학적 중력(Inverted/Normal) 로직 구현.*
- [ ] **ALNPGameState 시드 동기화 구현**
    - *전략: 서버 생성 랜덤 시드 리플리케이션 및 PCG 결정론적 생성 보장.*
- [x] Enhanced Input 매핑 및 기본 액션 정의

## Phase 2: Environment (환경 및 상세화)
- [ ] PCG 기반 세부 지형(언덕, 절벽) 및 식생 생성
- [ ] SmartObject 베이스 및 상호작용 인터페이스 구현
- [ ] 오브젝트 랜덤 스폰 로직 개발

## Phase 3: Gameplay (전투 및 상호작용)
- [ ] GAS (Gameplay Ability System) 기본 인프라 구축
    - *전략: MassEntity와의 연동을 위한 Lightweight Actor 또는 Bridge Component 설계.*
- [ ] MassEntity 기반 수학적 히트 판정 프로세서 구현
- [ ] 아이템 획득/장착 및 피격 시 물리적 이탈(Launch) 시스템

## Phase 4: AI & Scale (대규모 AI)
- [ ] MassEntity 기반 NPC 로직 및 시각화 (구형 이동 지원)
- [ ] StateTree를 활용한 복합 행동(배회, 방해, 추적) 설계
- [ ] 잔여 오브젝트 수에 따른 NPC 성장/난이도 조절 시스템

## Phase 5: Loop (멀티플레이어 완성)
- [ ] Iris 기반 네트워킹 최적화 및 동기화
- [ ] 승리 조건(메달 수집) 및 세션 관리 로직
- [ ] 게임 시작/종료 시퀀스 및 결과 처리

## Phase 6: Polish (최종 폴리싱)
- [ ] MVVM 기반 HUD 및 반응형 UI 구현
- [ ] VFX(나이아가라) 및 사운드 통합
- [ ] 전반적인 게임 플레이 밸런싱
