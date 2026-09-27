# Phase 4b — 부유섬·동굴 키트 다층 Support

> 상태: 진행 중
> 예상 범위: 2~3세션
> 선행 조건: Phase 4a(완료), Phase 4 공통 전제(완료, `../history/Phase04b_Log.md`)

## 1. 목표

1. 지각이 아닌 Support source(섬 윗면·경사로·계단·동굴 키트 바닥)를 **Layer로 분리**하고, 각 Layer의 **sparse Atlas**를 에디터에서 결정론적으로 베이크해 `SupportPayload`에 지각과 함께 저장한다
2. 같은 방향에 여러 Layer가 겹칠 때 **발 위치 기준으로 Layer 하나를 고르는** 순수 조회 함수를 만든다. 다른 Layer로 임의 스냅하지 않는다
3. runtime hit identity가 소비할 **face→Layer 대응표**(D-037)를 만들고, exact trace의 `FaceIndex`가 표를 거쳐 Atlas와 같은 Layer로 해석됨을 증명한다
4. 4a fixture의 분리 sheet·양면 판으로 한 컴포넌트 안의 Layer 분리를 검증한다(Phase 3 Gate -1 B 이관 항목)

런타임 로더·snapshot 게시·hit identity registry의 `(slot, LocalLayerId)` binding은 Phase 5다. 4b는 에디터 베이크, runtime 모듈 순수 함수, 자동화까지만 한다.

## 2. 현재 코드의 출발점

- 베이커 `FLNPOctantSurfaceBaker`(Editor)는 Support source를 모두 추출·검증하고 지각(D-055)만 rasterize한다. 비지각 Support는 검증만 한다.
- 추출기 `FLNPOctantTriangleExtractor`는 cooked Chaos trimesh를 **내부 삼각형 순서**로 읽는다. ISM Support는 오류로 막는다. source 이름은 `GetPathName()` 전체 경로다.
- 지각 Atlas `LNPCrustAtlas`: 꼭짓점 중심 octahedral 격자, codec v1(샘플당 7바이트), `QuerySupport`, seam hash. `DataVersion` 2.
- `FLNPExactHitIdentity`는 이미 `FaceIndex`·`InstanceIndex`를 싣는다(Phase 3). Layer 해석은 비어 있다.
- 입력 옥탄트 세 개:

| LVI | 비지각 Support source | 4b에서 보는 것 |
|:---|:---|:---|
| `LVI_Octant_Fixture_Regression` | 섬 윗면 5개(섬 하나, 섬 둘 안쪽·바깥쪽, 가장자리 섬), 공동 바닥, 통로 바닥 | 3층 겹침, 섬 가장자리 유령 지면, 동굴 바닥이 지각·천장 뒤에서도 살아 있음 |
| `LVI_Octant_Fixture_Crust` | `FX_SplitSheet`(캡 2개), `FX_DoubleSidedPlate`, `FX_NegativeScaleSlab` | 한 컴포넌트 두 Layer, 양면 판의 바깥 winding은 Layer 없음, 음수 scale |
| `Meadow_00` | 큰 섬·섬 A·섬 B 윗면, 경사로 윗면(수직 구멍 포함), 섬 B 계단 3단·플랫폼(`SM_TerrainBox`), 동굴 키트 바닥 2개 | 30cm 단차 계단의 Layer 선택, 구멍 난 경사로 윗면, 실제 콘텐츠 크기·시간 |

## 3. 확정 결정

### 3.1 범위: ISM Support와 Support proxy는 미룬다(사용자 결정, 2026-09-27, D-058)

- 현재 콘텐츠에 둘 다 없다. 징검다리·잔해 같은 콘텐츠가 실제로 필요해질 때 fixture와 함께 구현한다.
- ISM·HISM Support는 지금처럼 베이크 오류다.
- **Support-only proxy도 베이크 오류로 막는다.** `Support` 태그가 있고 `Blocker` 태그가 없는 source(`LNPStaticSupport`)는 지금 일반 Support처럼 구워지는데, D-039가 요구하는 exact 짝 검증이 없으므로 차단한다. 구현 단위 0에서 넣는다.
- `../design/RegressionMap.md` §7의 해당 행은 "콘텐츠가 필요할 때"로 옮긴다.

