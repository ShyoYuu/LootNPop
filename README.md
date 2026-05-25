# Loot & Pop

언리얼 엔진 5.7의 최신 기능을 활용한 개인 기술 실증 및 포트폴리오 프로젝트입니다.
본 프로젝트의 개요 및 설계 문서는 [context](context/) 폴더를 참고해 주세요.

---

> [!IMPORTANT]  
> **Notice for Visitors** > 본 프로젝트는 개인 포트폴리오 용도로 제작되었으며, 다음 사항을 제한하므로 양해 부탁드립니다.
> - **No Pull Requests**: 어떠한 형태의 PR도 받지 않습니다.
> - **No Issues / Comments**: 코드 리뷰 및 수정 제안을 포함한 일체의 피드백을 받지 않습니다.

---

## 🎮 Game Concept

거대한 구체의 **내벽 표면(Dyson Sphere)**이 플레이 공간인 멀티플레이어 파티 게임입니다.
전체 월드에 랜덤 보상(각종 무기, 버프)을 루팅할 수 있는 특수 오브젝트들이 존재합니다.
플레이어들은 해당 특수 오브젝트들을 지키고 있는 Enemy NPC의 방해를 뚫고 보상을 수집해야 합니다.

---

## 🛠 Development Philosophy
          
엔진의 최신 피처를 최대한 활용하고 데이터 지향적 설계(DOD)를 따르는데 초점을 맞췄습니다.

### Key Unreal Engine Features
- **GAS (Gameplay Ability System)**: Ability 및 상태 관리
- **Enhanced Input**: 플레이어 Input Action 및 Runtime Input Context 관리
- **Mover 2.0**: 플레이어 캐릭터, NPC 캐릭터의 Movement 및 Movement 동기화, 커스텀 중력
- **Motion Matching**: 플레이어 캐릭터, NPC 캐릭터의 Locomotion
- **MassEntity**: NPC 캐릭터와 상호작용 오브젝트는 MassEntity(LowLOD) - Actor(HighLOD) 하이브리드, HitDetection용 충돌체는 Pure MassEntity
- **StateTree**: NPC 캐릭터 행동 제어
- **Gameplay Camera**: 플레이어 카메라를 Gameplay Camera로 적용
- **World Partition**: 대규모 맵 스트리밍 및 로딩 최적화
- **PCG (Procedural Content Generation)**: 배경 프랍 배치
- **Geometry Script**: 지형용 Static Mesh 생성

---

## 🤖 AI Collaboration

본 프로젝트의 코드 작업에는 다음 AI 도구들이 참여하고 있습니다.

- **Claude Code**
- **Gemini CLI**

---

© 2026. All rights reserved.