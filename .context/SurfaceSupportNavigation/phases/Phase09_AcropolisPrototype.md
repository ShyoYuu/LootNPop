# Acropolis_00 부유 유적 그레이박스

현재 기준본과 남은 사용자 검토는 **[V015 경계 복귀로](Phase09_AcropolisV015.md)**를 따른다.

현재 형상은 **[V011 최종 접합 틈 수정](Phase09_AcropolisV011.md)**이다. 아래는 V005 제작·검사 기록이다. 사용자가 V005 전체 구성과 훅 배치를 PIE로 확인한 뒤 V009에서 섬 윤곽·크기와 동굴 깊이·천고·진입 굽이를 수정했다. V009 전체 승인 후 남은 Pocket_05 틈을 V011에서 닫았다.

2026-10-13, V005. **SurfaceData DA 생성·베이크·혼합 풀 및 현재성 검사를 완료했고, 후속 PIE 준비 요청으로 실제 `OctantPoolData.OctantDefinitions`에 Acropolis 정의를 추가·저장했다.** 에디터는 `TestMap03`을 열고 PIE 시작을 기다리는 상태다. 실제 조작·교전의 플레이 검증 완료를 뜻하지 않는다.

## 1. 공간 의도와 원본

고대 그리스·로마 유적을 추후 아트 방향으로 삼는다. 지금은 낮은 언덕, 열린 보스 전투장, 단순 기둥, 다수의 팔각 부유섬과 가파른 부유석으로 공간 역할을 표현한다. Bastion의 탑·나선과 Hollow의 대형 지하 공동을 복제하지 않는다.

- 레벨: `/Game/Maps/Acropolis_00/LVI_Octant_Acropolis_00`
- SurfaceData: `/Game/Maps/Acropolis_00/DA_OctantSurface_Acropolis_00`
- 지각: `/Game/Maps/Acropolis_00/SM_Octant_Acropolis_00_R30000`
- 현재 형상: `/Game/Maps/Acropolis_00/Authored`의 현재 역할 메시만 참조한다.
- 편집 원본: `Art/Octants/Acropolis_00/Acropolis_00_V005.blend`
- 형상 생성: `Scripts/Blender/BuildAcropolisOctant.py`
- 형상 검사·배치 원본·검토 이미지: `Scripts/Blender/VerifyAcropolisOctant.py`
- 에디터 조립: `Scripts/AssembleAcropolisOctant.py`를 Unreal MCP `ProgrammaticToolset.execute_tool_script`로 실행한다.
- 기준 지각 내보내기: `Scripts/ExportAcropolisSeam.py`. 기존 Meadow 에셋을 읽기만 한다.
- native 경계 확정: `Scripts/FinalizeAcropolisSeam.py`.
- 베이크·독립 검사용 풀 생성: `Scripts/BakeAcropolisOctant.py`.
- 기존 네이티브 자동 검사 실행: `Scripts/RunAcropolisValidation.ps1`.
- 저장 마커 자세·최종 충돌·입력값 확인: `Scripts/VerifyAcropolisReady.py`.
- FBX·세부 로그·이전 버전: `Saved/SliceOctant/Acropolis_00/`.
- 최종 manifest·배치·검사 보고서 사본은 원본 `.blend` 옆에도 보존했다.

다른 세션의 C++와 `Current.md`는 수정하지 않았다. 이번 맵은 공용 `BuildSliceOctant`에 아직 등록하지 않은 독립 제작 경로다. `BuildSliceOctant Acropolis_00`을 실행해 재생성할 수 있다고 가정하지 않는다.

## 2. 규격과 평면·단면

좌표는 공통 가이드의 접평면 `(X,Y,H)` cm다. H가 커질수록 구 중심 쪽으로 올라간다. 월드 반지름 기준은 30000cm이며 Meadow의 기존 이음매 형상을 유지했다. 경계 1500cm는 보존하고 다음 4000cm에서 내부 지면으로 완만하게 전환한다. 경계 형상 보존과 베이크 seam signature 일치는 별도 검증이다.

기준 FBX의 경계 평면 정점 1194개에서 반지름은 29999.996773~30000.001179cm였다. 300m 설계값에 대한 부동소수점 반입 오차 범위이며 경계를 확장한 것이 아니다.