### 3.2 sparse Atlas 격자: 지각과 같은 octahedral 격자 계열(사용자 결정, D-057)

- Layer 격자는 지각과 같은 면 `x+y+z=1` 위 꼭짓점 중심 삼각 격자이고, 분할 수만 지각 N의 정수배 `N_L = m·N`이다. 방향·인덱스·보간 수식은 `LNPCrustAtlas`와 같다.
- 정수배라서 지각 격자점은 모두 Layer 격자점이기도 하다. Phase 8 patch가 "base 격자 인덱스"로 저장될 때 지각·Layer 어느 쪽이든 같은 규약을 쓴다(`../design/DynamicTerrain.md` §5).
- **저장은 row span.** Layer마다 `j` 범위 `[J0, J0+RowCount)`와 행별 `i` 구간 `[IStart, IStart+Count)`만 가진다. 행별 누적 offset으로 `(i, j)` → 샘플 인덱스가 O(1)이다. footprint의 격자점을 모두 담는 최소 행 구간이며, 구간 안에서 광선이 빗나간 샘플은 `Valid=0`이다.
- 해상도 후보는 50cm(m=2, N_L=1,470)와 25cm(m=4, N_L=2,940)다. 간격은 기준 반지름 R 기준이며 섬은 R보다 안쪽이라 실제 간격이 조금 더 좁다. 구현 단위 3의 오차 측정으로 m을 정한다. 옥탄트 안 모든 비지각 Layer는 같은 m을 쓴다.
- Layer별 반지름 양자화 기준은 기준 반지름이 아니라 **Layer 자체 기준 반지름**(Layer 샘플 반지름 범위의 중간값을 step 단위로 반올림)이다. 섬은 R에서 30m 이상 안쪽이지만 Layer 안의 반지름 폭은 작다. 범위는 codec v1과 같이 ±81.9m이고, 넘으면 인코딩 오류다.

### 3.3 Layer 분리 규칙

- **지각은 Layer 0 하나다.** D-055로 식별하고, 4a dense Atlas를 그대로 쓴다. walkable 연결성으로 나누지 않는다(절벽이 있어도 한 Layer).
- **비지각 Support source는 walkable 삼각형의 연결 성분마다 Layer 하나다.**
  - walkable: 앞면 법선과 삼각형 중심의 지역 Up(`-normalize(centroid)`)의 dot ≥ 0.71(지각과 같은 기준).
  - 연결: 모서리를 공유하는 walkable 삼각형끼리 잇는다. 정점은 위치로 용접(허용 0.1cm, `HitMergeDistance`와 같다)한 뒤 비교한다. cooked trimesh가 정점을 이미 합쳤더라도 UV seam 복제에 의존하지 않기 위해서다.
  - non-walkable 삼각형은 어느 Layer에도 속하지 않는다(섬 윗면 가장자리 비탈, 양면 판의 바깥 winding, 슬래브 옆면).
  - 한 Layer 안에서 한 방향에 앞면 교차가 둘 이상이면 베이크 오류다(접힌 sheet, `../design/SurfaceBaking.md` "베이크 검증 실패 조건").
- **Layer ID(`LocalLayerId`, uint16)**: 0은 지각. 나머지는 source key 오름차순, 같은 source 안에서는 성분이 가진 최소 external face index 오름차순이다. 결정론적이며, 같은 콘텐츠를 다시 구우면 같은 ID가 나온다. Phase 8 patch Layer는 이 뒤에 MarkerId 순으로 붙는다.
- **Layer 간 겹침은 오류가 아니다.** 섬 B 계단(30cm 단차)과 그 밑 섬 윗면처럼 같은 방향 캡슐 높이 안에 두 Layer가 겹치는 것은 정상 콘텐츠다. 베이커는 겹침 샘플 수를 Layer 쌍별로 보고서에 적기만 한다. Layer 사이 보행 연결(step·portal)은 Phase 7 Nav가 만든다.

### 3.4 source key와 face→Layer 대응표(D-037)

