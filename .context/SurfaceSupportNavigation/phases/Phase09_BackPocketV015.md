# Hollow_00 서쪽 접합 대표 구역 V015

2026-10-10. V014에서 이전 갇힘 위치와 BackPocket 입구 양 가장자리의 실제 통행은 사용자가 이상 없음을 확인했다. 주변의 내부가 보이는 세로 틈을 추가 보고해 V015에서 해당 교체 경계만 보정했다. 형상·반입·Live Coding·생성·베이크·두 정식 데이터 검사는 통과했고 사용자가 실제 세로 틈 해소를 확인했다. 이어 남은 외피 재제작 진행을 지시했다.

## 보고 위치와 원인

- `WhereAmI`: Hollow slot4, local **(4619, −9954, H−397)**, 발밑 `SM_Octant_Hollow_00_R30000`. 캐릭터는 통과할 수 없지만 메시 내부가 보인다는 보고이며 화면에서도 세로 틈을 확인했다.
- V014 남쪽 교체 경계Y−10000 바로 옆이다. 주변 실제 WorldStatic overlap에는 `MountainDeparture01_Shell`, `MountainArrival01_Shell`, `MountainTurn01_Shell`, V014 Shell과 Crust가 잡혔다. 발밑 지각을 틈의 소유자로 해석하지 않는다.
- 기존 `BuildBridge`의 Departure01 시작 끝면은 길의 안쪽 윤곽을 거쳐 절벽 채움까지 폭을 나눠 생성한다. Blender 원본은 외측→절벽 쪽을 한 사각형으로 단순화했다. 두 형상의 방사 절단선이 달라 낮은 높이에서 약40cm 벌어졌다. V014의 독립 메시 토폴로지 검사로는 이 조립 단면 차이를 검출하지 못했다.

## 수정

`BuildHollowBackPocket.py`에서 Departure01의 시작 끝면을 기존 C++와 같은 폭 분할·삼각형 순서·구면 좌표로 재계산했다. 그 삼각형과Y−10000 방사 평면의 교선을 새 외피의 해당 경계에 사용한다. 경계 정점 최대42.899cm 보정, 기존 교선의 꺾임3곳을 추가해 전체 경계가 같은 선분을 따른다. 바닥 경계의 수평 변을 벽 경계로 잘못 선택하지 않도록 높이 변화와 소유 범위를 검사한다.

교체 범위X3300~8400/Y−10000~−3000·원본 Boolean 몸체·BackPocket 바닥H1500/천장H4150·10m 길·Pod/앵커/패널은 V014와 같다. C++는 후보/로그 버전만V015로 바꿨다. V014 에셋은 UE move로 `Authored/ArchiveV014`에 보존했으며 V015 Floor/Shell만Authored 바로 아래에 둔다. 변경 전 코드/전체 Hollow 에셋은 `Saved/SliceOctant/Hollow_00/BackPocketV015/Before`에 보관했다.

## 검사 결과

| 검사 | 결과 | 범위/한계 |
|:---|:---|:---|
| 닫힌 원본 | 10441 verts/20882 tris/덩어리1, 비정상 연결·방향 불일치·중복·퇴화0 | V014와 동일 |
| 끝막음 제거·경계 보정 후 | 7116 verts/14047 tris, Floor7394/Shell6653 | V014보다3 tris 증가 |
| 열린 경계 | 처방한 네 경계187개/예상 밖0 | 전체 조립 폐쇄와 별도 |
| 최종 토폴로지/교차 | 방향 불일치·중복·퇴화0, 비인접 BVH 교차0 | 엔진 캡슐 아님 |
| 투영 오차 | 보정 전 전체 최대0.909cm, 보행면 최대0.172cm | 보정 경계는 실제 C++ 교선과 별도 비교 |
| 독립 단면 비교 | C++ double 수식으로 재구성한4개 선분↔출력6개 선분, 각 변21점씩 양방향 최대0.013667cm | 실제 UE 메시 정점 추출 검사가 아닌 수식 재구성 |
| UE 반입 | triangle 수 일치, bounds 최대오차0.003602cm, LOD1개/충돌LOD0/별도complex 없음 | 보고 틈 외형은 사용자 통과, 전체 조립/캡슐은 별도 |
| 충돌 설정 | complex-as-simple/양면false 설정·재조회·저장 | 실제 캡슐과 별도 |
| 코드·생성 | Live Coding Success, V015 authored2/errors0/lint0·저장 | 실제 외형/충돌은 별도 |
| 정식 데이터 검사 | HollowContent/ProductionSurfaceDataDefinitions 2 passed/0 failed, 1.736초 | 실제 조립 외형/캡슐을 대신하지 않는다 |
| PIE 게시 | Hollow slot1/3/6, 8 validated slots/637 source bindings, 이음매 반지름 차0.000cm | 사용자가 보고 틈 해소 확인, 이후 PIE 종료 |