| 공간 | 배치·높이 | 역할 |
|:---|:---|:---|
| 내부 지면 | H=-3500 | 경계보다 35m 낮춘 넓은 추락 회복 지면 |
| 중앙 언덕 | 정상 H=-1800, 평탄부 파라미터 반경 3300 | 주변 지면보다 17m 높은, 모든 방향에서 걸어 오르는 랜드마크 |
| 보스 기단 | 윗면 H=-1795, 파라미터 반경 3200 | 실제 폭 약 68m. 지면과 단차 5cm, 상공 중앙은 비움 |
| 바깥 섬 12개 | 중심 파라미터 반경 14500, H=1000~5500 | 세 방향 진입, 저·중층 순환 탐색 |
| 중간 섬 8개 | 중심 파라미터 반경 10100, H=6500~9500 | 바깥·상부를 잇는 분기·횡단 |
| 상부 섬 4개 | 중심 파라미터 반경 9600, H=11500~13000 | 높은 전투장과 탐색 끝의 횡단 선택 |
| 작은 지하방 5개 | H=-4500 바닥, H=-3700 천장 | 지면보다 10m 아래, 높이 8m, 방마다 Pod 하나 |
| 비보행 부유석 68개 | 여러 방사 높이·방향 | 비행형 NPC와의 사격 시야를 끊는 Blocker |

섬은 위에서 보면 세 띠지만 층마다 각도와 높이를 엇갈렸다. 실제 섬 중심→외곽 정점 거리는 약 14.2~20.0m다. 접평면 숫자를 실제 보행 거리나 원형 반경으로 그대로 읽지 않는다. 내부 지면 기준 섬 높이는 약 45~165m다.

```text
평면: 옥탄트 세 변 → 세 진입 방향 → 바깥 12섬 ↔ 중간 8섬 ↔ 상부 4섬
                                      ↘ 지상 회복·언덕 보스 ↙
       작은 지하방 5개는 방사 방향으로 흩어 놓고 각각 지상 경사로로 왕복한다.

단면 H(cm):
 13000    상부 전투섬
  9500    중간 전투섬      섬 사이 부유석, 중앙 상공은 열림
  1000    낮은 진입섬
     0    기존 이음매 기준
 -1800                    중앙 언덕·보스
 -3500    넓은 회복 지면 ─ 완만한 언덕 접근
 -3700       작은 방 천장
 -4500       방 바닥 ← 완만한 개별 진입 경사로
```

## 3. 연결과 배치

Pod 마커 33개는 부유섬 24개·지하 5개·지상 4개로 나눴다. 실제 LootPod Actor를 LVI에 직접 두지 않았다. `ALNPMassSpawnPoint`의 편성은 전역 기본값을 사용한다. 개별 NPC 종류·수량·보상과 전투 합류의 적합성은 아직 확정하지 않았다.

훅은 `ALNPPlacementMarker` 28개다. 섬마다 하나, 지상 회복 세 개, 중앙 보스 복귀 하나를 둔다. `BP_GrappleAnchor`와 마커별 사거리를 지정했고 `LNP.Grapple.AuthoredOnly`로 추가 시드 훅과 섞이지 않게 했다. GUID는 네이티브 Actor 생성 시 발급한 값을 저장하며 28개/33개의 유효성과 중복 없음을 확인했다. 조립을 다시 실행하면 새 ID가 생기므로 나중에 만든 SurfaceData는 반드시 재베이크해야 한다.

| 출발 → 도착 | 방식·의도 | 실패 시 회복 | 현재 확인 |
|:---|:---|:---|:---|
| 경계 진입 방향 3곳 → Outer_01/05/09 | 걸어서 진입 후 낮은 섬 훅 | 아래 열린 지면으로 낙하 | 대표 출발점에서 엔진 광선 확인 |
| 바깥 섬 ↔ 중간 섬 | 전투장을 고르는 분기·횡단 훅 | 아래 지면 또는 낮은 섬 | 섬당 2~9개 시야 연결 후보 |
| 중간 섬 ↔ 상부 섬 | 엇갈린 상승·횡단 훅 | 중간 섬 또는 지상 회복 | 24개 섬 모두 세 진입점에서 연결 그래프상 도달 |
| 높은 섬 → 지면·중앙 언덕 | 선택 하강 후 보스 접근 | 지상에서 걸어서 복귀 | 실제 낙하·착지는 PIE 보류 |
| 지면 → 지하방 → 같은 지면 | 넓은 개별 경사로, 왕복 탐색 | 경사로로 자력 복귀 | 세 폭 위치의 바닥·입구 접합 광선과 천장 확인 |

훅 시야 검사는 섬 중심을 그대로 출발점으로 쓰지 않고 목표 쪽 외곽으로 이동한 실제 로컬 시작점을 기록했다. 129개 연결 후보의 좌표는 `geometry-verification.json`에 있다. 이는 설계 연결 후보이며 NPC 정적 Nav 보행 링크가 아니다. 훅 광선 통과를 몸체 접근·종료 관성·착지·다음 타깃 발견 성공으로 확대하지 않는다.