- **source key**는 `<Actor FName>.<Component FName>`이다. LVI 패키지 경로를 넣지 않는다. slot Level Instance의 runtime component는 outer 경로가 다르므로 Phase 5 registry가 같은 key로 대응하게 하기 위해서다. key가 옥탄트 안에서 겹치면 베이크 오류다.
- **런타임 `FaceIndex`는 external index다.** 엔진은 trimesh hit의 내부 face 번호를 `GetExternalFaceIndexFromInternal`로 원본 mesh 삼각형 번호로 바꿔서 돌려준다(`CollisionConversions.cpp` `ConvertQueryImpactHitImp`). 현재 추출기는 내부 순서를 쓰므로 삼각형마다 external index를 함께 기록한다. external 표가 없는 trimesh(`ExternalFaceIndexMap` 부재)나 `TriMeshGeometries`가 2개 이상인 body setup은 face를 해석할 수 없으므로 오류다.
- 표 형식(source마다):
  - 모든 face가 같은 값(Layer 하나 또는 None)이면 **컴포넌트 단위** 한 값. 지각(Layer 0, non-walkable face 포함 전부)과 섬 윗면 대부분이 여기 해당한다.
  - 아니면 **face 단위** uint16 배열. 인덱스는 external face index, 값은 `LocalLayerId` 또는 None(0xFFFF). 배열 길이는 external index 최댓값+1이며, 쿠킹이 버린 퇴화 삼각형 자리는 None이다.
- 지각 face 단위 표는 만들지 않는다. 지각 non-walkable face(절벽)의 hit도 Layer 0이고, walkable 여부는 hit 법선이 결정한다.

### 3.5 같은 방향 다층 조회

순수 함수 `LNPSupportAtlas::QueryLayers(Atlas, LocalDirection, FeetRadius, MaxStepUp, MaxDrop, PreferredLayer)`다. 반지름이 작을수록 위(중심 쪽)다.

1. 후보 Layer: 방향을 담은 격자 삼각형의 꼭짓점 중 하나라도 Layer footprint 안에서 Valid인 Layer. 지각 포함.
2. 후보마다 4a 규칙으로 보간한다(세 꼭짓점 Valid이고 NeedsExact 아님).
3. 탐색 창은 `[FeetRadius - MaxStepUp, FeetRadius + MaxDrop]`이다.
4. 창에 걸치는 후보 중 **보간 실패인 Layer가 있으면 `NeedsExact`**를 반환한다. 걸침 판정은 그 Layer의 Valid 꼭짓점 반지름이 창 안에 있는지로 한다. 가장자리에서 가까운 Layer를 건너뛰고 먼 Layer로 떨어지지 않게 하기 위해서다.
5. 모두 보간됐으면 창 안에서 **반지름이 가장 작은(가장 위) Layer**를 고른다. `PreferredLayer`가 창 안에 있으면 그것을 우선한다. 창 안에 없으면 `NoSupport`다.

`MaxStepUp`·`MaxDrop`은 호출자가 넘긴다(적 exact 이동 기준값은 Phase 6에서 맞춘다). 이 함수는 `../design/RuntimeCollision.md` "Surface query API"의 조회 순서 1~5를 구현하며, snapshot·slot 변환은 Phase 5가 감싼다.

### 3.6 샘플 계산과 플래그

- Layer 격자점마다 구 중심에서 바깥쪽 광선을 쏴 **그 Layer의 walkable 삼각형만으로 만든 트리**에서 앞면 교차를 찾는다(`TMeshAABBTree3::FindAllHitTriangles`, watertight). 다른 Layer·천장·지각은 보지 않으므로 동굴 바닥이 지각·천장 뒤에 있어도 샘플된다.
- 교차 0이면 `Valid=0`, 1이면 반지름·법선, 2 이상(합침 거리 밖)이면 §3.3 오류다.
- 플래그 `Valid`·`Walkable`·`NeedsExact`와 판정 규칙은 4a 지각과 같다. Layer 경계 샘플은 이웃에 invalid가 생기므로 `NeedsExact`가 되어 섬 가장자리 밖에 유령 지면이 생기지 않는다.
- 이음매 스냅은 지각에만 적용한다. 비지각 Layer는 옥탄트 경계에 닿지 않는다(D-030, 기존 `ValidateSupportSource`).

