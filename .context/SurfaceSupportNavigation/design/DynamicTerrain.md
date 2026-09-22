# 동적 지형과 Runtime Overlay 설계

> 상태: 초안
> 읽기 조건: 움직이는 패널, 상태형 길, 파괴 또는 지역 revision을 구현할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## 동적 지형과 Runtime Overlay

### 동적 지형 Actor

움직이거나 파괴되는 지형은 별도 Actor 또는 명확한 전용 Component로 둔다.

이유:

- lifetime·복제·권위 주체 명확화
- static SurfaceData에서 제외 가능
- collision profile 분리
- stable dynamic ID 부여
- nav/support invalidation 이벤트 발행

### 움직이는 패널

움직이는 패널은 경로 탐색 대상이 아니지만 NPC가 우연히 착지할 수 있다.

```text
Airborne
   ↓ exact capsule sweep
DynamicSupport 착지
   ↓
플랫폼 local contact 저장
   ↓
매 프레임 platform transform delta 적용
   ↓
접촉 상실·경사 초과·가장자리 이탈
   ↓
Airborne
```

동적 지형 서브시스템은 worker가 읽을 immutable per-frame snapshot을 게시한다.

```cpp
struct FDynamicSupportSnapshot
{
    FDynamicSupportId Id;
    FTransform PreviousTransform;
    FTransform CurrentTransform;
    FVector LinearVelocity;
    FVector AngularVelocity;
    bool bWalkable;
};
```

NPC는 패널 위에서 상대 이동을 할 수 있지만 AI는 패널을 기다리거나 목적 경로로 선택하지 않는다.

### 쓰러지는 기둥·기믹 다리

```text
Standing
  - collision 활성
  - bridge traversal 비활성

Transitioning
  - kinematic 이동
  - bridge traversal 비활성
  - 위의 NPC는 DynamicSupport로 운반

Bridge
  - 안정 transform 검증
  - local Support Patch 활성
  - 양쪽 Nav 영역 Walk Link 활성
  - 영향 cluster revision 증가
```

이 구조는 문·내려오는 다리·회전 통로에도 재사용한다.

### 파괴

지원 범위:

- blocker 제거로 기존 통로 열기
- 바닥 제거로 기존 길 닫기
- 고정 상태 기믹으로 명시적 길 열기
- 임시 장애물로 cell 차단·해제

제외 범위:

- 물리 잔해가 우연히 새 보행면 생성
- 매 프레임 임의 형상 Nav 재생성
- 파편 더미 위 walkability 분석

파괴 AABB와 겹치는 Support/Nav cell을 runtime overlay에서 invalidate한다. 정적 asset은 수정하지 않는다.

### 지역 revision

전역 revision 하나는 사용하지 않는다.

- Support Atlas 또는 Nav Tile/Cluster별 revision
- 경로가 실제 통과하는 지역 revision만 기록
- 무관한 섬의 변경으로 모든 NPC가 재탐색하지 않음
- 계층형 단계에서는 dirty cluster만 portal 비용 재계산

---

