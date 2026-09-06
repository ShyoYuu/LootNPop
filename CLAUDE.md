# Claude Code 협업 규칙

## 1. 사용 언어
- **한국어:** 모든 대화, 주석, 코드 내 문서화(Doc Comments), 그리고 **`.md` 파일(문서)**의 내용은 **한국어**로 작성
- **영어:** 소스 코드, 폴더명, 파일명, 그리고 **런타임 출력 문자열**(`UE_LOG` 메시지, 콘솔 커맨드/CVar 도움말)은 모두 **영어(English)**로 작성

## 2. 상세 문서 인덱스

| 문서 | 내용 |
|:---|:---|
| .context/DevelopmentPlan.md | Phase별 구현 현황 및 마일스톤 |
| .context/DiscardedApproaches.md | 시도 후 제외된 기술적 접근과 사유 |
| .context/Idea_Backlog.md | 정규 스펙으로 채택되기 전 아이디어 후보 모음 |
| .context/TechDesign_WorldGeneration.md | Octant 8분할 전략, PCG 지형 생성, 결정론적 런타임 스폰 흐름 |
| .context/Guide_OctantLevelInstance.md | Octant Level Instance 에셋 신규 제작 절차 |
| .context/Guide_CustomSlateWidget.md | 커스텀 Slate 위젯 제작 절차 — 베이스 선택, TSlateAttribute, 스타일 분리, UMG 래퍼 |
| .context/TechDesign_InitSequence.md | 서버/클라 4단계 초기화, 투-게이트 레이스 컨디션 해결, 폰 스폰 게이팅 |
| .context/TechDesign_SurfaceCache.md | 등장방형 그리드 사전 베이킹, Mass 워커 스레드 O(1) 안전 조회, NavMesh 대체 이유 |
| .context/TechDesign_CharacterMovement.md | 구형 중력 3모드, 곡률 보정, 컨트롤 회전 파이프라인, 카메라 리그 노드 순서 제약, 질주·가드·대시·ADS 시스템 |
| .context/TechDesign_CombatAnimation.md | Motion Matching 로코모션, 무기별 Linked Anim Layer 교체, Aim Offset·왼손 Two Bone IK·Guard 자세 분기, 몽타주 ANS 구간 제어와 경직 차단 소유권 구분, 근접 공격 타겟 보정(Motion Warping) |
| .context/GameDesign_Ability.md | 무기·스킬·버프 아이템 구조, GAS 슬롯 관리, 합/곱 이원 스텟 체계, 구현 현황 |
| .context/TechDesign_Ability.md | ASC/AttributeSet 아키텍처, 합/곱 2채널 스탯 파이프라인, 발사체 Mass 프로세서 4종, 어빌리티 클래스 계층 |
| .context/GameDesign_Poise.md | 경직 시스템 — 누적/자연회복 원칙, 그로기·다운 2단계, 딜 구간 비대칭, 가드 브레이크·패링 연계 |
| .context/TechDesign_Poise.md | FLNPPoiseFragment·ULNPPoiseProcessor, 상태 기반 그로기, 폰별 임계값, 유지시간 비례 보너스, 비복제 근거 |
| .context/GameDesign_EnemyNPC.md | 슬롯 기반 타겟팅, 행동 상태 (Idle/Alert/Chase/Attack), LOD 전환 |
| .context/TechDesign_EnemyNPC.md | Fragment/Tag 구조, Mass 프로세서 12종, Actor 연동 (High LOD), 넷 모드별 표현 소유권(게스트는 복제 Actor만) |
| .context/TechDesign_EnemyNPC_StateTree.md | StateTree 상태 계층 (Combat/Alert/Idle), Evaluator 및 Task C++ 구성 |
| .context/TechDesign_EnemyNPC_LowLOD.md | CombatMode 옵션(Actor 승격/순수 엔티티), 가상 칼날 근접 판정, 행동 상태 1바이트 복제, ISM↔ISKM 인스턴싱 애니메이션 |
| .context/GameDesign_LootPod.md | 루팅 흐름, 존 사수(넉백) 취소 조건, 협동 루팅 속도, 보상 유형 |
| .context/TechDesign_LootPod.md | MassEntity 구성, Pod 레지스트리 상호작용 탐색, 게이지·보상 드랍·Low LOD 빛기둥 |
| .context/GameDesign_LootDice.md | 보상 아이템(LootDice) 주사위 굴림 컨셉·아이콘 식별·획득·인벤토리 드랍·소멸 기획 |
| .context/TechDesign_LootDice.md | 서버 권위 물리 Actor, Iris FRepMovement 동기화, 구면 중력 AddForce, 획득·드랍 RPC |
| .context/TechDesign_HUD.md | MVVM ViewModel 구조, ASC 델리게이트 기반 갱신 흐름, 대시 쿨다운 파이 위젯, 적 HP 바 오버레이 설계 |
| .context/GameDesign_InGameMenu.md | 인게임 메뉴 — 탭 구조, 게임패드 조작, 캐릭터 스탯·인벤토리·환경설정 탭 기획 |
| .context/TechDesign_InGameMenu.md | CommonUI 위젯 계층, Back 전파 규칙, 커스텀 힌트 바·입력 글리프, 스탯 합/곱 분해, 로컬라이제이션, PIE 2인 게임패드 라우팅 |
| .context/TechDesign_Inventory.md | 아이템 인스턴스 모델(UObject+FastArray+등록 서브오브젝트), GameplayTagStack 스탯, 장착/보관 분리, 버프 인스턴스 흐름 |
| .context/TechDesign_HitDetection.md | 근접 Swept Volume·원거리 Line Segment 판정, 판정 캡슐 중심 규약 단일 헬퍼, 조준 원본 단일화(서버가 클라 조준점을 읽음), 공간 쿼리 최적화 미구현 |
| .context/TechDesign_Networking.md | 멀티플레이 네트워킹 — Iris·MassReplication 하이브리드, Lag Compensation, 클라이언트 예측·Dead Reckoning, 대역폭 예산 규약(상한=안전판·엔티티당 비용·조용한 소실), 엔진 소스 분석 이슈 7건 |
| .context/Guide_NetBandwidth.md | 네트워크 대역폭 — 비용 3축(버블·승격 Actor·절편), 페이로드 양자화 규약과 int16 월드 반지름 캡, 절제(ablation)와 사유별 계수 측정법·분모 규약, 반복된 실패 패턴 |
| .context/GameDesign_ParrySystem.md | 패링 성공 조건, 투사체 타입별 반사, 플레이어 경험 의도 |
| .context/TechDesign_ParrySystem.md | FLNPParryStateFragment, HitDetection 연계 판정 흐름, Mass-GAS 브릿지 방안 |
| .context/EngineAnalysis_SlateUMG.md | Unreal Engine의 Slate·UMG 분석 — 레이아웃 2패스, 슬롯 개념, 위젯 수명주기, 애니메이션·드래그앤드롭, Invalidation·Retainer 성능 |
| .context/EngineAnalysis_MVVM.md | Unreal Engine의 ModelViewViewModel 분석 — FieldNotify 통지, 바인딩·실행 모드, ViewModel 주입 6종, 리스트 한계 |
| .context/EngineAnalysis_CommonUI.md | Unreal Engine의 CommonUI 분석 — 화면 활성화·Back 전파, 입력 액션/모드, 스타일 에셋, 탭·리스트 함정 |
| .context/EngineAnalysis_MassEntity.md | Unreal Engine의 MassEntity 분석 — Archetype/Fragment 구조, 프로세서 실행 스케줄링, 지연 커맨드 버퍼, 스레드 안전성 |
| .context/EngineAnalysis_MassReplication.md | Unreal Engine의 Mass Replication 분석 — 버블 구조, 서버/클라 복제 흐름, 다중 버블 파괴 경로·공유 프래그먼트 CRC 함정 |
| .context/EngineAnalysis_MassStateTree.md | Unreal Engine의 StateTree - MassBehavior 분석 — 노드 베이스 API, Mass 실행·신호 시스템, 하이브리드 제어 전략 |
| .context/EngineAnalysis_MoverArchitecture.md | Unreal Engine의 Mover 2.0 분석 — 이동 모드·모디파이어·인스턴트 효과·레이어드 무브 결정 매트릭스, 시뮬레이션 틱 순서 |