## 4. 형상 소유권과 검사

지각은 Meadow 전용 동굴 입구 한 곳을 닫은 뒤 내부 지면을 변형하고, 새 방 5개의 소유 사각 영역을 정확한 평면으로 절단했다. 방의 바닥·경사로·벽·천장·지상 지붕은 같은 좌표 변환을 사용한다. 작은 방은 닫힌 단순 몸체의 조합이며 암반 내부의 의도된 겹침이 있다. Boolean 통합 외피나 전역 자기 교차 검사 완료로 표현하지 않는다.

V004 최초 베이크를 혼합 풀에 연결해 검사하자 경계 Valid 비트 불일치가 발생했다. FBX 왕복의 미세 오차와 방 절단 평면을 옥탄트 전체에 적용한 분할이 보존 띠까지 영향을 주었다. V005는 절단 범위를 각 방 주변으로 제한하고, 보존 띠의 정점 13989개를 **원본 UE MeshDescription에서 읽은 native 좌표**로 복원했다. 최대 보정은 0.007628cm 미만이다. 실제 양자화 반지름·Valid 비트 2208개가 Meadow와 모두 일치했고, 혼합 풀 검사에서도 seam delta=0을 확인했다. 단순한 300m 구면 재투영이나 FBX bounds 일치로 이 절차를 대체하지 않는다.

독립 섬의 Floor는 섬마다 별도 Support source다. Shell·기둥·부유석에는 Support를 붙이지 않았다. 정적 Component Tag와 `LNPStaticTerrain`/`LNPStaticBlocker`, collision LOD 0, complex-as-simple을 적용했다. 모두 identity 배치이며 FBX 정점이 최종 옥탄트 로컬 좌표를 소유한다.

| 검사 | 결과 | 한계 |
|:---|:---|:---|
| 형상 | 지각·단순 몸체에서 중복·퇴화·방향 불일치·비정상 비경계 edge 0 | 지각 외곽·의도한 방 절단부의 열린 경계는 별도 존재 |
| 기준 이음매 | 원본 표본 13988개 보존, native 정점 13989개 복원, 베이크 seam 표본 2208개 일치 | 플레이어 경계 통행은 PIE 보류 |
| UE 메시 반입 | 63개 메시, 반입 bounds 최대 오차 0.004883cm 미만 후 경계 native 보정 | native 보정은 위 별도 보고서를 기준으로 함 |
| 저장 형상 | 총 429681 tris, 지각 226321 tris | 3D 배치·동선은 V004와 동일, 지각의 불필요한 전역 분할 제거 |
| Component 설정 | 63개 정적 메시 profile·역할 확인 | 엔진 캡슐 sweep 아님 |
| Pod 바닥 | V005 저장본 엔진 광선 297개 통과, 기대 바닥 최대 오차 약 0.223cm | 실제 교전 검사 아님 |
| 지하방 | 입구·경사로·본실 광선 165개, 천장 5개 통과 | 캡슐 측면·천장 통행과 카메라 미확인 |
| 훅·마커 자세 | V005 저장본 광선 129개 통과, 훅/Pod 61개 위치 오차 0cm·현지 Up 정렬 확인 | 조준 조작·몸체·관성 낙하 미확인 |
| 의존성 | LVI가 다른 옥탄트 메시나 `_WorkInProgress`를 참조하지 않음 | 패키징 미실시 |

### 베이크 결과와 정식 검사

- 지각 격자 N=735, Support source 31개·Layer 46개, 총 표본 1360851개.
- Support 9635746 bytes, Navigation 802412 bytes, Traversal 53507 bytes, Spawn 408976 bytes, Conditional Patch 4 bytes(항목 0).
- Nav cell 98706개, 정적 component 255개, portal 1853개, seam endpoint 1107개.
- Spawn authored 33개, 후보 10168개. 베이커의 Pod 캡슐 여유 검사도 통과했다.
- 포털 후보 68370/66073/39126, spacing 기각 37273, portal bake 6.527초. 추출 1.76초, 지각 raster 0.16초, Layer raster 0.32초. 원본/캐시 상태가 다른 V004와 전체 시간 향상률을 단정하지 않는다.
- `ProductionSurfaceDataDefinitions`: 32개 시드·32개 서로 다른 배치·4종×8회전·순서 있는 16종 경계 조합·양방향 경로 2304개, `maxSeamRadiusDelta=0.000`.
- `ProductionSpawnComposition`: 기존 패키지 배치와 혼합 시드 총 33개 배치, Pod 부족 배치 0, NPC 84475/84475 전량 배정.
- `ProductionPoolCurrentAndDeterministic`: Acropolis 단독 검사용 풀에서 전체 베이크 두 번과 저장된 DA의 source hash·5개 stream이 모두 일치.
- 자동 검사 3개는 오류 0·경고 0으로 통과했다. 일반 엔진 로그에는 고삼각형 지각의 Recast geometry export 경고가 있으며 정식 부하 최적화는 별도다.

