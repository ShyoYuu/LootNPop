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

## 3. Component 단위 규칙

- 의미 판정의 최소 단위는 `UPrimitiveComponent`다. 한 Actor 안에 역할이 다른 컴포넌트가 있어도 각 컴포넌트를 독립 판정한다.
- 하나의 HISM 컴포넌트에는 같은 의미 조합만 넣는다. 나무 blocker와 비충돌 장식은 HISM 컴포넌트를 분리한다.
- `Support`는 실제로 agent capsule을 지지할 의도가 있는 면에만 부여한다. 측벽·밑면·동굴 천장은 메시가 같은 Actor에 있어도 자동 Support로 간주하지 않는다.
- `Blocker`는 현재 `ECC_WorldStatic`과 동의어가 아니다. 기존 시스템 전환이 끝날 때까지 둘은 병존한다.
- 베이커는 태그 없는 geometry를 암묵적으로 지형으로 승격하지 않는다. 태그 누락은 validation 오류로 보고 명시적으로 수정한다.
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

## 6. 단순 동굴 계약

- Phase 0~9의 필수 범위는 입구 하나와 단일 floor corridor다.
- floor는 `Support+Blocker+Static`, 천장·측벽은 `Blocker+Static`으로 분리한다.
- 외부 지면과 동굴 floor의 연결은 명시적인 입구 portal 후보로 기록한다.
- 복층, 수직 통로, Y자 분기, 서로 교차하는 corridor는 필수 범위가 아니다.
- 방사 방향의 첫 표면이 천장이어도 floor Support Layer를 잃지 않아야 한다.
- 투사체와 sweep은 Support 결과가 아니라 `LNPWorldExact` 기준으로 천장·측벽을 맞힌다.

## 7. 검증 오류

다음은 경고가 아니라 bake 차단 오류로 취급한다.

- `Decoration`과 다른 지형 의미 태그의 동시 사용
- 역할 태그가 있지만 수명주기 태그가 없음
- 둘 이상의 수명주기 태그 사용
- `Dynamic`, `StatefulTraversal`, `Destructible` source를 immutable static payload에 포함
- `LNPStaticSupport` profile에 `Blocker` 태그 부여
- `LNPDecoration` 이외 profile을 사용하는 `Decoration`
- 태그와 collision profile의 `LNPSurfaceSupport`·`LNPWorldExact` 응답 불일치

태그가 없는 기존 콘텐츠는 마이그레이션 기간에는 legacy 대상으로 보고 보고서에 집계한다. 신규 SurfaceData의 정식 source로는 사용할 수 없다.

