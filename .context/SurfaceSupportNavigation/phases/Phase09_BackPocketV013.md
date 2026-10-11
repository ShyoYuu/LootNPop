# Hollow_00 서쪽 접합 대표 구역 V013

2026-10-10. [V012](Phase09_BackPocketV012.md)의 베이크 실패를 수정한 버전이다. 천장을H4150으로50cm 높여 생성·베이크·두 정식 검사·PIE snapshot 게시를 통과했으나, 사용자 검토에서 기존 왕복 암반 쪽 일방 통과 후 갇힘이 보고됐다. 현재 수정본은 [V014 인계](Phase09_BackPocketV014.md)를 따른다. V013을 실제 통행 통과본으로 기록하지 않는다.

## V012 베이크와 실패 원인

사용자 베이크 완료와 SurfaceData 저장을 확인했다. 원래 높이 차 2600cm의 평평한 구면 천장이 실제 2600cm로 베이크됐다. 기존 생성형 천장의 Pod 여유는 2646cm였으므로 원본 치수만 옮긴 결과가 실제 충돌 높이까지 같지는 않았다.

현재 Flyer01은 배회 상한 2300 + 몸 반지름 80 + clearance 50 + 고정 여유 200 = **2630cm**를 요구한다. `HollowContent`는 Pod 천장 검사 32건과 16 seed×8 slot의 Pod8 비행 편성 128건에서 실패했다. `ProductionSurfaceDataDefinitions`는 통과했다. 검사 코드는 EnemyDensity CVar를 읽지 않으므로 밀도 0이 이 실패의 원인이 아니다. 적 설정이나 검사 기준을 낮추지 않는다.

| 항목 | 변경 전 기준 | V012 |
|:---|---:|---:|
| 지각 tris | 236941 | 233467 |
| 부가 지형 tris(Authored 포함) | 193676 | 199502 |
| Support sources / Layers | 108 / 123 | 106 / 126 |
| TotalSamples | 2287854 | 2152613 |
| Nav cells / components / portals | 138871 / 734 / 2871 | 138037 / 699 / 2872 |
| 포털 후보 | 781478 / 727946 / 474964 | 760139 / 707193 / 466489 |
| 간격 기각 | 472093 | 463617 |
| 포털 검사 시간 | 173.124초 | 111.527초 |

해상도 변경 없이 비교했다. 단일 실행 시간 감소를 성능 보장으로 확대하지 않는다. 좁은 Atlas Layer 경고 2건이 남았고 Layers·부가 삼각형은 증가했다. V012는 편성 검사 실패본이므로 비용 수치만으로 채택하지 않는다.

## V013 산출물과 검사

- 코드 원본: `Scripts/Blender/BuildHollowBackPocket.py`, `POCKET_ROOF=4150`.
- 로컬 산출물: `Saved/SliceOctant/Hollow_00/BackPocketV013/`의 `.blend`·FBX·manifest·UE 반입 검사.
- 변경 전 복구본: 같은 폴더 `Before/Hollow_00`. V012 원본/FBX는 그대로 보존한다.
- UE staging: `/Game/Maps/OctantWorkflow/HollowBackPocketV013`.
- 현재 Authored 후보: `/Game/Maps/Hollow_00/Authored/SM_Hollow_00_BackPocketV013_Floor|Shell`.
- V012 production 에셋은 UE move로 `Authored/ArchiveV012`에 보존한다. 빌더의 바로 아래 비재귀 스캔 대상에서 제외한다. 다음 생성에서 `authored=2`로 중복 미배치를 확인한다.

닫힌 원본은 8680 verts/17360 tris/덩어리 1, 비정상 연결·방향 불일치·중복·퇴화 0이다. 경계 끝막음 제거 후 5825 verts/11469 tris(Floor6463/Shell5006). 열린 경계183개는 전부 지정 네 평면 위이며 예상 밖 열린 경계0, 비인접 BVH 교차0. 투영 오차 전체 최대0.909cm/보행면0.172cm다.

UE triangle 수 일치, bounds 오차 최대0.003174cm, 각 LOD1개/충돌 LOD0/별도 complex mesh 없음, complex-as-simple·양면 충돌 false·저장을 확인했다. 이 결과는 최종 조립 경계·캡슐·베이크·실제 통행을 보증하지 않는다.

V012/V013 Floor 정점을0.001cm 정밀도로 비교해 동일함을 확인했다. Blender의 Pod 중심(local6000,−5300) 천장 방사선은 약2650.176cm다. UE 베이크와 실제 충돌 검사를 대신하지 않는다. Live Coding은03:36 KST **Result: Success**다. TestMap03에서 생성 완료 뒤 베이크를 실행하도록 사용자에게 요청했다.

V013 생성·저장 로그 확인(03:37 KST): `authored=2/authoredErrors=0/lint=0`, Generated6807 tris 부분 제외, crust233467 tris/degenerateDropped0, Generated188029 tris/208 actors, Pod8/anchors52/panels2. Authored 포함 부가 지형은199498 tris로 원래 기준보다5822 증가했다. 베이크 명령은03:38 KST에 시작했다. 이후 생성형 메시 거리장 캐시 로그가 이어졌으며 그 시간과 포털 검사 시간을 구분한다.

## 다음 검증

1. 완료: Live Coding 성공·사용자 V013 재생성·저장, `authored=2/authoredErrors=0/lint=0`.
2. 완료: 사용자 같은 옵션 베이크·저장, `HollowContent`/`ProductionSurfaceDataDefinitions` **2 passed/0 failed**, 검사 오류·경고0, 총1.711초. Pod8 layer86/edge800cm/headroom2650cm/투영11.3cm, 16 seed 전량 편성과 기존 연결 검사를 통과했다.
3. 완료: EnemyDensity0을 조회한 뒤 PIE 시작. Hollow slot0/4/6, 8 validated slots/551 source bindings/generation1 게시, maxSeamRadiusDelta0.000cm. 이전 missing runtime Support source 문제는 이번 시작에서 해소됐다. 기존 Mass CharacterMovementComponent 추출 오류가 남아 전역 런타임 오류0으로 기록하지 않는다.
4. 대기: `HollowPocketBack 0`에서 양쪽 벽 가까이 입구 왕복·천장/벽·깜박임/돌출, `HollowShuttleDock 0`에서 마지막 왕복 굽이와 정상 접합을 사용자에게 요청했다. 현재 도구의 자동 캡슐 검사는 미실시다. 네 경계의 전체 정점 일치·조립 폐쇄는 아직 입증하지 않았다.

V013 베이크(03:41 KST): Support106/Layers126/TotalSamples2152613/SupportPayload15271126 bytes, Nav138037 cells/699 components/2872 portals, 후보760139/707193/466489·간격 기각463617·포털 **151.500초**, Spawn authored8/candidates12459/payload500852 bytes. N735/기본 Support25cm 옵션은 변경하지 않았다. 좁은 Atlas Layer 경고2건은 남았다. V012의111.527초와도 실행 편차가 있으므로 고정적인 비용 감소율로 해석하지 않는다. 원래 기준보다 부가 삼각형5822개/Layers3개가 늘었다.

최신 다섯 보고 전체·Hollow 전체·벽보행 LoS의 완료를 선언하지 않는다.
