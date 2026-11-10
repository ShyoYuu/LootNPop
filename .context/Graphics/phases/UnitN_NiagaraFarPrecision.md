# 단위 N — Niagara 원거리 비표시 원인 확정

> 상태: 완료(2026-11-10) — 원인 확정, 대체 중력 검증, 규칙 반영(GX-011), V3b 게이트 해제
>
> **결론 요약:** 원인은 `PointAttractionForce`(`Use Falloff=false`)다. 이 설정에서는 힘이 끌개까지의 거리에 비례한다. 월드 원점이 끌개이면 24,000cm에서 첫 틱에 약 24,000g를 받아 1.5km 밖으로 사라진다. float 정밀도·바운드 문제가 아니며, 인력만 끄면 월드 스페이스도 원거리에서 원점과 똑같이 동작한다. 근거는 [단위 N 로그](../history/UnitN_Log.md).
> 작성: 2026-11-10
> 게이트 대상: JustDodge V3b(월드 스페이스 이동 꼬리, 1차 이미터 규칙 재확인) — GX-009
> 출발점: [TechDesign_ParrySystem.md](../../TechDesign_ParrySystem.md) §8, [JustDodge VisualDesign](../../JustDodge/design/VisualDesign.md) §4

## 1. 목적

원점에서 약 24,000cm 떨어진 구 내벽에서 월드 스페이스 이미터(`NS_Guard_Block`·`NS_Parry_Success`의 `Sparks`)가 한 픽셀도 그려지지 않았던 원인을 **실험으로 확정**하고, 프로젝트 이미터 규칙(로컬/월드 스페이스, 힘 모듈, 속도 모듈)을 그 결론에 맞춘다. 그래플 앵커 성능 개선과는 별개 문제다.

## 2. 지금 알고 있는 사실

- 2026-09-20 관측: 같은 시스템의 `Flash`(로컬)는 보였고 `Sparks`(월드)는 크기·색·머티리얼·힘 모듈을 바꿔도 화면이 1px도 변하지 않았다. `bLocalSpace=true`로 바꾸자 보였다.
- 반례: `NS_Impact_Generic`의 `Sparks`는 월드 스페이스인데 같은 월드에서 정상이었다. 차이로 지목된 것은 속도 모듈이다(가드 `AddVelocity` From Point / 임팩트 `AddVelocityInCone`). 그러므로 "원점에서 멀면 월드 스페이스는 무조건 안 그려진다"는 아니다.
- 현재 `NS_Guard_Block`의 `Sparks` 설정(2026-11-10 MCP로 읽음):
  - `AddVelocity` From Point, 속도 500, `Velocity Origin = Engine.Emitter.SimulationPosition`, `Origin Offset = (0,0,-7)`·좌표계 **Local**
  - `PointAttractionForce` 강도 −980, `AttractorPosition = (0,0,0)`, 반경 200,000 / `CurlNoiseForce` 230 / `Drag` 2
  - CPU, Self·Once, 버스트 20, 수명 0.18~0.4초

## 3. 기존 가설의 재평가

§8의 유력 가설은 "24,000cm 좌표에서 float32로 위치를 빼면 유효 자릿수가 무너진다"였다. 수치로는 약하다.

- float32의 24,000 부근 간격(ulp)은 2^(14−23) ≈ **0.002cm**다. 위치 뺄셈의 오차가 이 정도라면 방향·크기 모두 눈에 보이게 틀어질 수 없다.
- UE5 Niagara는 월드 스페이스 위치를 `Position` 타입(LWC 타일 기준 상대값)으로 다루므로 실제 정밀도는 이보다 낫다.
- `Velocity Origin`도 월드 원점이 아니라 `Engine.Emitter.SimulationPosition`(월드 시뮬에서는 컴포넌트 월드 위치)이다.

그래서 이 단위는 정밀도 가설을 전제로 두지 않고, 아래 후보를 **입자가 존재하는가 → 어디에 있는가 → 그려지는가** 순서로 가른다.

| 후보 | 설명 | 구분 증거 |
|:---|:---|:---|
| H1 거리 무관 | 월드 스페이스 + 이 모듈 조합이 원점 근처에서도 안 보인다(거리와 무관한 설정 문제) | 원점 근처 대조군에서도 안 보임 |
| H2 위치 변환 오류 | `Origin Offset`(Local) 같은 Local↔Simulation 변환이 월드 시뮬에서 컴포넌트 이동량을 한 번 더 더해, 입자나 속도 기준점이 원점 거리만큼 밀린다. 원점에서는 무해하고 멀수록 커지는 버그라 "원거리 전용" 증상과 맞는다 | 디버거의 입자 `Position`이 컴포넌트에서 ~24,000cm 떨어져 있음 |
| H3 힘 방향 | 월드 시뮬에서 `AttractorPosition (0,0,0)`이 행성 중심이 되어 입자를 지면 쪽으로 밀거나, 노이즈·드래그와 합쳐 즉시 가려진다 | 입자 위치가 지면 아래로 들어감 / 힘 모듈 끄면 보임 |
| H4 바운드·컬링 | 고정 바운드 또는 월드 시뮬의 동적 바운드 계산이 틀려 컴포넌트가 통째로 컬링된다 | 입자 위치는 정상인데 `fx.Niagara.Debug` 바운드 표시가 엉뚱함 / 바운드 고정 해제로 해결 |
| H5 렌더 단계 | 입자는 정상 위치에 있는데 정점 생성·셰이딩 단계에서 사라진다(LWC 렌더 오프셋 등) | 위 넷이 모두 아님 → RenderDoc에서 드로우콜 유무·정점 위치 확인 |

