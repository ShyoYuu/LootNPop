# Hollow_00 서쪽 접합 대표 구역 V012

2026-10-10. BackPocket 입구·잔도와 절벽 부착 암반의 접합 한 구역을 Blender 외피로 전환했다. V012는 생성·베이크했으나 비행 적의 천장 여유 검사에 실패했다. 현재 수정본은 [V013 인계](Phase09_BackPocketV013.md)를 따른다. V012를 최종 채택본으로 사용하지 않는다.

## 설계와 소유권

- 범위: 옥탄트 로컬 X **3800~8400cm**, Y **−8100~−3000cm**의 방사형 절단 구역. H는 원본 몸체의 −7600~6000cm를 따른다.
- 구역 안 외피 소유자: `Scripts/Blender/BuildHollowBackPocket.py` → `BackPocketV012/BackPocket.blend` → Authored Floor/Shell. 구역 바깥과 지각 절단·배치·Pod·앵커·패널은 C++ 소유다.
- 보존: W01 원본 윤곽과 정상 H=6000, 왕복 길의 명목 10m 폭, 기존 비밀 통로와 Pod 8개·앵커·패널 설정.
- 변경: 암반과 잔도를 Union한 뒤 동일 cutter로 굴을 Difference한다. 입구 앞에 남는 낮은 암반은 Y −8100/H2000 → −7350/H1500의 완만한 절단 바닥으로 연결한다. 해당 부분은 조립 후 기존 외곽 표면과 재확인한다.

| 위치·연결 | 단면·이동 역할 | 검증 상태 |
|:---|:---|:---|
| 뒤편 잔도 → BackPocket 입구 | 입구 잔도 H1500, 굴 바닥 H1500 | 원본/반입 확인, 실제 양방향 통행 미확인 |
| BackPocket 입구 → Pod 공간 | 폭 3600×Scale(1500) 로컬 cm, 천장 H4100, 뒷벽 Y−4300 | 원본 형상 확인, Pod 캡슐·적 출현·회피 공간 미확인 |
| 왕복 길 → 절벽 정상 | 기존 Arrival03/Summit의 H6000 보행면 보존 | 구역 경계 접합·전체 폭 미확인 |
| 정상 → Shuttle 패널 접안 | 기존 ShuttleDock/패널 경로 보존 | 인접 가장자리·패널 공간 미확인; 기존 점프 우회 보고도 미해결 |

평면 경계는 위 X/Y 사각형이다. 단면은 밑동 H−7600, 굴 바닥 H1500, 굴 천장 H4100, 정상 H6000 순서다. 굴은 닫힌 뒷벽이 있는 전투 공간이며 같은 입구로 복귀한다. 이번 작업에서 새 비밀 통로나 훅 연결을 추가하지 않는다.

다음 Generated part의 **구역 안 부분만** 제외한다. 구역 바깥은 유지한다.

`W01`, `BackPocketLip`, `WestPanel`, `WestBackHigh`, `MountainTurn03`, `ShuttleDock`, `BackPocket`, `MountainArrival01`, `MountainDeparture01`, `MountainArrival03`, `MountainSummit`.

`WestPanel`은 변경 구역의 협곡 쪽 경계를 가로지르는 기존 잔도여서 포함했다. 새 암반 안의 지각 Support는 W01/MountainTurn03와 해당 부착 경사 몸체의 범위에서 코드로 절단한다. 노출 지면과 외부 이음매는 보존 대상이다. 그 절단 결과 역시 아직 조립 검증 전이다.

## 산출물과 확인 범위

- 복구본: `Saved/SliceOctant/Hollow_00/BackPocketV001/Before/`의 코드와 Hollow 에셋 전체. 기존 `BeforeExteriorRebuild`도 유지한다.
- 현재 출력: `Saved/SliceOctant/Hollow_00/BackPocketV012/`. 편집 가능한 Boolean 원본, 구면 역할 메시, FBX, manifest, UE 반입 결과, 입구 미리보기.
- 독립 반입: `/Game/Maps/OctantWorkflow/HollowBackPocketV012`.
- 다음 생성에서 사용할 후보: `/Game/Maps/Hollow_00/Authored/SM_Hollow_00_BackPocketV012_Floor|Shell`. V012 두 개만 바로 아래에 둔다. 다음 버전과 동시 배치하지 않는다.
- 생성 전 두 메시의 존재와 complex-as-simple 설정을 검사한다. 실패하면 기존 LVI/지각을 변경하지 않는다.
- Live Coding **Result: Success** 확인(2026-10-10). 사용자가 생성 명령을 실행했고 LVI 저장 성공을 로그에서 확인했다. MCP에 콘솔 명령 실행 API가 없으므로 제작 가이드 §6에 따라 요청했으며 베이크는 요청하지 않았다.

