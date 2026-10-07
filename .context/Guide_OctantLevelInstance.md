# Octant Level Instance 제작 가이드

Octant Level Instance(LVI) 신규 제작 절차. 옥탄트는 **지각 + 구조물·유기 메시 + 마커**로 구성되며, 완성본을 베이크해 `OctantPoolData`에 등록하면 런타임에 시드 기반으로 8개가 선택·회전 배치된다.

> 런타임 스폰 흐름: [TechDesign_WorldGeneration.md](TechDesign_WorldGeneration.md)
> 지형 의미 태그·profile·경계 규칙: [SurfaceSupportNavigation/design/TerrainContract.md](SurfaceSupportNavigation/design/TerrainContract.md)

---

## 1. 제작 경로

2026-10-06(Surface Navigation Phase 9)부터 옥탄트는 **코드 정의 빌더**로 만든다. LVI를 손으로 편집하지 않고 명령 한 번으로 다시 만들기 때문에 재베이크·이음매 호환이 결정론적이다.

| 구성 | 도구 | 비고 |
|:---|:---|:---|
| 지각 | `LNP.SurfaceNav.BuildSliceOctant` | Meadow 지각의 이음매 밴드를 보존하고 내부만 옥탄트 고유 지형으로 바꾼다 |
| 건물·탑·계단·벽 | 같은 빌더(C++, `LNPSliceOctant.cpp`의 옥탄트 spec) | 구면 바닥, D-070 경사로 충돌 + Decoration 계단 칸 |
| 바위·절벽·동굴 외피 같은 유기 형태 | Blender `Scripts/Blender/GenerateSliceOctant.py` → FBX 임포트 | 빌더가 `Authored` 폴더의 메시를 접미사 규약으로 배치 |
| 동적 요소·앵커·Pod 위치 | 빌더가 Placement Marker·`ALNPMassSpawnPoint` 배치 | 동적 요소는 LVI에 직접 두지 않는다(D-026). 그래플 앵커 거리는 마커마다 `GrappleMaxDistance`로 정한다. 지형을 앵커 거리에 맞추지 않는다 |

> 옛 `BP_OctantGenerator`(Geometry Script 지각 + PCG 프랍 BP) 경로는 `Meadow_00`을 만든 방식이다. 새 지각을 이 경로로 시드만 바꿔 만들면 이음매가 달라져 Meadow와 섞일 수 없다(아래 §3).

---

## 2. 폴더와 이름

```
/Game/Maps/<Name>/
  LVI_Octant_<Name>             빌더가 생성·재생성
  DA_OctantSurface_<Name>       BakeOctant 산출물
  SM_Octant_<Name>_R30000       파생 지각(빌더가 제자리 갱신)
  Generated/SM_<Name>_<Id>_Floor|Shell   빌더가 만든 구조물 메시
  Authored/SM_<Name>_<Id>_<Role>         Blender에서 임포트한 메시
```

`Authored` 메시의 역할 접미사:

| 접미사 | Component Tag | Profile |
|:---|:---|:---|
| `_Floor` | `Support`·`Blocker`·`Static` | `LNPStaticTerrain` |
| `_Coarse` | 위 + `CoarseSupport`(D-072, 넓은 공동 바닥) | `LNPStaticTerrain` |
| `_Shell` | `Blocker`·`Static` | `LNPStaticBlocker` |
| `_Deco` | `Decoration` | `LNPDecoration` |

접미사가 없으면 빌더가 오류로 보고하고 배치하지 않는다.

---

## 3. 지각 — 이음매 보존

런타임은 이웃 slot의 이음매 샘플 양자화 반지름과 Valid 비트가 **비트 단위로 같아야** snapshot을 게시한다(`LNPSurfaceDataSubsystem` `ValidateSeams`). 그래서 새 지각은 Meadow 지각(`SM_Octant_Meadow_00_R30000`)을 복제해 만든다.

- 이음매 평면에서 `SeamMargin`(기본 15m) 안쪽 정점은 좌표를 바꾸지 않는다.
- 그 바깥 `SeamBand`(기본 40m)에 걸쳐 smoothstep으로 옥탄트 목표 지형으로 넘어간다.
- 목표 지형은 시드 fBm 노이즈(`Amplitude`·`Wavelength`)이고, 구조물 자리는 `Pad`(지정 반지름의 구면 원판)로 평탄화한다.
- 정점을 반지름 방향으로만 옮기므로 지각 overhang이 생기지 않는다.

검증: 베이크한 `DA_OctantSurface_<Name>`의 seam hash 세 개가 Meadow(`eb7e9631…`)와 같아야 한다.

---

## 4. Blender 유기 메시

