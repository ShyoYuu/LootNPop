# 지형 의미·충돌 authoring 계약

> 상태: 기준 설계
> 적용 시작: Phase 0
> 관련 회귀 공간: `RegressionMap.md`

## 1. 결론

지형 의미는 메시 종류나 `WorldStatic` 여부에서 추론하지 않고, 충돌을 제공하는 `UPrimitiveComponent`의 Component Tag로 명시한다. Actor Tag는 테스트 사례와 편집기 정리에만 사용하며 베이커의 의미 원본으로 사용하지 않는다.

의미는 역할과 수명주기 두 축을 조합한다.

### 역할 태그

| Component Tag | 의미 |
|:---|:---|
| `LNP.Surface.Support` | 중력 방향을 기준으로 캐릭터를 지지할 수 있는 후보 표면 |
| `LNP.Surface.Blocker` | 정밀 충돌과 Nav 점유에 반영되는 장애물 |

### 수명주기 태그

| Component Tag | 의미 |
|:---|:---|
| `LNP.Surface.Static` | 옥탄트 로드 후 transform과 형상이 바뀌지 않는 정적 source |
| `LNP.Surface.Dynamic` | transform이 변하는 Runtime Overlay source |
| `LNP.Surface.StatefulTraversal` | 정해진 안정 상태에서만 Walk Link를 열 수 있는 기믹 source |
| `LNP.Surface.Destructible` | 파괴 전후 유효성이 Runtime Overlay revision으로 바뀌는 source |
| `LNP.Surface.Decoration` | Support·Nav·정밀 월드 충돌에서 모두 제외되는 장식 geometry |

`Decoration`은 다른 의미 태그와 함께 사용할 수 없다. 장식이 실제 탄환이나 Pawn을 막아야 한다면 장식이 아니라 `Blocker`로 authoring한다.

## 2. 표준 조합

| 범주 | 필수 Component Tag | 정적 Support bake | 정적 Nav bake | 정밀 충돌 |
|:---|:---|:---:|:---:|:---:|
| 일반 정적 지형 | `Support`, `Blocker`, `Static` | 포함 | 보행면·경계 포함 | 포함 |
| 정적 Support proxy | `Support`, `Static` | 포함 | 보행면 후보 | 제외 가능 |
| 정적 벽·천장·프랍 | `Blocker`, `Static` | 제외 | 점유·clearance 포함 | 포함 |
| 움직이는 패널 | `Support`, `Blocker`, `Dynamic` | 제외 | 계획 경로에서 제외 | 포함 |
| 상태형 기둥·다리 | `Blocker`, `StatefulTraversal` | 제외 | 정적 link 제외 | 포함 |
| 파괴 가능한 바닥 | `Support`, `Blocker`, `Destructible` | immutable payload에서 제외 | Runtime Overlay로 반영 | 파괴 전 포함 |
| 파괴 가능한 blocker | `Blocker`, `Destructible` | 제외 | Runtime Overlay로 반영 | 파괴 전 포함 |
| 장식 | `Decoration` | 제외 | 제외 | 제외 |

상태형 지형이 안정 상태에 도달해도 원본 태그를 `Static`으로 바꾸지 않는다. 안정 상태, 활성 link, 지역 revision은 런타임 상태이며 immutable 옥탄트 데이터와 분리한다.

### 배치 위치

- `Static`·`Decoration` source는 옥탄트 LVI에 직접 둔다.
- `Dynamic`·`StatefulTraversal`·`Destructible` source는 옥탄트 LVI에 두지 않는다. 이 태그는 서버가 스폰하는 동적 요소 Actor 클래스의 컴포넌트에만 붙는다(D-026). LVI에는 그 위치를 가리키는 Placement Marker만 둔다.
- 회귀 맵(`RegressionMap.md`)의 동적 fixture는 Phase 3에서 마커 방식으로 전환하기 전까지 예외로 둔다.

## 2-1. Placement Marker 계약

- 마커는 LVI 안의 비복제 Actor이며 충돌은 `NoCollision`, 시각화 컴포넌트는 editor-only다. Terrain Contract 태그를 갖지 않는다.
- 마커는 `UPROPERTY FGuid MarkerId`를 저장한다. 배치할 때 발급하고, 복제·붙여넣기로 생긴 마커는 새 ID를 받는다. `AActor::ActorGuid`는 editor-only 데이터라 cooked 런타임에 없으므로 사용하지 않는다.
- 마커는 스폰할 Actor 클래스와 그 요소의 로컬 authoring 데이터(경로 spline, 안정 상태 transform, 파라미터)를 가진다.
- 런타임 식별자는 `(slot, MarkerId)`다. 같은 LVI가 여러 slot에 들어가도 구분된다.
- 스폰할 Actor 클래스가 Conditional Patch를 여는 요소라면, 베이커가 읽을 static patch source mesh를 제공해야 한다. 런타임 표현이 Geometry Collection이어도 patch source는 Static Mesh다.

