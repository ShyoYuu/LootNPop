# Hollow 동쪽 Dock/Rim 외피 V006

> 최신 지시: [Hollow 전체 단순화 재제작](Phase09_HollowLowPolyRebuild.md)으로 전환했다. 아래 국소 지각 수정 후보는Live Coding까지만 적용됐으며 수동Build 요청은 보류했다. 이 후보를 먼저 검증하는 기존 순서보다 전체 재제작·자동 검증 후마지막 PIE를 우선한다.

2026-10-10. V004는 급한 경사 윗끝, V005는 짧은 smoothstep의 시작부 곡률에서 Nav 연결이 실패했다. V006은 경사를 길게 바꾸고 내부 보행 폭 전체를 진단한 후보다. 형상·단면·UE 반입·Live Coding·생성·베이크와 두 정식 검사를 통과했다. PIE 게시를 확인하고 사용자 통행 검토를 요청했다.

## 수정

- OuterEastBridge의 시작(−12800,10000,H2400)은 그대로 두고 도착을X−10950→−10300으로 옮긴다. 높이차600cm/폭800cm를 유지하며 smoothstep 상승 길이는1850→2500cm, local 길이 분할은50cm 이하로 한다.
- OuterEastLanding은X−10300→−9800/H3000의 수평500cm 도착부다. 전체 진입축 양 끝·Rim 섬의 중심/높이/반경은 유지한다. 옆OuterEastRockJoin도 같은 경사 높이 함수를 쓴다.
- 교체 영역X−14000~−7000/Y7800~12000과Rim 길3개의 C++ 밑면 수정, 서쪽V015는 유지한다. 베이크 해상도/캡슐/충돌 기준·정식 검사는 바꾸지 않았다.

## 확인한 결과

| 항목 | 결과 |
|:---|:---|
| 닫힌 몸체 | 15464 verts/30928 tris/덩어리1, 토폴로지 오류0 |
| 최종 출력 | 15570 verts/30975 tris(Floor5279/Shell25696) |
| 최종 경계/교차 | 처방 경계169개, 예상 밖/방향/중복/퇴화/BVH 교차0 |
| 경계 보정 | 최대 이동5.697cm, 기존 C++ 꺾임23곳 삽입 |
| 독립 단면 | X−7000 최대0.353784cm/Y12000 최대0.002037cm |
| 경사 전체 진단 | N669의X−13100~−9950/Y9650~10350 내부 보행 폭, 연결565개, 하단 구 침투0/최근접 최소50.719cm(반경50cm) |
| UE staging | triangle 일치/bounds 최대차0.00390625cm/LOD1개 |
| staging·생산 충돌 | 충돌LOD0/별도complex없음/complex-as-simple/양면false 저장·재조회 |
| Live Coding | patch_4 Result: Success |

하단 구 진단은 실제 Nav 격자와베이커의50cm Support 경로/양방향 고정Up을 재구성한 Blender Floor BVH 샘플 검사다. Shell 전체의 Chaos 연속 캡슐과 실제플레이어 검사를 대신하지 않는다. 끝쪽 실패41개만 보던V005와 달리 경사 시작부~도착부의 내부 폭 전체를 포함한다. 가장자리/굽이/Rim 하부와 조립 경계는 UE/사용자 검토가 남는다.

복구본은 `Saved/SliceOctant/Hollow_00/EastRimV006/Before`, 최종blend/FBX/manifest·seam-verification·crest-ramp-all-edges-verification·unreal-verification은 같은V006 폴더다. 검사 좌표는EastRimV001/ramp-all-edges.json이다. V005는Authored/ArchiveEastRimV005에 UE move로 보존했다. Authored 바로 아래는서쪽V015와동쪽V006 두 역할씩이다.

## 생성·베이크·정식 검사·PIE