검사용 풀 사본은 `_WorkInProgress/Acropolis_00/Validation`에만 있다. 검사 프로세스의 `-ini:Game` 메모리 오버라이드로 사용했으며, 실제 `OctantPoolData`와 설정 파일은 수정하지 않았다. 실제 풀 파일 SHA256은 작업 전후 `586a0d6e79fcd0d127f2a6f15e99978282493b5aed1c8398e5351670a5497d29`로 동일하다.

## 5. 다음 검토

준비 작업과 실제 풀 등록을 완료했다. 사용자는 `TestMap03`에서 Play를 눌러 PIE를 시작한다. 기존 Meadow·Bastion·Hollow 정의를 보존하고 아래 Acropolis 정의를 추가했다. 네 정의 모두 8개 회전을 허용하므로 현재 선택 규칙에서는 시드와 무관하게 각 테마가 두 슬롯에 배치된다.

등록 후 실제 풀을 대상으로 `ProductionSurfaceDataDefinitions`와 `ProductionSpawnComposition`을 다시 실행해 2개 모두 성공·오류 0을 확인했다. 경계 검사 실행 중 외부 HTTP 연결 확인의 3초 timeout 경고 1건이 수집됐으며 옥탄트 검사 실패는 없었다. SurfaceData DA 로드, 실제 풀 저장 상태, 현재 맵 `TestMap03`, PIE 비실행을 확인했다. 콘텐츠 브라우저는 Acropolis DA를 선택해 두었다.

| 필드 | 입력값 |
|:---|:---|
| LevelAsset | `/Game/Maps/Acropolis_00/LVI_Octant_Acropolis_00` |
| SurfaceData | `/Game/Maps/Acropolis_00/DA_OctantSurface_Acropolis_00` |
| AllowedSlotRotations | `255` — 전체 8개 회전 |
| SeamSignature | `DysonSphere_R300m_V1` |

`Art/Octants/Acropolis_00/OctantDefinition.txt`는 UE가 직접 내보낸 구조체 텍스트다. 필요하면 새 정의 항목에 붙여넣을 수 있다. JSON 사본과 베이크·정식 검사·최종 저장본 검사 결과도 같은 폴더에 보존했다.

1. `TestMap03`에서 PIE를 시작한다. 위 정의는 이미 실제 풀에 저장했으며 추가 베이크나 코드 빌드가 필요 없다.
2. 실제 자연 진입 세 방향에서 낮은 섬 훅의 조준·몸체 접근·해제·착지를 확인한다.
3. 바깥→중간→상부→지상 회복→보스를 왕복하고, 넉백 추락 뒤 복귀 부담을 본다.
4. 방 5개의 입구·경사로·측면·천장을 실제 캡슐로 확인한다.
5. Pod별 NPC 출현과 추격·비행 엄폐 사용성을 본 뒤 필요한 편성을 지정한다.

검토점은 `AcropolisBoss`, `AcropolisEntry_1~3`, `AcropolisUpper_01~04`, `AcropolisPocket_01~05`다. PIE 시작 후 서버 콘솔에서 `LNP.Debug.TeleportToReview AcropolisEntry_1`로 첫 진입점으로 이동할 수 있다. slot을 생략하면 해당 이름의 첫 검토점을 사용한다. 특정 사본을 검토하려면 인자 없는 `LNP.Debug.TeleportToReview`로 slot을 확인한 뒤 이름과 slot을 함께 지정한다.

재제작 순서는 새 버전 형상 생성 → 역할별 반입/조립 → native 경계 확정 → 베이크 → 독립 검사용 풀 검사다. `FinalizeAcropolisSeam.py`로 production 지각을 갱신한 뒤에는 항상 재베이크한다. 마커 재조립도 ID가 바뀌므로 같은 원칙을 적용한다. 최종 저장본의 물리 광선 검사는 `VerifyAcropolisReady.py`를 commandlet이 아닌 `-ExecutePythonScript` 에디터 모드로 실행한다.

동선 제안으로 채택한 것은 **세 방향의 낮은 진입섬, 층 사이의 여러 횡단 선택, 넓은 지상 회복, 짧은 보스 복귀, 작은 방의 보행 탈출**이다. 상부에서만 계속 탐색하거나 계단을 반복해서 다시 오르는 부담을 줄이는 것이 목표다.
