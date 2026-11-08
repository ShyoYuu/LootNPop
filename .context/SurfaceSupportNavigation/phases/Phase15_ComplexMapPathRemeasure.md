# Phase 15 — 복잡한 맵 기준 추격 경로 재측정

> 상태: 계획(미착수)
> 착수 조건: Grove_00·Ridge_00 완료와 옥탄트 풀 추가, Meadow_00 풀 제외
> 작성: 2026-11-08

## 1. 왜 다시 재는가

Phase 10은 flow field·계층형 A*·개선 A*를 비교했고 개선 A*를 유지했다(D-079·D-080). 그 판정의 측정 조건이 지금 production 방향과 다르다.

- 당시 풀은 Meadow·Bastion·Hollow 3종이었다. Acropolis(2026-10-13)·Pyramid(2026-10-16)는 측정 뒤에 들어왔다.
- 추격 재생 S2·S3은 Meadow 섬(`-IslandSlot 5`)에서 캡처했다. 로그에도 "재생 경로는 50~85m, 2~4 Tile의 짧고 트인 경로"로 적혀 있다. 계층형이 확장을 못 줄인 근거도 이 짧고 트인 경로였다.
- "production 경로 비용은 병목이 아니다"(V011 경로 tick P95 0.014~0.021ms)도 3종 풀 기준이다.
- Meadow는 레벨 디자인을 고려하지 않은 기능 테스트 맵이라 production 풀에서 빠진다. production은 Pyramid·Acropolis 수준의 복잡도를 기준으로 한다.

복잡도가 올라갔다는 신호는 이미 있다. 같은 seed 1768·같은 방식의 무작위 쌍 100개에서, A*가 30,000 확장 안에 찾은 쌍이 92개에서 51개로 줄었다. 상한 없이 풀면 쌍당 평균 확장이 105,646이다(Phase 10은 4,240). 계층형 사전 계산 메모리는 4.47MiB에서 9.44MiB로 늘었다(Cluster 7,226 → 15,206, Cluster당 바이트는 같음). corridor 고리 1 정제는 100쌍 중 8쌍이 기본 확장 상한 30,000을 넘었다. 무작위 쌍이라 실제 추격 비용으로 그대로 옮기지는 않는다(2026-11-08 `Nav.Hierarchy`).

## 2. 가설

- 개선 A*는 상대 순위가 내려갈 수 있다. 비용이 우회 면적에 비례하고, 다층 실내·공중섬에서는 직선거리 휴리스틱이 크게 과소평가한다.
- 계층형 A*는 긴 우회 경로에서 이득이 커질 수 있다. 다만 메모리가 Cluster 수에 비례하고, segment 정제의 경로 길이(P95 1.09~1.10)와 overlay 재계산 4~5ms 문제가 남는다.
- flow field는 같은 목표를 쫓는 적이 많을수록 유리하고 Phase 10의 첫 후보다. 대신 우회가 크면 반경 밖 폴백이 늘고, 층이 많으면 field 빌드가 무거워진다. overlay 토글 때 field 전체를 다시 짓는 문제도 남는다.
- 계층형과 flow field 중 어느 쪽이 앞설지는 측정 전에 정하지 않는다.

## 3. 순서

1. **Meadow 제외 전 의존 정리.**
   - 이음매 기준 메시(`LNPSliceOctant`의 `SeamSourceMesh`)와 경계 표본 비교 원본은 Meadow다. 풀에서만 빼고 원본은 보존한다.
   - 추격 harness가 Meadow 섬 slot을 가정한다(`RunPhase13Gate.ps1 -IslandSlot`, `LNPLoadBaseline`). 새 풀에서 링·목표를 둘 slot을 다시 정한다.
   - Meadow 이름을 직접 쓰는 테스트와 seed 고정 테스트의 배치가 바뀐다. 풀 변경 후 `LootNPop.SurfaceNavigation` 전체를 한 번 돌려 기준선을 새로 남긴다.
2. **전제 재확인.** V011과 같은 방식으로 새 풀의 패키지 경로 tick P95·큐 대기 P95를 잰다. 여전히 병목이 아니면 개선 A*를 유지하고 여기서 끝낸다.
3. **새 캡처.** Meadow가 아닌 복잡한 옥탄트(Pyramid·Acropolis·Grove·Ridge, 보조로 Bastion·Hollow)에서 N700Chase·N2000Chase에 해당하는 추격 요청을 캡처한다.
4. **재생 비교.** `Nav.RequestCostReplay`의 세 방식 변형과 `ComparePhase10Replay.py`로 같은 입력을 비교한다. Phase 10에서 재지 못한 두 조건(먼 추격자가 없는 경우, 움직이는 목표·목표 양자화)도 함께 본다.
5. **채택 기준 재검토.** 기준 4(추가 메모리 ≤ 8MiB)는 콘텐츠 규모에 맞춰 다시 정한다. 계층형을 다시 쓴다면 저수준 정제 확장 상한(`MaxCorridorExpansions` 30,000)도 함께 정한다. `Nav.Hierarchy`는 이미 Cluster당 바이트로만 메모리 회귀를 강제하고 절대 크기는 기록만 한다. 기준 2(burst 완료 tick P95 −50%)가 복잡한 맵에서도 같은 의미인지 확인한다. 기준 3(경로 cost 비율 P95 ≤ 1.05)은 유지한다.

## 4. 산출

- 새 풀 기준 판정표(Phase 10 단위 5 형식)와 결정 기록(D-080 갱신 또는 새 결정).
- 재생·캡처 명령과 입력 형식은 [Phase 10 로그](../history/Phase10_Log.md) 2026-10-13 단위 5 "채택 판정"을 따른다.