## 3. Component 단위 규칙

- 의미 판정의 최소 단위는 `UPrimitiveComponent`다. 한 Actor 안에 역할이 다른 컴포넌트가 있어도 각 컴포넌트를 독립 판정한다.
- 하나의 HISM 컴포넌트에는 같은 의미 조합만 넣는다. 나무 blocker와 비충돌 장식은 HISM 컴포넌트를 분리한다.
- `Support`는 실제로 agent capsule을 지지할 의도가 있는 면에만 부여한다. 측벽·밑면·동굴 천장은 메시가 같은 Actor에 있어도 자동 Support로 간주하지 않는다.
- `Blocker`는 현재 `ECC_WorldStatic`과 동의어가 아니다. 기존 시스템 전환이 끝날 때까지 둘은 병존한다.
- 베이커는 태그 없는 geometry를 암묵적으로 지형으로 승격하지 않는다. 태그 누락은 validation 오류로 보고 명시적으로 수정한다.
- 특히 collision이 켜져 있는데 태그가 없는 primitive component는 조용히 건너뛰지 않고 보고한다. 태그 없는 나무 HISM이 Nav 점유에서 말없이 빠지는 것을 막기 위해서다.
- 역할 태그를 받을 수 있는 컴포넌트는 `UStaticMeshComponent`와 그 파생(ISM·HISM)이다. 다른 primitive 종류에 역할 태그가 붙으면 지원 전까지 오류다.
- Level Instance slot transform은 source component의 로컬 transform 뒤에 적용한다. semantic tag는 회전과 무관하게 유지하고 normal·방향·bounds만 slot transform으로 변환한다.

## 4. Collision channel과 profile

Phase 0에서 다음 trace channel을 예약했다. 기존 소비자는 아직 전환하지 않는다.

| 이름 | 엔진 슬롯 | 기본 응답 | 용도 |
|:---|:---|:---:|:---|
| `LNPSurfaceSupport` | `ECC_GameTraceChannel1` | Ignore | 에디터 Support bake와 support source 검증 |
| `LNPWorldExact` | `ECC_GameTraceChannel2` | Ignore | 벽·천장·프랍·동적 지형을 포함하는 정밀 world query |

| Profile | Object Type | `LNPSurfaceSupport` | `LNPWorldExact` | 대표 의미 |
|:---|:---|:---:|:---:|:---|
| `LNPStaticTerrain` | WorldStatic | Block | Block | `Support+Blocker+Static` |
| `LNPStaticSupport` | WorldStatic | Block | Ignore | `Support+Static` proxy |
| `LNPStaticBlocker` | WorldStatic | Ignore | Block | `Blocker+Static` |
| `LNPDynamicTerrain` | WorldDynamic | Block | Block | `Support+Blocker+Dynamic` |
| `LNPStatefulTraversal` | WorldDynamic | Ignore | Block | `Blocker+StatefulTraversal` |
| `LNPDestructibleTerrain` | Destructible | Block | Block | Destructible support 또는 blocker |
| `LNPDecoration` | WorldStatic | Ignore | Ignore | `Decoration` |

profile은 물리 응답을, Component Tag는 제품 의미를 소유한다. 둘이 불일치하면 validation 오류다. profile 이름만으로 베이커 의미를 추론하지 않는다.

현재 `ULNPSurfaceCacheSubsystem`과 `ULNPWorldDeviceSpawnSubsystem`의 `ECC_WorldStatic`, 플레이어 조준의 `ECC_Visibility` 사용은 Phase 0에서 유지한다. 신규 channel 소비자 전환은 Phase 3 이후 정확성 테스트와 함께 수행한다.

## 5. 부유섬 계약

