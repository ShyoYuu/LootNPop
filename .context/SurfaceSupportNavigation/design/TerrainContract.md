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
| 정적 Support proxy | `Support`, `Static` | 포함 | 보행면 후보 | 대응 exact geometry가 있을 때만 제외 가능 |
| 정적 벽·천장·프랍 | `Blocker`, `Static` | 제외 | 점유·clearance 포함 | 포함 |
| 움직이는 패널 | `Support`, `Blocker`, `Dynamic` | 제외 | 계획 경로에서 제외 | 포함 |
| 상태형 기둥·다리 | `Blocker`, `StatefulTraversal` | 제외 | 정적 link 제외 | 포함 |
| 파괴 가능한 바닥 | `Support`, `Blocker`, `Destructible` | immutable payload에서 제외 | Runtime Overlay로 반영 | 파괴 전 포함 |
| 파괴 가능한 blocker | `Blocker`, `Destructible` | 제외 | Runtime Overlay로 반영 | 파괴 전 포함 |
| 장식 | `Decoration` | 제외 | 제외 | 제외 |

`LNPStaticSupport` 같은 Support-only proxy를 실제 보행면으로 사용하려면 같은 표면을 제공하는 exact collision component와 명시적 authoring association이 있어야 한다(D-039). 베이커는 proxy와 exact geometry의 오차·coverage를 검증한다. 대응 exact geometry가 없는 proxy는 spawn·grounded 이동에 사용할 수 없으며 분석·보조 데이터 전용이다.

상태형 지형이 안정 상태에 도달해도 원본 태그를 `Static`으로 바꾸지 않는다. 안정 상태, 활성 link, 지역 revision은 런타임 상태이며 immutable 옥탄트 데이터와 분리한다.

### 배치 위치

- `Static`·`Decoration` source는 옥탄트 LVI에 직접 둔다.
- `Dynamic`·`StatefulTraversal`·`Destructible` source는 옥탄트 LVI에 두지 않는다. 이 태그는 서버가 스폰하는 동적 요소 Actor 클래스의 컴포넌트에만 붙는다(D-026). LVI에는 그 위치를 가리키는 Placement Marker만 둔다.
- 회귀 맵(`RegressionMap.md`)의 동적 fixture는 Phase 3에서 마커 방식으로 전환하기 전까지 예외로 둔다.

### 서버 스폰 정적 장치

훅 앵커·스프링 런처처럼 **배치는 런타임에 서버가 하지만 스폰 뒤 transform과 형상이 바뀌지 않는** 장치는 월드 의미상 정적이다(D-045).

- 수명주기는 `Static`, profile은 역할에 맞는 `LNPStatic*`을 쓴다. 스프링 런처는 밟고 올라서는 발판이므로 `LNPStaticTerrain`이다. 훅 앵커는 현재 충돌 geometry가 없다.
- LVI 밖에 존재하므로 옥탄트 정적 SurfaceData bake 입력이 아니다. exact query는 스폰 직후부터 이 장치를 맞힌다.
- Nav 점유·Support 반영 방식은 Phase 7 Nav 설계에서 정한다.
- 동적 요소(`Dynamic`·`StatefulTraversal`·`Destructible`)와 달리 Runtime Overlay revision을 만들지 않는다.

충돌을 어디에 두는지는 표현 방식에 따라 두 가지다. 월드 의미는 둘 다 같다.

| 표현 | 예 | 충돌 소유 |
|:---|:---|:---|
| 항상 Actor | 스프링 런처 | Actor의 component 자체. Actor가 LOD와 무관하게 존재하므로 충돌도 LOD와 무관하다 |
| Mass 엔티티 + LOD 승격 Actor | LootPod | 엔티티 수명에 묶인 **collision proxy**. 승격 Actor는 충돌을 갖지 않는다(D-047) |

#### Collision proxy 규약 (LootPod 기준 구현)

필드에 놓이는 Mass 기반 상호작용 오브젝트가 새로 생기면 이 규약을 따른다.