## 4. 실행 계획

원본 에셋(`NS_Guard_Block`·`NS_Parry_Success`·`NS_Impact_Generic`·JustDodge 시스템)은 **수정하지 않는다.** 시험 에셋은 `/Game/Dev/UnitN/` 아래 복제본으로만 만들고, 단위 종료 시 삭제하거나 git에 올리지 않는다.

### N-1 재현 (에디터, 빈 레벨)

1. `NS_Guard_Block`을 `NS_UnitN_Sparks`로 복제하고 `Sparks`만 남긴 뒤 `bLocalSpace=false`, 힘 모듈을 2026-09-20 이전처럼 켠 상태를 만든다. `NS_Impact_Generic`도 `NS_UnitN_Impact`로 복제해 대조군으로 쓴다.
2. 빈 시험 레벨에 두 시스템을 **원점 근처(0,0,0)**와 **원거리(24,000cm, 구 내벽 거리)** 두 곳에 배치한다. 구 지형이 없으므로 지면 가림이 배제된다.
3. 2×2(거리 × 로컬/월드)로 보이는지 확인한다. 판정은 스크린샷 대신 Niagara 디버거의 입자 수·`Position`·`Velocity` 표시와 바운드 표시로 한다.
   - 원점에서도 안 보임 → H1. N-2로 모듈 절제.
   - 원거리에서만 안 보임 → H2~H5. N-2로 모듈 절제.
   - 빈 레벨에서는 다 보임 → 구 월드 맥락(지면 가림·큐 스폰 경로·부착) 문제. N-3로 PIE에서 재현.

### N-2 요인 절제 (실패 칸에서)

한 번에 한 요인씩 끈다: `Origin Offset` 0 / 좌표계 Simulation → `AddVelocity` 모드 교체(InCone) → `PointAttractionForce` → `CurlNoise` → `Drag` → 고정 바운드 여부. 어느 하나로 증상이 사라지면 그 요인의 엔진 모듈 그래프(HLSL/노드)를 읽어 메커니즘을 확인한다.

### N-3 PIE 재현 (N-1이 빈 레벨에서 재현되지 않을 때만)

실제 맵에서 가드 큐 경로로 시험 시스템을 띄우고, 디버거로 같은 항목을 본다. 수동 PIE가 빠르면 사용자에게 요청한다.

### N-4 RenderDoc (H5가 남을 때만)

입자가 정상 위치에 있는데 안 그려질 때만 RenderDoc으로 해당 이미터의 드로우콜 유무와 정점 셰이더 출력 위치를 확인한다. 엔진 수정은 범위 밖이다(GX-008).

## 5. 산출물

1. 원인 확정 문장과 근거(디버거 수치, 절제 표, 엔진 모듈 근거)를 [단위 N 로그](../history/UnitN_Log.md)에 남긴다.
2. [TechDesign_ParrySystem.md](../../TechDesign_ParrySystem.md) §8의 "미확정"을 결론으로 바꾼다.
3. 이미터 규칙을 [JustDodge VisualDesign](../../JustDodge/design/VisualDesign.md) §4에 반영하고, V3b가 월드 스페이스 꼬리를 어떤 조건에서 쓸 수 있는지 적는다.
4. 프로젝트 전체가 지켜야 하는 규칙이면 `Decisions.md`에 GX 결정 한 줄을 추가한다.
5. `Roadmap.md`의 단위 N 상태와 `Current.md`를 갱신한다.

## 6. 완료 조건

- [x] N-1 2×2 결과와 판정 근거(입자 수·위치)를 기록했다 — 스크린샷 대신 `bDumpDebugEmitterInfo` 입자 덤프
- [x] 증상을 없애는 최소 요인을 절제로 하나로 좁혔다 — `PointAttractionForce` 하나
- [x] 메커니즘을 엔진 모듈·소스로 설명했다 — 모듈 출력(`PointOriginVec`·길이)과 가속도/거리 ≈ 980 검산. 모듈 그래프 내부는 직접 읽지 않았다
- [x] 이미터 규칙을 ParrySystem §8·JustDodge VisualDesign §4에 반영했다(+ Decisions GX-011)
- [x] 시험 에셋을 정리했다 — `Content/Dev/UnitN/`(시스템 7개·`L_UnitN`)을 외부 참조 0건 확인 후 삭제(2026-11-10). 가드·패링 원본은 수정하지 않았고, `NS_Impact_Generic`·`NS_Explosion`은 사용자 요청으로 교체해 `28bd2a0`에 커밋했다
- [x] V3b 게이트 해제 여부를 Current·Roadmap에 적었다 — 해제

## 7. 범위 밖

- 엔진 소스 수정(GX-008)
- 기존 가드·패링·JustDodge 이미터의 화질 재조정. 규칙이 바뀌어도 재작업은 V3b에서 한다
- 그래플 앵커 Niagara 성능
