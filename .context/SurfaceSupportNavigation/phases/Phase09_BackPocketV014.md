# Hollow_00 서쪽 접합 대표 구역 V014

2026-10-10. V013 데이터 검사는 통과했으나 사용자가 기존 왕복 암반 쪽에서 일방 통과 후 갇힘을 보고했다. 외피 구역을 그 접합까지 넓힌 V014의 형상·반입·Live Coding·재생성·베이크·두 정식 데이터 검사는 통과했다. 사용자는 이전 갇힘 위치와 BackPocket 입구 양 가장자리 모두 이상 없음을 확인했다. 주변 남쪽 교체 경계의 내부가 보이는 틈을 새로 보고했으며 후속 수정은 [V015](Phase09_BackPocketV015.md)다.

## 보고와 확인된 사실

- `WhereAmI`: Hollow slot4, local **(3516, −9364, H2098)**, 발밑 `SM_Hollow_00_MountainDeparture01_Floor`.
- 사용자는 정확히 통과한 면은 모르지만 통과 후 갇혔다고 확인했다. 화면에서도 암반 사이의 캐릭터를 확인했다.
- 보고 당시 새 에디터 세션의 Hollow slot은1/4/7이었다. V013 Authored 로딩과8 validated slots 게시 로그를 확인했으므로 옛 production 버전을 검토한 것으로 해석하지 않는다. V014 재검토 PIE의 Hollow slot은0/4로 다시 확인했다.
- 실제 WorldStatic 충돌 overlap(플레이어 중심±120cm AABB)은 `MountainArrival01_Floor`, `MountainDeparture01_Floor`, `MountainTurn03_Shell`, `MountainArrival03_Shell`, `MountainSummit_Shell`을 반환했다. 원래 bounds만 조회하면 훨씬 많은 액터가 나오므로 구분했다.
- 발밑 메시가 일방 통과한 면의 소유자라는 증거는 없다. 위쪽 암반의 겹친 벽과 내부의 낮은 보행 바닥이 함께 남아 갇힘을 만드는 것으로 판단했다. 정확한 진입 면/경로 재현은 미확인이다.

## 수정 범위

V013의 X3800~8400/Y−8100~−3000에서 **X3300~8400/Y−10000~−3000cm**로 확장했다. 보고 지점과 상층 도착부·정상 굽이·낮은 도착부의 접합을 같은 Boolean 몸체로 처리한다. 대체 part 목록은 V013과 같다. 10m 왕복 길의 원본 중심선·폭, BackPocket 바닥H1500/천장H4150, Pod/앵커/패널·비밀 통로는 유지한다. 새 범위 안에서 상층 솔리드에 묻힌 낮은 바닥/벽은 외피에서 제거한다. 양면 충돌 설정으로 가리지 않는다.

`BuildHollowBackPocket.py`의 BOUNDS와 C++ 교체 사각형을 함께 갱신했다. Blender crop·열린 경계 검사도 같은 BOUNDS를 참조하도록 맞췄다. V013 production 에셋은 UE move로 `Authored/ArchiveV013`에 보존했고 V014 두 역할만 바로 아래에 둔다. 관련 코드·에셋 복구본은 `Saved/SliceOctant/Hollow_00/BackPocketV014/Before`다.

## 검사 결과