- 월드 서브시스템이 오브젝트 종류별로 collision 전용 ISM 하나를 가진 Actor를 소유한다. 서버와 각 클라이언트가 로컬로 만들고, 네트워크로 복제하지 않는다.
- 엔티티가 생기면(서버 스폰, 클라이언트는 복제 도착) 인스턴스를 추가하고, 엔티티가 사라지면 제거한다. 존재·위치는 이미 MassReplication이 전달하므로 추가 트래픽이 없다.
- 형상은 단순 캡슐이고 profile은 `LNPStaticBlocker`다. Pawn(Mover), 투사체 exact, PureEntity exact가 모두 같은 형상을 본다.
- Mass 시각화 ISM(LOD 밴드별 표시)은 충돌 소스로 쓰지 않는다. LOD에 따라 생기고 사라지므로 머신·거리마다 충돌이 달라진다.
- 승격 Actor의 메시는 `NoCollision`이다. 상호작용 판정용 overlap(`LootingZoneSphere` 등)만 남긴다.
- hit identity(D-037)는 ISM instance index를 오브젝트의 Mass 엔티티 핸들로 바꾼다. 엔티티 핸들은 머신 로컬 값이며, 복제되지 않는 서버 전용 식별자(LootPod의 `PodID`)는 필요하면 서버가 fragment에서 읽는다. 식별자를 복제 페이로드에 추가하지 않는다.
- proxy ISM은 `SetRemoveSwap()`으로 만든다. 엔진 기본 제거는 `RemoveAt`이라 뒤쪽 index가 전부 한 칸씩 밀린다. swap 모드에서는 마지막 인스턴스가 빈 index로 옮겨지므로(physics body 포함), index→엔티티 표도 같은 방식으로 게임 스레드가 갱신하고 registry generation을 올린다.
- 인스턴스 추가는 생성 observer가 아니라 proxy 미보유 태그를 조회하는 게임 스레드 프로세서가 한다. 서버는 엔티티 생성 뒤에 transform을 채우기 때문이다. 제거는 식별 태그의 Remove observer가 한다.
- 추가·제거 요청은 큐에만 쌓고, hit identity snapshot 게시 직전에 ISM과 표에 한꺼번에 반영한다. ISM 제거는 physics body index를 즉시 swap하므로, 요청 시점에 반영하면 다음 게시까지 Mass worker가 바뀐 index를 이전 표로 해석해 다른 엔티티를 돌려준다. 대가로 proxy 충돌의 생성·소멸이 최대 1프레임 늦다.
- LootPod 캡슐: `SM_MatPreviewMesh_01` bounds(X ±128.7, Y ±119.9, Z 0~255.5cm)에서 반지름 128cm, 반높이 128cm, 중심은 Pod 로컬 Up +128cm이다. 반높이가 반지름과 같아 실질적으로 구이므로 엔진 `/Engine/BasicShapes/Sphere`(반지름 50cm)를 2.56배로 쓴다(`ULNPLootPodCollisionProxySubsystem`).
- 클라이언트 proxy 위치는 양자화된 복제 위치에서 만들어지므로 서버와 수 cm 차이가 날 수 있다.

## 2-1. Placement Marker 계약

- 마커는 LVI 안의 비복제 Actor이며 충돌은 `NoCollision`, 시각화 컴포넌트는 editor-only다. Terrain Contract 태그를 갖지 않는다.
- 마커는 `UPROPERTY FGuid MarkerId`를 저장한다. 배치할 때 발급하고, 복제·붙여넣기로 생긴 마커는 새 ID를 받는다. `AActor::ActorGuid`는 editor-only 데이터라 cooked 런타임에 없으므로 사용하지 않는다.
- 마커는 스폰할 Actor 클래스와 그 요소의 로컬 authoring 데이터(경로 spline, 안정 상태 transform, 파라미터)를 가진다.
- 런타임 식별자는 `(slot, MarkerId)`다. 같은 LVI가 여러 slot에 들어가도 구분된다.
- 스폰할 Actor 클래스가 Conditional Patch를 여는 요소라면, 베이커가 읽을 static patch source mesh를 제공해야 한다. 런타임 표현이 Geometry Collection이어도 patch source는 Static Mesh다.
- `MarkerId`, marker transform, Actor class, 경로·상태 파라미터, patch source mesh와 안정 상태 transform은 Conditional Patch stale hash 입력이다(D-041).

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
| `LNPDestructibleSupport` | Destructible | Block | Ignore | `Support+Destructible` proxy, 대응 exact geometry 필수 |
| `LNPDestructibleBlocker` | Destructible | Ignore | Block | `Blocker+Destructible` |
| `LNPDestructibleTerrain` | Destructible | Block | Block | `Support+Blocker+Destructible` |
| `LNPDecoration` | WorldStatic | Ignore | Ignore | `Decoration` |