1. Blender MCP에서 `exec(open(r"<repo>/Scripts/Blender/GenerateSliceOctant.py", encoding="utf-8").read())` 후 `generate("<Name>", r"<repo>/Saved/SliceOctant/<Name>")`.
2. 정점은 **옥탄트 로컬 좌표 그대로**(피벗 = 옥탄트 원점)다. UE (x, y, z)cm → Blender (x, −y, z)/100 m이며 FBX 임포트가 되돌린다.
3. 메시는 닫힌 형태로 만들고, 면 법선과 그 지점 Up(구 중심 방향)의 내적이 0.71 이상이면 `_Floor`, 아니면 `_Shell`로 나눠 내보낸다.
4. UE에서 `StaticMeshTools.import_file`로 `/Game/Maps/<Name>/Authored`에 임포트한다. 같은 이름 재임포트는 거부되므로 기존 에셋을 지운 뒤 임포트한다.
5. 각 메시의 `BodySetup_0`에 `collisionTraceFlag = CTF_UseComplexAsSimple`을 설정하고 저장한다. 빌더가 complex-as-simple이 아니면 오류로 보고한다.

---

## 5. 생성·베이크·등록

1. 에디터에서 다른 맵(예: TestMap03)을 연 상태로 `LNP.SurfaceNav.BuildSliceOctant <Name>`. 열린 LVI는 다시 만들지 않는다. 로그 끝의 `lint=`가 0이어야 한다(§6). 생성 없이 검사만 하려면 `LNP.SurfaceNav.LintSliceOctant <Name>`.
2. `LNP.SurfaceNav.BakeOctant /Game/Maps/<Name>/LVI_Octant_<Name>` — 로그의 Layer·portal·Valid 수와 오류를 확인한다.
3. `OctantPoolData`의 `OctantDefinitions`에 `LevelAsset`·`SurfaceData`·`AllowedSlotRotations=255`·`SeamSignature`를 추가하고 저장한다. slot 제한을 두려면 D-043 제약 할당이 먼저다.
4. PIE에서 `LNPSurfaceDataSubsystem: Published generation … validated slots`와 `maxSeamRadiusDelta=0.000cm`를 확인한다.
5. 어느 slot에 놓였는지는 `LNPOctantSpawnSubsystem: slot N (rotation …) <- LVI_…` 로그로 본다. 검토 이동은 서버 콘솔에서 한다.
   - `LNP.Debug.TeleportToReview [Name] [Slot]`: spec의 검토 지점(빌더가 에디터 전용 `ATargetPoint`로 둔다)으로 이동. 인자가 없으면 이름과 slot 목록을 출력한다.
   - `LNP.Debug.TeleportToOctant <Slot> <X> <Y> [H]`: 옥탄트 로컬 좌표로 이동.
   - `LNP.Debug.WhereAmI`: 각 플레이어의 slot·LVI·로컬 (X, Y, H)·극좌표·발밑 메시를 출력한다. 플레이 중 문제 지점을 알릴 때 쓴다.
6. Pod 앵커는 주변 250cm 안 샘플이 모두 walkable·exact 불필요여야 한다. 좁은 바위 꼭대기에 두면 베이크가 `clearance … requires 250cm`로 막는다.

---

## 6. 플레이어 치수표와 레이아웃 규칙

빌더 spec은 아래 치수로 검사한다(`LNPSliceOctant.cpp`의 `PlayerMetrics`·`DesignRules`). 플레이어 값은 `BP_LNPPlayer` 캡슐과 Mover `CommonLegacyMovementSettings`를 PIE에서 읽었다(2026-10-07). 값이 바뀌면 코드와 이 표를 함께 고친다.

| 플레이어 치수 | 값 |
|:---|:---|
| 캡슐 반지름·높이 | 42cm · 192cm |
| 턱 오르기 | 40cm |
| 걷기 가능 경사 | 44.8°(cos 0.71) |
| 점프 높이 | 140cm(초속 750cm/s, 중력 2,000cm/s²) |

| 규칙 | 값 | 근거 | 검사 |
|:---|:---|:---|:---|
| 벽 두께 | 실제 100cm 닫힌 판 | 두께 0인 양면 벽은 붙어 걷다 뚫고 빈 구조물 속에 갇힌다(Chaos trimesh) | 생성 시 두께 0 벽 수(`ZeroThickness`), 캡슐 지름 이상(컴파일 검사) |
| 경사로 | 30°, 꼭대기에 경사로 폭 정사각형 층계참 | 층계참이 없으면 올라온 직후 불편하다 | 빌더가 늘 만든다, 걷기 경사 미만(컴파일 검사) |
| 경사로 옆 통로 | 300cm 이상 | 200cm는 "겨우 지나간다", 800cm는 "적절하다" | `Walkway` |
| 다리 기울기 | 35° 이하 | 걷기 한계보다 넉넉히 | `BridgeSlope` |
| 다리 끝 | 경사로·층계참 위로 내리지 않는다 | 경사로 몸체와 겹친다 | `BridgeOnStair` |
| 위아래 구조물 틈 | 400cm 이상(겹치면 음수) | 캡슐 높이 + 점프 높이 | `Clearance`(건물·섬·성벽 띠) |
| 지면 구조물 위치 | 이음매 전이 띠 밖, 평탄화 Pad 안 | 띠 안 지면은 Meadow 높이로 휘어 밑동이 뜨거나 묻힌다 | `SeamBand`·`OffPad` |
| 문 높이 | 250cm 이상 | 캡슐 높이 + 여유 | `DoorHeight` |

경고는 `[SliceOctant] lint <Name> <규칙>: … at local (X, Y, H) r= deg=` 한 줄씩 나온다.