| 검사 | V014 결과 | 한계 |
|:---|:---|:---|
| 닫힌 원본 | 10441 verts/20882 tris/덩어리1, 비정상 연결·방향 불일치·중복·퇴화0 | 조립 폐쇄와 별도 |
| 경계 끝막음 제거 후 | 7113 verts/14044 tris(Floor7394/Shell6650) | 기존 Generated와 맞물리는 열린 경계184개 |
| 열린 경계/교차 | 예상 밖 경계0, 비인접 BVH 교차0 | 전체 조립 검사 아님 |
| 투영 오차 | 전체최대0.913cm/보행면0.172cm | 기존 단면 일치 미확인 |
| 보고 지점 아래 H2100→1000 Floor ray | H2000 내부 바닥 검출 없음 | Blender 형상 검사, UE 캡슐 아님 |
| 바깥(3516,−9900,H2098)→보고 지점 Shell ray | local(3516,−9413.542,H2098.304)에서 hit, 진입 방향·법선 내적−0.9922 | 실제 진입 경로를 확정하지 않는다 |
| UE 반입 | triangle 일치, bounds오차최대0.003602cm, LOD1개/충돌LOD0/별도complex 없음 | 최종 조립/통행 미확인 |
| 충돌 설정 | complex-as-simple, 양면false 재조회·저장 | 실제 캡슐 통과 미확인 |
| 코드·생성 | Live Coding Success, V014 authored2/errors0/lint0·저장 성공 | 최종 조립/캡슐은 별도 |
| 정식 데이터 검사 | HollowContent/ProductionSurfaceDataDefinitions 2 passed/0 failed, 1.733초 | 실제 통과·갇힘을 대신하지 않는다 |
| PIE 게시 | Hollow slot0/4, 8 validated slots/543 source bindings, 이음매 반지름 차0.000cm | 실제 플레이 재검토 대기 |

산출물은 `Saved/SliceOctant/Hollow_00/BackPocketV014`의 `.blend`·FBX·manifest·report-point-check·UE 반입 검사다. staging `/Game/Maps/OctantWorkflow/HollowBackPocketV014`, 현재 후보 `/Game/Maps/Hollow_00/Authored/SM_Hollow_00_BackPocketV014_Floor|Shell`.

## 생성·베이크·데이터 검사

- 사용자EnemyDensity0·V014 생성/베이크 완료 응답과 저장 로그를 확인했다. authored2/errors0/lint0, Generated8823 tris 부분 제외, crust232433 tris/degenerateDropped0, Generated186054 tris/206 actors, Pod8/anchors52/panels2다. Authored 포함 부가 지형200098 tris는 원래 기준193676보다6422 증가했고V013보다600 증가했다.
- 같은 옵션 베이크: Support105/Layers126/TotalSamples2139521/SupportPayload15180262 bytes. Nav137448 cells/681 components/2837 portals, 후보727967/675077/455453·간격 기각452616·포털151.595초. Spawn authored8/candidates12404/payload498652 bytes. 좁은 Atlas 경고2건은 남았다. V013 포털151.500초와 거의 같으며 원래 기준173.124초와의 단일 측정 차이를 고정 성능 개선율로 해석하지 않는다.
- 새 세션의 첫 RunTests는 발견된 검사0개여서 통과로 세지 않았다. DiscoverTests 후 실제 두 검사를 실행했다. HollowContent 1.579112초/ProductionSurfaceDataDefinitions 0.153897초, 2 passed/0 failed·검사 오류/경고0. Pod8 layer120/headroom2650cm/edge800cm/투영42.2cm·group571, 16 seed 전량 편성과 기존 연결 통과다.
- EnemyDensity0 확인 후 PIE 시작. Hollow slot0/4, 8 validated slots/543 source bindings/generation1 게시, maxSeamRadiusDelta0.000cm/minNormalDot0.95296. 기존 Mass CharacterMovementComponent 추출 오류/Mover 초기화 경고는 남아 런타임 오류0으로 기록하지 않는다.

## 남은 실제 검토

사용자는 이전 갇힘 위치와 BackPocket 입구 양 가장자리 모두 이상 없다고 확인했다. 이 범위의 실제 통행은 통과로 기록한다. 이어 slot4/local(4619,−9954,H−397)/발밑Crust에서 통과는 안 되지만 내부가 보이는 틈을 보고했다. 화면과 좌표를 보존한 뒤 PIE를 종료했으며 V015에서 해당 조립 경계를 보정한다. V014 전체 외형/조립 폐쇄·자동 캡슐·정상 접합 완료로 확대 해석하지 않는다.

이번 보고로 V013의 실제 통행 검토는 실패 상태다. 자동 데이터 검사 통과를 외피 충돌 완료로 기록하지 않는다. 다른 네 보고 구역·벽보행LoS·Hollow 전체는 범위 밖이다.
