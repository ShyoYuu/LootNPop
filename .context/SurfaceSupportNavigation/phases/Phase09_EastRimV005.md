# Hollow 동쪽 Dock/Rim 외피 V005

2026-10-10. Blender/Codex 재시작 뒤 연결을 확인하고 동쪽V004를 반입·생성·베이크했다. 정식 검사에서 Ridge04→안부 경로가 양방향 실패해V005로 경사 윗끝을 수정했다. V005도 생성·베이크 후 같은 경로가 실패했고, 끊긴 위치가 경사 시작부로 옮겨졌다. 최신 후보는 [V006](Phase09_EastRimV006.md)이다.

## V004 결과와 실패 원인

- 생성: authored4/errors0/lint0, Generated177089·Authored38998·부가 지형216087 tris, crust231529 tris. 기존 서쪽V015와 동쪽V004 두 역할씩이다.
- 베이크: Support101/Layers133/TotalSamples2128908/SupportPayload15106927 bytes, Nav137071 cells/667 components/2744 portals, 후보694113/643741/434216·간격 기각431472·포털138.868초. Spawn authored8/candidates12349/payload496452 bytes. 좁은 Atlas 경고10건 중 새 Floor 소스가8건이다. 이전V015의 경고2건을 무시하거나 새 경고를 안전하다고 판정하지 않는다.
- `Bake.HollowContent`는1.687440초에 실패했다. Ridge04(−11000,11300,H2400)→안부(−6000,6000,H4500)의 왕복2건이며 나머지 외곽 경로/비밀 경로/편성 검사는 통과했다. `Runtime.ProductionSurfaceDataDefinitions`는 올바른 전체 이름으로 별도 실행해0.166275초에 통과했다. 첫 호출의 잘못된 Bake 접두어로 실행되지 않은 검사는 통과로 세지 않는다.
- 새 NavigationPayload를 codec 그대로 읽어 Floor layer121의 두 주요 component가 X−10950의 경사 윗끝에서 갈라졌음을 확인했다. 기존 SurfaceSupport/충돌 기준은 바꾸지 않았다.
- 실제 끊긴 Nav edge 좌표41개에서 베이커의50cm Support 경로/양방향 고정 Up을 Blender Floor BVH로 재구성했다. 반경50cm 캡슐 하단 구의 최근접 거리 최솟값48.264cm, 침투35개다. 급한 경사→평지 꺾임에서 이동 경로가 바닥을 침범하는 형상 원인이다. 이 진단은 Chaos 연속 캡슐 sweep/실제 플레이어 검사가 아니다.

## V005 수정과 확인

- OuterEastBridge의 높이를 양 끝H2400→3000 그대로 두고 cubic smoothstep으로 연결한다. 중심선·길 폭·Rim 높이·교체 영역·서쪽V015는 유지한다. 해당 길만 local 길이100cm 이하로 나누고 OuterEastRockJoin의 높이 함수도 같은 경사에 맞춘다.
- 닫힌 몸체13514 verts/27028 tris/덩어리1, nonmanifold·방향·중복·퇴화0. 이번 내부 반대면 상쇄0개다.
- 최종13548 verts/26931 tris(Floor5044/Shell21887), 처방 경계169개/예상 밖0/BVH 교차0.
- 같은41개 edge의 진단은 최솟값50.246cm/침투0개다. 이 작은 여유를 최종 UE 검사 통과로 확대하지 않는다.
- 독립 double 단면 비교는 X−7000 최대0.353784cm/Y12000 최대0.002037cm로V004 수준을 유지한다.
- UE staging과 생산 후보: triangle 수 일치, bounds 최대차0.00390625cm, LOD1개/충돌LOD0/별도complex없음, complex-as-simple·양면false 설정/저장/재조회 통과.
- V004를 `Authored/ArchiveEastRimV004`로 UE move하고V005 두 역할을 바로 아래에 둔다. C++ preflight/로그 버전만V005로 갱신했다. Live Coding patch_3 Result: Success.

산출물은 `Saved/SliceOctant/Hollow_00/EastRimV005`의 원본/최종blend·FBX·manifest·seam-verification·crest-verification·unreal-verification이다. V004 조립/베이크의 복구본은V005/Before에 있다. 생성기는 `Scripts/Blender/BuildHollowEastRim.py`, 진단기는 `VerifyHollowEastRimCrest.py`다. 실패 좌표41개는EastRimV001/crest-edges.json에 보존했다.

## V005 생성·베이크 결과

- 사용자 완료 응답과 저장 로그: authored4/errors0/lint0, Generated177089/부가 지형218067/crust231529 tris. Support101/Layers129/TotalSamples2128908/SupportPayload15107260 bytes, Nav137075 cells/669 components/2744 portals, 후보694123/643751/434221·간격 기각431477·포털139.687초. Spawn authored8/candidates12348/payload496412 bytes, 좁은 Atlas 경고6건이다.
- 정식2개 실제 실행: HollowContent1.637988초 실패(같은 능선 왕복2건), ProductionSurfaceDataDefinitions0.160125초 통과. 새manifest에V005만 있으며 이전V004를 베이크한 경우가 아니다.
- 새NavigationPayload에서 윗끝은 연결됐고 X−12557~−12176의 시작부에4개 component가 더 생겼다. 실패 위치142개를 같은 하단 구 진단으로 검사해침투110개/최솟값49.512cm를 재현했다. 기존41개 윗끝만 검사한 결과를 경사 전체 통과로 해석한 범위 부족을 확인했다.
- V006은 수평 도착부 일부를 경사에 포함해 전체 상승 길이를1850→2500cm로 늘린다. 앞으로는 이전 실패 위치만이 아니라 시작부~도착부의 내부 보행 폭 전체 격자 연결을 함께 검사한다. 실제 UE 베이크·플레이어 검토는 별도다.