### 3.7 codec v2

- `SupportPayload` 한 개에 지각과 비지각 Layer를 모두 담는다. codec version 2, `FLNPSurfaceBakeHeader::CurrentDataVersion` 3. 레이아웃 초안:
  - header: codec version, Layer 수, source 수, 지각 N, 기준 반지름, 반지름 step, 지각 seam hash 3개
  - Layer 표: Layer마다 source 번호, N_L, Layer 기준 반지름, `J0`·`RowCount`, 행별 `IStart`·`Count`, 샘플 offset
  - body: Layer마다 SoA(`int16` 반지름, 법선 2×`int16`, `uint8` 플래그). 지각 Layer 0 body는 v1과 같다
  - source 표: source key, face 표 종류와 값(§3.4)
- 정확한 바이트 배치는 구현 단위 2에서 확정하고 `../design/SurfaceBaking.md`로 옮긴다. v1 decode는 남기지 않는다. 저장된 SurfaceData 3개를 모두 다시 굽는다.
- `BakerSchemaVersion`을 2로 올리고 Layer 설정(m, 겹침 보고 캡슐 높이)을 bake settings hash에 넣는다.

## 4. 구현 단위

### 구현 단위 0 — 전제 검증과 proxy 차단

- 추출기가 삼각형마다 external face index를 기록한다(§3.4). 표 부재·다중 trimesh 오류.
- 자동화: 저장된 fixture 컴포넌트에 `LNPSurfaceSupport` trace(`bReturnFaceIndex`)를 쏴 hit `FaceIndex`가 추출기 external index의 삼각형과 위치·법선이 일치함을 확인한다. 음수 scale 슬래브를 포함한다.
- **패키지 빌드에서 `FaceIndex`가 나오는지 확인한다.** 에디터 PIE는 remap 표를 쓰지만 cooked trimesh에 external 표가 남는지는 `bSupportFaceRemapOnMeshBVH` 경로에 달려 있다. Development 패키지에서 `LNP.SurfaceNav.ProbeHitIdentity`로 지각·섬 윗면 hit의 `FaceIndex`가 -1이 아닌지 본다. -1이면 face 단위 표를 버리고 hit 위치 기반 해석으로 설계를 바꾼다(그 경우 사용자와 재논의).
- Support-only proxy 베이크 오류(§3.1). source key 중복 오류.
- 검증: 세 LVI 베이크가 여전히 통과, proxy 음성 사례 자동화.
- 상태(2026-09-27): 완료.
  - `FLNPBakeTriangleMesh::ExternalFaceIndices`와 `FLNPBakeSupportSource::Key`를 추가했다.
  - 추출 자동화 `Bake.FixtureTriangleExtractionMatchesExact`가 hit `FaceIndex`로 추출 삼각형을 찾아 위치·법선을 비교한다. 4개 컴포넌트 모두 불일치 0이다. 내부 순서와 external 번호는 지각 9,209개 중 9,208개가 다르고 슬래브만 같았다. 그래서 내부 순서로 표를 만들면 틀린다.
  - 수집기는 Support-only proxy를 오류로 막는다(`Schema.SourceContractValidation` 음성 사례).
  - 패키지 확인은 비-Shipping 명령 `LNP.SurfaceNav.ProbeFaceIndex`로 했다. 월드 중심에서 2,000방향으로 `LNPSurfaceSupport` trace를 쏜다. 부하 harness 보고 끝에서 `ProbePanels`와 함께 호출한다. Development 패키지 리슨 2P에서 호스트·게스트 모두 hit 2,000, `FaceIndex` 누락 0으로 PASS다. 설계를 바꿀 필요가 없다.

### 구현 단위 1 — Layer 분리와 face 표