새 출력 후 방향·중복·퇴화 오류도 명시적으로 중단하도록 게이트를 보강했다. 버전의 메시/객체 이름이 이미 있으면 자동 `.001` 출력 대신 중단한다. 산출물은 `Saved/SliceOctant/Hollow_00/BackPocketV015`의 `.blend`, FBX, manifest, seam-verification, unreal-verification이다. staging은 `/Game/Maps/OctantWorkflow/HollowBackPocketV015`다.

## 생성·베이크·데이터 검사

- 사용자 생성·베이크 실행 응답과 완료/저장 로그를 확인했다. Generated8823 tris 부분 제외, crust232433/degenerateDropped0, Generated186054 tris/206 actors, Authored14047 tris/2 actors/errors0/lint0, Pod8/anchors52/panels2다. 부가 지형200101 tris는V014보다3/원래 기준193676보다6425 증가했다.
- 같은 옵션 베이크: Support105/Layers126/TotalSamples2139522/SupportPayload15180269 bytes. Nav137448 cells/681 components/2837 portals, 후보727967/675077/455453·간격 기각452616·포털150.360초. Navigation/Traversal payload1119340/76595 bytes, Spawn authored8/candidates12404/payload498652 bytes. 좁은 Atlas 경고2건 남음. V014151.595초와의 단일 차이를 고정 성능 개선율로 해석하지 않는다.
- V015에 정식 검사를 새로 실행했다. HollowContent1.578576초/ProductionSurfaceDataDefinitions0.157442초, 2 passed/0 failed·검사 오류/경고0·총1.736018초. Pod8 layer120/headroom2650cm/edge800cm/투영42.2cm/group571, 16 seed 전량 편성과 기존 연결 통과다.
- EnemyDensity0 확인 후 PIE 시작. Hollow slot1/3/6, generation1/8 validated slots/637 source bindings/registry generation2 게시, maxSeamRadiusDelta0.000cm/minNormalDot0.95296. 이번 풀 배치의 runtimeLayers889/localComponents3140/runtimeStaticComponents2285/seamLinks4330/blockedSeamNodes98/blockedSeamEdges4/navResident26.88MiB다. V014와 풀 배치가 달라 런타임 합계 변화 전체를 이 경계 수정 효과로 해석하지 않는다. 기존 Mass/Mover 메시지는 별도이며 런타임 오류0으로 주장하지 않는다.

## 남은 실제 검토

`LNP.Debug.TeleportToOctant 1 4800 -9954 -500`으로 보고 지점 바로 바깥에서 재검토를 요청했고, 사용자는 “틈이 없어졌어”라고 확인했다. 이 위치의 실제 외형은 통과로 기록한다. 새 답변은 세로 틈 해소를 명시했으며 모든 벽 접촉 동작이나 네 경계 전체를 확인한 것으로 확대 해석하지 않는다. 사용자 지시에 따라 동쪽 Dock/Rim 재제작으로 이어간다.

다른 경계 전체의 조립 폐쇄·자동 캡슐·정상 접합·기존 네 보고 구역·벽보행LoS·Hollow 전체 완료는 이번 검사로 확정하지 않는다.
