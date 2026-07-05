# Claude Code 협업 규칙

## 1. 사용 언어
- **한국어:** 모든 대화, 주석, 로그 메시지, 코드 내 문서화(Doc Comments), 그리고 **`.md` 파일(문서)**의 내용은 **한국어**로 작성
- **영어:** 소스 코드, 폴더명, 파일명, 로그 메시지는 모두 **영어(English)**로 작성

## 2. 상세 문서 인덱스

| 문서 | 내용 |
|:---|:---|
| .context/DevelopmentPlan.md | Phase별 구현 현황 및 마일스톤 |
| .context/DiscardedApproaches.md | 시도 후 제외된 기술적 접근과 사유 |
| .context/Idea_Backlog.md | 정규 스펙으로 채택되기 전 아이디어 후보 모음 |
| .context/TechDesign_WorldGeneration.md | Octant 8분할 전략, PCG 지형 생성, 결정론적 런타임 스폰 흐름 |
| .context/Guide_OctantLevelInstance.md | Octant Level Instance 에셋 신규 제작 절차 |
| .context/TechDesign_InitSequence.md | 서버/클라 4단계 초기화, 투-게이트 레이스 컨디션 해결, 폰 스폰 게이팅 |
| .context/TechDesign_SurfaceCache.md | 등장방형 그리드 사전 베이킹, Mass 워커 스레드 O(1) 안전 조회, NavMesh 대체 이유 |
| .context/TechDesign_CharacterMovement.md | 구형 중력 3모드, 곡률 보정, 질주·대시 시스템 |
| .context/TechDesign_CombatAnimation.md | Motion Matching 로코모션, 무기별 Linked Anim Layer 교체, Aim Offset·상하체 블랜딩 구현 완료 — Guard ABP 분기 등 에디터 작업 잔여 |
| .context/GameDesign_Ability.md | 무기·스킬·버프 아이템 구조, GAS 슬롯 관리, 구현 현황 |
| .context/TechDesign_Ability.md | ASC/AttributeSet 아키텍처, 발사체 Mass 프로세서 4종, 어빌리티 클래스 계층 상세 |
| .context/GameDesign_EnemyNPC.md | 슬롯 기반 타겟팅, 행동 상태 (Idle/Alert/Chase/Attack), LOD 전환 |
| .context/TechDesign_EnemyNPC.md | Fragment/Tag 구조, Mass 프로세서 9종, Actor 연동 (High LOD) |
| .context/TechDesign_EnemyNPC_StateTree.md | StateTree 상태 계층 (Combat/Alert/Idle), Evaluator 및 Task C++ 구성 |
| .context/GameDesign_LootPod.md | 루팅 흐름, 취소 조건, 보상 유형 |
| .context/TechDesign_LootPod.md | MassEntity 구성, SmartObject 연동, 게이지·인터럽션·보상 미구현 상세 |
| .context/TechDesign_HUD.md | MVVM ViewModel 구조, ASC 델리게이트 기반 갱신 흐름, Blueprint 바인딩 설정 |
| .context/TechDesign_HitDetection.md | 근접 Swept Volume·원거리 Line Segment 판정 (근접·원거리 구현 완료, 공간 쿼리 최적화 미구현) |
| .context/TechDesign_Networking.md | Iris 기반 멀티플레이 설계 — 설계 원칙, MassEntity 네트워크 분류, 시스템별 복제 방안, 7단계 구현 계획 |
| .context/GameDesign_ParrySystem.md | 패링 성공 조건, 투사체 타입별 반사, 플레이어 경험 의도 |
| .context/TechDesign_ParrySystem.md | FLNPParryStateFragment, HitDetection 연계 판정 흐름, Mass-GAS 브릿지 방안 |
| .context/EngineAnalysis_MassEntity.md | Unreal Engine의 MassEntity 시스템 분석 |
| .context/EngineAnalysis_MassStateTree.md | Unreal Engine의 StateTree - MassBehavior 시스템 분석 |
| .context/EngineAnalysis_MoverArchitecture.md | Unreal Engine의 Mover 2.0 시스템 분석 |