- runtime 순수 함수: walkable 분류, 위치 용접 연결 성분, Layer ID 정렬, face 표 생성(§3.3·§3.4). 코드 위치 초안 `LootNPop/SurfaceNavigation/LNPSupportLayers.*`.
- 합성 자동화: 떨어진 캡 2개 → Layer 2, 양면 캡 → Layer 1과 바깥 winding None, non-walkable 띠로 끊긴 판 → Layer 2. 접힌 sheet 오류는 광선 교차로만 드러나므로 구현 단위 2에서 검사한다.
- 실제 입력: `Fixture_Crust`(split 2, double-sided 1, slab 1), `Fixture_Regression`(지각 + 6), `Meadow_00` Layer 수와 source별 face 표 종류를 보고한다.
- 상태(2026-09-27): 완료. runtime `LNPSupportLayers::BuildLayers`, 자동화 `Bake.SupportLayersSplit`(합성: 분리 sheet·양면·계단 벽·삼각형 soup 용접·Key 순서·오류 2종)과 `Bake.OctantSupportLayers`(세 LVI). 베이커는 Layer 분리를 실행하고 보고서에 Layer 수를 적는다(payload는 아직 지각만).
  - `Fixture_Crust`: Layer 5개. split 2(per-face 표), 양면 판 1(per-face, 바깥 winding None), 슬래브 1(윗면, 옆면 None).
  - `Fixture_Regression`: source 7개 = Layer 7개(지각, 섬 윗면 4, 공동·통로 바닥 2), 모두 컴포넌트 단위 표.
  - `Meadow_00`: source 11개 = Layer 11개. 섬 B 계단·플랫폼(`SM_TerrainBox`, 삼각형 48개) 4개는 옆면 때문에 per-face 표이고, 나머지는 컴포넌트 단위다. 구멍 난 경사로 윗면도 Layer 1개다.
  - source key는 actor FName이다. OFPA 옥탄트는 `StaticMeshActor_UAID_...` 형태이고 에디터 label과 다르다. runtime slot Level Instance에서 같은 이름이 나오는지는 Phase 5 registry binding에서 확인한다.

### 구현 단위 2 — sparse rasterization·codec v2·다층 조회

- runtime: row span 산출, Layer rasterize, codec v2 encode/decode, `QueryLayers`(§3.5·§3.6·§3.7). 4a `LNPCrustAtlas`의 격자·보간 코드는 공유하도록 옮기고 지각 전용 함수는 Layer 0 경로로 흡수한다.
- Editor: 베이커가 Layer 전체를 굽고 보고서에 Layer별 샘플 수·Valid·NeedsExact·크기·시간, Layer 쌍 겹침 수를 적는다.
- 합성 자동화: 같은 방향 3층 캡(창에 따라 각 층 선택), 계단형 30cm 단차(StepUp 안이면 위 칸), 가장자리 NeedsExact 전파(가까운 Layer 보간 실패 시 먼 Layer로 떨어지지 않음), 접힌 sheet 오류, codec 왕복.
- 세 옥탄트 재베이크, `Bake.OctantBakeDeterministic`·`CrustSeamMatch`·`CrustAtlasExactError` 통과 유지.
- 상태(2026-09-27): 완료. 형식 원본은 `../design/SurfaceBaking.md` "다층 Atlas 규약".
  - runtime `LNPSupportAtlas`(격자·row span `ComputeFootprint`·공용 `Rasterize`·codec v2·`QueryLayer`·`QueryLayers`). `LNPCrustAtlas`에는 이음매 스냅 rasterize와 seam 규약만 남겼다. codec v1 decode는 없다.
  - 레이아웃 초안(§3.7)에서 바꾼 점: header에 지각 N·기준 반지름을 두지 않고 Layer 표에 Layer마다 분할 수·기준 반지름을 둔다(지각은 Layer 0). 샘플 offset은 행 `Count` 누적으로 정해지므로 저장하지 않는다.
  - 베이커는 source를 Key 순으로 정렬한 뒤 굽는다. Layer 설정(m, 겹침 보고 높이, Layer 분리 walkable·용접 거리)을 bake settings hash에 넣었다. 겹침 보고 높이는 200cm로 두었다(보고 전용).
  - 합성 자동화 4개: `Bake.SupportAtlasFootprint`(row span이 전체 배치의 Valid·플래그를 그대로 보존), `SupportAtlasLayerQuery`(3층 캡·30cm 계단·가장자리 전파), `SupportAtlasFoldedSheet`(1.3바퀴 나선 경사로 → 오류), `SupportAtlasCodec`(왕복·거부 사례).
  - m=2 결과: `Meadow_00` 비지각 Layer 10개 샘플 25,623개, payload 2,088,939바이트(지각 대비 +10%). 섬 B 계단 3칸(Layer 5~7)은 샘플 14~15개가 모두 NeedsExact다. 칸이 50cm 격자로는 내부 샘플이 없을 만큼 좁다. 구현 단위 3의 m 결정과 구현 단위 4 계단 검증에 직접 걸린다.