- 사용자 생성·베이크 완료와 저장 로그: authored4/errors0/lint0, Generated177089/Authored45022/부가 지형222111/crust231529 tris. 서쪽V015 단독보다부가 지형22010 tris 증가했다.
- Support101/Layers133/TotalSamples2128909/SupportPayload15107897 bytes, Nav137090 cells/666 components/2744 portals, 후보694126/643754/434223·간격 기각431479·포털203.433초. Spawn authored8/candidates12348/payload496412 bytes, 좁은 Atlas 경고9건(새Floor7건+기존2건)이다.
- 포털 시간은V005139.687초보다63.746초 길다. 이번 베이크 동안 distance field build 로그가 함께 발생했다. 단일 측정이므로 형상 변경만의 고정 비용으로 단정하거나 성능 개선으로 기록하지 않는다.
- HollowContent1.912384초/ProductionSurfaceDataDefinitions0.191497초, 실제2 passed/0 failed·검사 오류/경고0. 동쪽 능선→안부의양방향Found(2363/3699 expansions), 다른 외곽/비밀 경로와편성 검사를 통과했다. Pod8 headroom2650cm/edge800cm/투영42.2cm는 유지한다.
- EnemyDensity0 확인 후PIE 시작: Hollow slot2/5/6, generation1/8 validated slots/625 source bindings/registry generation2 게시. 이전부터 있던Mass CharacterMovementComponent 추출/초기화 로그와 구분한다.
- 사용자에게HollowOuterEast2에서 능선→Dock 굽이→새 경사→Rim 섬 왕복/가장자리 점프, slot2/local(−7800,12500,H0)에서Rim 하부 바깥 접근·통행·복귀와외형 확인을 요청했다. 실제 응답·최종 조립 전체/자동 캡슐은 미확인이다.

## 사용자 PIE 실패와 지각 접합 수정

정식 검사 통과 후 사용자 PIE에서 실패했다. slot2/local(−10444,9248,H−428)은 Pawn 중심이며 발밑 5m 광선은 none, AsyncFalling 반복이다. 사용자는 틈 입구 바닥의 작은 시각적 구멍과 이어진 벽 통과 후 틈에 갇혀 탈출할 수 없음을 보고했다. 경사 내부 565개 하단 구 진단을 지각 접합의 폐쇄·외벽 충돌 통과로 확대할 수 없다.

동쪽 코드는 경사 몸체의 넓은 지하 밑동 윤곽 전체에서 지각을 제거했다. 실제 벽 단면과 제거 경계 사이의 빈 공간을 우선 수정한다. 기존 V006 Blender 에셋은 유지하고 지각 절단만 실제 부피에 맞춰 검증한다. 수정 전 코드·LVI·SurfaceData는 `EastRimV006/GroundJoinBefore`에 보존했다. 수정·재생성·베이크·실제 통행 재검토가 남으며 동쪽 완료로 기록하지 않는다.

수정 후보는 기존 서쪽의 부피 절단 방식을 동쪽에 적용하되 V006의25m smoothstep/50cm 길이 분할과 도착부 경계를 따른다. 비공면 사각형의 두 삼각형 평면을 모두 사용한다. 단순 부피도 최종 Boolean 외벽 바깥으로 최대10.926cm 나가므로 옆면 절단 평면을15cm 몸체 안쪽으로 넣는다. 2,712개 지면 높이H−1000~1000/각 구간 중점·양쪽 표본에서 바깥0/3방향 광선 판정 불일치0, 최소 매입3.626cm다. 이는 최종 외피 BVH와 절단 부피의 독립 비교이며 실제 생성된 지각·Chaos 캡슐 통과를 대신하지 않는다. 진단은 `Scripts/Blender/VerifyHollowEastGroundJoin.py`, 산출물은 `ground-volume-inset0/15-verification.json`이다. Live Coding patch_5 Success. 생성 후 지면 연결 확인·베이크·정식 검사·사용자 재검토가 남는다.

동쪽 통과 후 외곽 구덩이 복귀→Shuttle 패널 우회로 이어간다. LoS는 별도 세션이다.