- 서로 떨어진 각 부유섬 보행면은 별도 Support Layer source다.
- 같은 방사 방향에 기본 지각과 여러 섬이 겹칠 수 있다. 중심에서 첫 hit만 저장하는 모델은 금지한다.
- 섬 측벽과 밑면은 기본적으로 `Blocker`이며 `Support`가 아니다.
- 한 메시 안에서 윗면과 측벽 의미를 안정적으로 분리할 수 없다면 Support proxy 또는 별도 component를 만든다.
- 서로 다른 섬은 geometry가 교차하지 않아야 하며 연결 여부를 반지름 차이만으로 추론하지 않는다.
- 섬 간 Traversal Link는 실제 정적 보행면이 이어진 경우에만 생성한다.

## 6. 동굴 키트 계약

내부형 구에서 땅속은 바깥쪽(반지름이 큰 쪽)이고 지각은 두께 없는 단면이다. 지각 한 장으로는 동굴 천장과 그 위 지면을 동시에 표현할 수 없으므로, 지하 공간은 별도의 닫힌 메시로 만든다(D-035).

### 구성

- **공동 모듈**: 반구·직육면체 같은 닫힌 공간. 몇 종을 미리 만들어 여러 옥탄트에서 재사용한다. 내부에 기둥 같은 `Blocker` 프랍을 둘 수 있다.
- **통로 조각**: 직선·곡선 통로. 지각 입구에서 공동까지 잇는다.
- **지각 입구**: 옥탄트 지각에 통로 1~2개만 뚫는다. 지각은 Geometry Script 생성기(`LNPOctantMeshGenerator`)로 만들므로 입구 절단을 생성 파라미터로 처리하는 것을 우선한다. C안에는 Boolean이 없다.

### 규칙

- 공동 하나당 통로는 1~2개다. 통로가 2개면 순환 경로가 생기며 Nav는 이를 정상적으로 다룬다.
- 복층, 수직 통로, 분기, 서로 교차하는 통로는 지원하지 않는다.
- 바닥은 `Support+Blocker+Static`, 천장·측벽은 `Blocker+Static` 컴포넌트로 분리한다.
- 외부 지면과 통로 바닥의 연결은 명시적인 입구 portal 후보로 기록한다.
- 방사 방향의 첫 표면이 지각이나 천장이어도 바닥 Support Layer를 잃지 않아야 한다.
- 투사체와 sweep은 Support 결과가 아니라 `LNPWorldExact` 기준으로 천장·측벽을 맞힌다.
- 공동과 통로는 옥탄트 경계를 넘지 않는다(D-030).

### 제작 가이드

바닥은 평면보다 **일정 반지름의 구면 캡**으로 모델링하길 권한다. 중력이 방사형이라 평면 바닥은 가장자리로 갈수록 반지름이 달라진다. 폭 20m 평면은 가장자리가 중심보다 약 20cm 높고 경사 약 2.3°, 폭 40m면 약 80cm·4.6°다. 걷기에는 문제없지만 구면 캡이면 체감상 완전히 수평이고 Support 샘플이 한 반지름에 모인다. 의무 규칙은 아니다.

## 7. 옥탄트 경계 계약

- 옥탄트 이음매는 기준 반지름에 고정된 단일 대칭 프로필이다(D-030). `SeamSignature` 하나로 세 변의 호환성을 표현할 수 있는 근거가 이것이다.
- 부유섬, 동굴, 마커가 스폰하는 요소의 영향 범위는 옥탄트 경계를 넘지 않는다. 각 옥탄트의 SurfaceData가 자기 내부만 소유하게 하기 위해서다.
- 경계를 넘는 보행 연결은 기본 지각 이음매뿐이며, 런타임 snapshot 게시 때 이웃 slot과 연결한다.

## 8. 검증 오류

다음은 경고가 아니라 bake 차단 오류로 취급한다.

- `Decoration`과 다른 지형 의미 태그의 동시 사용
- 역할 태그가 있지만 수명주기 태그가 없음
- 둘 이상의 수명주기 태그 사용
- `Dynamic`, `StatefulTraversal`, `Destructible` source가 옥탄트 LVI 안에 존재
- collision이 켜진 무태그 primitive component
- `MarkerId`가 비었거나 LVI 안에서 중복된 마커
- 옥탄트 경계를 넘는 Support source 또는 마커 영향 범위
- `LNPStaticSupport` profile에 `Blocker` 태그 부여
- `LNPDecoration` 이외 profile을 사용하는 `Decoration`
- 태그와 collision profile의 `LNPSurfaceSupport`·`LNPWorldExact` 응답 불일치

태그가 없는 기존 콘텐츠는 마이그레이션 기간에는 legacy 대상으로 보고 보고서에 집계한다. 신규 SurfaceData의 정식 source로는 사용할 수 없다.