### 구현 단위 3 — 오차 측정과 해상도 확정

- 자동화 `Bake.LayerAtlasExactError`: Layer마다 footprint 안 무작위 방향(고정 seed)에서 Atlas 조회와 그 컴포넌트만 대상으로 한 exact trace(`LineTraceComponent`)를 비교한다. 유령 지면, 반지름 오차 P50/P99/최대, 법선 오차, NeedsExact 비율. m=2·4 비교.
- 합격 기준 초안: 4a와 같이 유령 지면 0, 반지름 P99 ≤ 2cm·최대 ≤ 10cm, 법선 P99 ≤ 5°. 측정 뒤 확정한다.
- 기록: Layer payload 크기, 옥탄트 총 payload, 베이크 시간. 결과로 m을 정한다.

### 구현 단위 4 — face→Layer 종단 검증과 Phase 종료

- 자동화 `WorldCollision.LayerIdentity`: fixture 회귀 LVI를 8 slot으로 복제한 테스트 월드에서 §3 정적 사례마다 exact trace hit의 (source key, `FaceIndex`)를 face 표로 해석한 Layer가 같은 지점의 `QueryLayers` 결과 Layer와 같은지 확인한다. 섬 둘(3층), 동굴 바닥(지각·천장 뒤), 섬 가장자리 바깥(Layer 없음 → 지각).
- `Meadow_00` 섬 B 계단: 각 칸 위 발 위치에서 그 칸이 선택되고, 칸 옆에서는 섬 윗면이 선택된다.
- 에디터 빌드, 자동화 전체, `-game` 리슨 2P 스모크(D-031. 4b는 런타임 경로를 바꾸지 않으므로 무회귀 확인).
- 문서: `../design/SurfaceBaking.md`(다층 규약·codec v2), `../design/DataModel.md`(DataVersion 3), `../design/RuntimeCollision.md`(face 표 형식과 external index), `../design/RegressionMap.md` §3·§7, Roadmap·Current.

## 5. 완료 조건

- [x] Support-only proxy와 ISM Support가 베이크 오류로 막히고, 세 LVI는 통과함
- [x] 분리 sheet는 Layer 2개, 양면 판은 Layer 1개(바깥 winding은 None), 접힌 sheet는 오류
- [x] 세 옥탄트의 다층 SupportPayload(codec v2, `DataVersion` 3)가 베이크·저장되고, 두 번 구운 결과가 같음
- [x] `QueryLayers`가 3층 겹침·계단 단차·가장자리에서 규칙대로 Layer를 고르고, 가장자리에서 다른 Layer로 떨어지지 않음(합성 입력. `Meadow_00` 계단은 구현 단위 4)
- [ ] Layer Atlas와 exact 오차가 측정됐고, 그 결과로 해상도 m과 허용값이 문서에 확정됨
- [ ] 동굴 바닥 Layer가 지각·천장 뒤에서도 샘플되고, 섬 가장자리 밖에 유령 지면이 없음
- [ ] 8 slot에서 exact hit `FaceIndex` → face 표 → Layer가 Atlas 조회 Layer와 일치하고, 패키지 빌드에서 `FaceIndex`가 유효함
- [ ] 지각 Atlas 4a 검증(이음매·오차·결정론) 무회귀
- [ ] 에디터 빌드, 자동화 전체 통과, `-game` 리슨 2P 스모크 통과