profile은 물리 응답을, Component Tag는 제품 의미를 소유한다. 둘이 불일치하면 validation 오류다. profile 이름만으로 베이커 의미를 추론하지 않는다.

### 전환 순서

- Phase 3 Gate -1에서 production 지형의 `LNPWorldExact` response를 먼저 audit·마이그레이션한다(D-036).
- 베이크 의미를 위한 `LNP.Surface.*` Component Tag 전환은 실제 베이커 적용 시점인 Phase 4까지 나눠 진행할 수 있다.
- exact response audit가 끝나기 전에는 투사체·Mover·MassWorldCollision의 production 기본 경로를 `LNPWorldExact` 단독으로 바꾸지 않는다.
- 전환 CVar는 비교와 롤백용이며 Shipping 경로에 legacy `ECC_WorldStatic` fallback을 영구 유지하지 않는다.

현재 `ULNPSurfaceCacheSubsystem`과 `ULNPWorldDeviceSpawnSubsystem`의 `ECC_WorldStatic`, 플레이어 조준의 `ECC_Visibility` 사용은 Phase 3 Gate -1 전까지 유지한다. 신규 channel 소비자 전환은 production response audit·마이그레이션과 정확성 테스트를 통과한 뒤 Phase 3에서 수행한다.

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
- 기준 반지름은 월드 전역 값(`ULNPSettings::SphereRadius`)이다. Mesh Terrain 부유섬·동굴 옥탄트부터 30,000cm로 제작한다(D-046). 반지름이 다른 옥탄트는 이음매가 맞지 않으므로 같은 pool에 넣지 않는다. 기존 25,000cm 옥탄트(`LVI_Octant_Meadow_00`)는 30,000cm 전환 시 재제작하거나 pool에서 뺀다.
- Mass 위치 복제의 int16 캡(`../../Guide_NetBandwidth.md` §2.4)은 반지름이 아니라 좌표 성분마다 걸린다. 방향 `d`, 반지름 `r`인 점의 조건은 `r × max(|dx|, |dy|, |dz|) ≤ 32,767cm`다. 옥탄트 중심 방향은 약 56,700cm, 좌표평면 45° 방향은 약 46,300cm까지 여유가 있다. 좌표축 방향에서만 `r ≤ 32,767cm`로 묶인다.
- 좌표축 방향은 옥탄트 꼭짓점이다. slot 회전은 축을 축으로 보내므로 어느 slot에서도 꼭짓점은 축 위에 있다. 30,000cm 월드에서 꼭짓점 부근의 여유는 약 2,767cm다. 지각보다 바깥쪽(반지름이 큰 쪽)으로 파고드는 동굴은 꼭짓점 부근에 두지 않는다.
- SurfaceData 베이커는 slot transform 적용 뒤 geometry의 좌표 성분 최대 절댓값이 캡과 여유를 넘지 않는지 검사하고, 넘으면 bake 차단 오류로 보고한다. 캡을 넘은 엔티티 위치는 경고 없이 clamp되기 때문이다.
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
- slot transform 적용 뒤 geometry 좌표 성분이 int16 위치 복제 캡과 여유를 넘음(§7)
- `LNPStaticSupport` profile에 `Blocker` 태그 부여
- `LNPDecoration` 이외 profile을 사용하는 `Decoration`
- 태그와 collision profile의 `LNPSurfaceSupport`·`LNPWorldExact` 응답 불일치
- playable Support proxy에 대응 exact geometry association이 없거나 허용 오차를 초과함
- Destructible 역할 조합과 세 profile 중 하나가 일치하지 않음

태그가 없는 기존 콘텐츠는 마이그레이션 기간에는 legacy 대상으로 보고 보고서에 집계한다. 신규 SurfaceData의 정식 source로는 사용할 수 없다.