| 검사 | 결과 | 한계 |
|:---|:---|:---|
| 닫힌 구역, 경계 뚜껑 제거 전 | 8682 verts / 17364 tris, 덩어리 1, 비정상 연결·방향 불일치·중복·퇴화 0 | 최종 조립 검사와 별도 |
| 경계 뚜껑 제거 후 | 5827 verts / 11473 tris, Floor 6463 + Shell 5010 | 열린 경계 183개는 기존 Generated와 접합해야 한다 |
| 비인접 삼각형 BVH 교차 | 검출 0 | 검사 정밀도 밖의 교차까지 보증하지 않는다 |
| 투영 중점 오차 | 전체 최대 0.913cm / 보행면 최대 0.172cm | 기존 Generated 단면과 좌표 일치 검사는 아직 안 했다 |
| UE 반입 | triangle 수 일치, bounds 오차 최대 0.003174cm, LOD 1개/충돌 LOD0, 별도 complex mesh 없음 | 개별 정점·실제 충돌·일방 통과 미확인 |
| 충돌 설정 | Floor/Shell complex-as-simple 재조회·저장 확인 | 역할/profile은 다음 LVI 생성에서 설정한다 |
| 열린 경계의 위치 | 183개 전부 지정한 네 절단 평면에 있음, 예상 밖 열린 경계 0 | Generated 단면과의 조립 일치는 별도 검사 |

원본 시험 도중 발견한 문제: 삼각분할하지 않은 Boolean 입력의 교차 결과 소실, 선택적 세분화 뒤의 일직선 퇴화 삼각형. 입력 삼각분할·평면 정리·퇴화 변 정리 후 gate를 통과한 V012만 반입했다. 실패본 V001~V010과 중간 V011은 production 후보가 아니다.

## 이어서 확인할 순서

사용자 생성 결과(2026-10-10 03:19 KST): `lint=0`, `authored=2`, `authoredErrors=0`, 기존 Generated 절단 대상 삼각형 6807개, crust 233467 tris/degenerateDropped=0, parts 107/Generated actors 208/Generated tris 188029, Pod 8/anchors 52/panels 2, 저장 성공. Authored 11473 tris를 더한 부가 지형은 **199502 tris**로 기준 193676보다 **5826 tris 증가**했다. 포털 비용 개선은 아직 측정하지 않았다. 기존 다른 Generated Shell의 nearly-zero normal/binormal 경고가 남았으며 경고 0으로 기록하지 않는다.

에디터에서 LVI를 여는 동안 기존 Generated들의 거리장 캐시를 생성했다. 이 로딩 시간을 베이크 시간으로 합치지 않는다.

실제 LVI 조회: 새 역할 액터 두 개의 identity 배치, Floor의 Support/Blocker/Static + `LNPStaticTerrain`, Shell의 Blocker/Static + `LNPStaticBlocker`, QueryAndPhysics, LOD0/complex-as-simple/양면 충돌 false를 확인했다. UE 뷰포트 캡처는 단색 표면 위주여서 접합 외형 통과 증거로 사용하지 않았다.

베이크 전 PIE를 시작했으나 기존 SurfaceData가 새 LVI의 액터 ID와 달라 `Slot 2 is missing runtime Support source 'StaticMeshActor_0.StaticMeshComponent0'`로 초기화에 실패했다. PIE를 종료했다. 아직 자동 캡슐/조립 경계 검증은 하지 못했으며, 제작 가이드에 이 의존 관계를 추가했다. 직접 플레이를 검증 수단으로 쓰기 위해 같은 옵션의 베이크와 EnemyDensity 0 설정을 사용자에게 요청했다. 해당 선행 베이크를 실제 충돌 통과로 기록하지 않는다.

1. 완료: Live Coding 성공 후 사용자가 TestMap03에서 생성 명령을 실행했다. 교체 로그·lint=0·authoredErrors=0·저장 성공을 확인했다.
2. 생성된 LVI에서 입구/굴 내부/정상과 네 절단 경계를 검사한다. 경계 단면은 열린 상태이므로 Generated와의 위치·높이·면 방향·균열 검사를 통과하기 전에는 외피 폐쇄로 기록하지 않는다. 내부 지각 절단이 과도하지 않은지도 확인한다.
3. 실제 반지름 42cm/반높이 96cm 캡슐로 전체 폭·측벽·천장·입구 양방향을 검사한다. 기존 도구로 불가능하면 짧은 사용자 PIE 검토를 요청한다. 검사 장치를 먼저 구현하지 않는다.
4. 조립 충돌 오류를 해결한 뒤 같은 옵션으로 베이크한다. 기준은 Support 108/Layers 123, 부가 지형 193676 tris, 포털 검사 173.124초/간격 기각 472093개다. 해상도를 낮추지 않는다.
5. `LootNPop.SurfaceNavigation.Bake.HollowContent`와 `ProductionSurfaceDataDefinitions`를 실행하고, 해당 구역의 입구→Pod→입구 왕복과 정상 접합을 사용자 PIE로 확인한다.

최신 사용자 문제 다섯 곳 전체의 해결, 벽보행 LoS, Hollow 전체 완료를 이번 결과로 선언하지 않는다. 최종 조립·베이크 전에는 기존 SurfaceData가 이전 LVI 버전의 데이터임을 명시한다.
