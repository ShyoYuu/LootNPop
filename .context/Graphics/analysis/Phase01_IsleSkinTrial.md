# Phase 1-BR 부유섬·하늘 석교 외피 시험(IsleSkin)

> 상태: V007(석교 B안 + 모노리스 A안) 외관 기준 · GX-039·GX-040 / Lit 미리보기 완료(§9) · 충돌 측정 3a(§10)·Shell 파생 3b(§11, GX-041)·베이크 3c-1(§12, IsleSkin_V009)·1인 PIE 3c-2와 복원 3c-3(§13, 14지점 통과) 완료 · 섬 윗면 단위 2(GX-042·043) **전체 완료**: 2a 덮개 재분할(V010) · 2b 엄폐 각기둥·IsleRock·잔돌 충돌(V016, 베이크·1인 PIE 통과) · 2c 잔돌·식생(**IsleSkin_V018** 채택, NoCollision Static ISM, 재베이크 불필요)
> 기준 기록: `Scripts/Profiling/Graphics/IsleSkin_V001/Acceptance_V007.json` · `IsleTop_V016.json` · `IsleTop_V018.json`
> 갱신: 2026-11-28
> 진입: [Current](../Current.md) · [GX-036~038](../Decisions.md) · [V008 채택](Phase01_U5aBoundaryReview.md)

## 1. 범위와 원칙

V008 검토 뒤 사용자는 부유섬 2개와 하늘 석교 외피부터 진행하도록 지시했다. 동시에 판타지 부유 표현의 쓰임새를 GX-038로 한정했다. 부유 표현은 기존 지형의 재해석·보강과 중력 규칙을 엄격하게 지키지 않기 위한 장치로만 쓰고, 기존 지형과 관계없는 빈 공간에 새 부유 지형을 띄우지 않는다.

- 원본은 채택 V008(`U5aBody_V008`)이다. 새 시험 맵은 `EDTerrain_V001/IsleSkin_<판>/LVI_Ridge_EDTerrain_IsleSkin_<판>`으로 복제해서 만들었다. V008 맵과 공유 에셋은 바꾸지 않았다.
- 부유섬·석교·동굴의 형상과 충돌은 V012 보존 구조물 메시 `SM_Ridge_00_V012_PreservedRocks_Floor`/`_Shell` 한 쌍이 맡는다. 이 메시가 보라색으로 보인 원인은 하상 재질 `MI_Ridge_Z1_Channel`의 구역 마스크 밖 평지색이었다.
- 새 외피는 모두 Static/NoCollision이고 태그는 `Decoration`·`LNPDecoration`·`IsleSkinTrial`이다. 보존 구조물의 충돌·메시 에셋은 그대로이며, 시험 맵의 컴포넌트 재질만 바꿨다.

## 2. 판별 경과

| 판 | 구성 | 결과 |
|:---|:---|:---|
| V001 | 보존 구조물 재질 → 지각 Surface V003. 섬 윗면 테두리·옆면·끝 덩어리에 ED 절벽 껍질 82개, 석교 용골·첨탑·파편 14개 | **석교는 방향 채택 후보.** 섬은 판자처럼 붙은 껍질과 V011 밑면 17m 평면이 드러나 기각했다 |
| V002 | 섬 껍질을 Blender 닫힌 몸체(뒤집힌 뿔 + 갈래)와 부유석으로 교체. 석교 회색 NordicBoulder → 사암 | 실루엣은 성립했다. 갈래 사이로 V011 밑면이 비쳤고, 저폴리 면과 밝은 CapRock 재질이 밋밋했다 |
| V003 | 몸체 240각·지층 띠·잔요철, 갈래는 별도 닫힌 원뿔 6개, 몸체 재질을 지각 재질로 변경 | 골짜기 섬은 양호했다. 서쪽 섬은 이음매 띠(경계 55m) 안이라 구역 판정 때문에 절반이 보라색이었다 |
| V004 | V003 형상 그대로. 구역 판정만 1로 둔 내부 구조물용 재질 `IsleSkin_V001/Surface_V001/MI_Ridge_IsleSurface`를 몸체와 보존 구조물에 적용 | 보라색이 사라졌다. 사용자 판정: 섬 밑 갈래·석교 아래 Cliff02 파편이 어색하고, 석교가 반듯한 직육면체다 |
| V005 | 갈래 제거·밑면을 둥글게 닫음(부유석은 V003과 같음). 석교 Cliff02 첨탑·파편 제거. 석교 자연 형상 몸체 **A안**(보호 띠 유지) | 상판 아래 4.5m 수직 띠가 남아 상자 위에 바위가 얹힌 계단처럼 보인다 |
| **V006** | V005와 같고 석교 몸체만 **B안**(석교 자신의 외피가 옆 띠까지 부풂) | **사용자 선택(GX-039).** 석교가 양 끝이 두껍고 가운데가 얇은 하나의 바위 아치로 읽힌다 |
| **V007** | V006 사본 + 산괴 동쪽 탑(M1) 모노리스 외피(§7) | **사용자 채택(GX-040), 현재 기준.** 갈라진 첨탑과 그 위에 뜬 각진 판이 읽힌다 |

V001~V003 맵과 반입 에셋은 비교 이력으로 남겼다(지워도 무방하지만 지우지 않았다).

## 3. V004 구성(V005·V006 변경은 §3.1)

| 구분 | 내용 |
|:---|:---|
| 섬 몸체(2) | `SM_Ridge_IsleBody_<섬>_V003`, 각 8,160삼각형. 윗면 테두리는 외곽선과 같고 덮개는 V011 윗면보다 3cm 아래라 기존 보행면이 보인다. 아래로 좁아져 윗면 아래 약 26.5m에서 닫힌다. 모든 깊이에서 V011 뿔대(+3%)를 감싼다 |
| 갈래(2) | `SM_Ridge_IsleSpikes_<섬>_V003`, 섬마다 닫힌 원뿔 6개(1,152삼각형). 몸체 안 20.5m에서 시작하며 끝은 33.5m보다 얕다 |
| 중간 단계 부유석(2) | `SM_Ridge_IsleFloat_<섬>_V003`, 섬마다 바위 5개(3.5~7.5m)와 파편 4개(0.9~2m). 섬 중심에서 38~47m, 윗면 아래 4~21m에 둔다. 새 빈 공간 장식이 아니라 기존 섬의 보강이다(GX-038) |
| 석교(14) | 사암 Cliff07/08/09 용골 9개(상판 4.7m 아래에 열린 면을 위로 매닮), Cliff02 첨탑 2개, 파편 3개 |
| 재질 | 몸체·보존 구조물: `MI_Ridge_IsleSurface`(Surface V003 복제, 구역 Custom 노드만 `float2(1, 경사)`) |

### 3.1. V005·V006 변경

- 섬: 갈래 원뿔(`IsleSpikes`)을 뺐다. 몸체 밑면은 윗면 아래 약 27m에서 둥글게 닫힌다(`IsleBody_<섬>_V005`, 8,640삼각형). 부유석은 갈래 난수 소비를 유지해 V003과 같은 자리다.
- 석교: Cliff02 첨탑 2개·파편 3개를 뺐다. 사암 용골 9개는 남겼지만 새 몸체 안에 대부분 묻힌다. 새 몸체 `SkyBridgeBody<A|B>_V005`가 V011 판을 감싼다. 윗면은 상판 3cm 아래이고, 가장자리만 6cm 덮어 반듯한 모서리를 지운다. 두께는 양 끝 15m·가운데 8m이고 반폭은 양 끝 7.6m·가운데 5.6m다. 밑면은 불규칙하게 처진 아치다.
- **보호 공간 SkyBridgeDeck(상판 아래 4.5m·+1m, 반폭 3.5m)과 충돌한다.**
  - A안은 상판 아래 4.6m까지 옆면을 반폭 3.48m 안에 묶어 **새 점유 0**이다.
  - B안은 옆면을 바깥으로 부풀려 이 띠의 표본 **41개**(모두 상판 4.5m 아래 높이, 보행 공간 +1m 표본은 0)를 차지한다. 숨기지 않고 `IsleSkin_V006.json`의 `review_hits`에 기록했다.
  - V011 판 안에 원래 있던 표본 20~21개는 따로 분류했다.
  - 사용자가 B안을 골랐다(GX-039). 이 41개는 이후 검사에서도 지우지 않고 "석교 자신의 외피"로 따로 센다. 보행 +1m 표본과 다른 소유자에 대한 보호는 그대로다.

재현 원본(Git)은 다음과 같다.

- 생성: `Scripts/Blender/BuildRidgeIsleBody.py -- build V005`(섬 몸체·부유석·석교 A/B + BVH 보호 검사, V004는 `build V003`), `Scripts/Profiling/Graphics/GenerateRidgeIsleSkin.py V005|V006`(석교 용골·OBB 보호 검사·배치 JSON)
- 반입: `BuildRidgeIsleSkin.py material V001`(한 번) → `build V005|V006` → 다음 Cmd `verify V005|V006`
- 배치 JSON: `IsleSkin_V001/IsleSkin_V00N.json`. 메시 경계·열린 방향 실측은 `AnalyzeRidgeEDMeshes.py` → `IsleSkin_V001/MeshAnalysis.json`
- 근거: `Saved/Profiling/Graphics/Phase01/IsleSkin_V001/`(Geometry_V002~V005 Build.json, Check_V00N, Verify_V00N, Material_V001)

## 4. 실제 검사

- **보호 공간:** 기존 51공간·16,552표본을 같은 표본화로 검사했다.
  - 석교 14개는 메시 경계 OBB 기준 0이다(사선 표본 여유 150cm).
  - 섬 몸체·갈래·부유석은 BVH 안쪽 판정에서 **새 점유가 0**이다.
  - 기존 V011 섬 몸체 안에 원래 있던 표본 2개(서쪽 섬 20m 중심, 골짜기 섬 23m 중심)는 새 몸체도 포함한다. 이는 새로 차지한 자리가 아니라서 따로 분류했다. 보호 공간이나 표본을 빼서 통과시키지 않았다.
- **위상:** 6개 메시 모두 열린 변 0·비다양체 변 0이다.
- **UE 반입:** `Verify_V004`는 PASS다. 배치 14개 위치·축 오차 0, 몸체 6개는 원점·NoCollision이고 경계 오차는 0.0034cm다. 보존 구조물 2개는 재질만 바뀌었고 QueryAndPhysics를 유지한다. dirty는 없고 PIE=false다.
- **보존:** `FrozenInputs_V008` 696파일과 `AcceptedHandoffHashes_V008` 25파일의 해시가 모두 같다(V004·V006 뒤 두 번 확인). V008 맵은 바뀌지 않았다.
- **V005·V006:** 두 맵 모두 `Verify` PASS다. 배치 9개 오차 0, 몸체 5개 경계 오차 0.0033cm, dirty 없음. 섬·부유석 새 점유 0, 위상 결함 0. 석교 보호 띠는 §3.1에 정리했다.

## 5. 남은 REVIEW와 다음 판단

- **시각과 충돌이 다르다.** 몸체 옆면은 윗면 아래 약 1.5~6m에서 외곽선보다 최대 약 10%(약 3m) 바깥으로 나온다(테두리 돌출 1.5% × 요철·지층 상한 계산값). 부유석·갈래·석교 용골은 보이지만 충돌이 없다. 섬 윗면 보행면과 석교 상판 충돌은 기존 그대로라서 걷는 면은 맞는다. 몸체·부유석의 Shell 파생은 U5b 충돌 단계에서 GX-031·032 계약(Floor 50㎡·Nav Layer 256)으로 다룬다.
- **아직 없는 것:** 섬 윗면 장식(잔돌·식생)과 엄폐 각기둥의 ED 외피가 없다. 엄폐 외피는 V001에서 각기둥 자리가 섬 위 보호 표본과 겹쳐 빠졌다. 이는 U7 세부 마감이나 정확한 메시 검사와 함께 다룬다. **정정(§14.1):** 각기둥 자체가 U3 V012 보존 구조물에서 빠져 있었고, V002 이후에는 섬 껍질 생성 전체가 꺼져 남은 2개도 사라졌다.
- **확인하지 않은 범위:** Lit·Lumen 판정(이번 화면은 Unlit), 패키지 성능, PIE는 하지 않았다. 갈래·부유석이 그래플 사선 시야를 시각적으로 가리는지는 표본 검사만 했다.
- **석교 상판 자체**(보행면)는 V011 판의 곧은 직선 통로 그대로다. 통로 선형·폭을 바꾸면 기존 경로·보호 폭 변경이므로 이번에는 바꾸지 않았다.
- **다음 판단:** 모노리스 재해석은 V007로 채택했다(§6~7). 남은 단위는 §8이다.

## 6. 모노리스 재해석 대상 특정(2026-11-27)

V006 맵에서 같은 시점의 화면 차이(지각·보존 구조물을 에디터에서 임시로 숨김)와 광선 판정으로 세 후보를 특정했다. 근거는 `Saved/Profiling/Graphics/Phase01/Monolith_V001/`(Overlay_*.png·Pick_*.json·Height_*.json)이고 도구는 `Scripts/Profiling/Graphics/InspectRidgeMonolithTargets.py`(state·open·pick·describe·heightmap·dump·hide, 읽기 전용)다.

| 후보 | 실체 |
|:---|:---|
| Z1 부감의 지각 재질 각기둥 = 석교 옆 높은 덩어리(**M1**) | 동굴 보존 구조물(`PreservedRocks_Shell`) 북동 모서리 벽(옥탄트 로컬 x -7,700~-6,900 · y -3,700~-1,500, 꼭대기 52~56m)과 그 동쪽 렌더링 지각(`Z1MassifCover_V012`) 절벽(56m → 고원 7m). 서쪽 착지면(38m)에서는 약 19m, 동쪽 고원에서는 약 50m 높이다 |
| WestStairEye 보라색 각기둥(**M2**) | 같은 보존 구조물의 MassifCavern 덩어리. 윗면 약 38m가 StairRamp2→3 착지면(Floor)이고 안쪽이 동굴이다 |

- 보라색 원인은 이음매 띠가 아니다(맞은 점의 이음매 거리 약 81m > 55m). V008에서 이 Shell이 하상 재질 `MI_Ridge_Z1_Channel`을 써 구역 마스크 밖 평지색이 칠해진 것이고, V004 이후 `MI_Ridge_IsleSurface`로 이미 사라졌다.
- 원래 왕관 정상(CrownSummit) 자리는 현재 렌더링 지각에서 약 10m 고원이라 따로 솟은 "왕관 탑"은 없다. 사용자가 말한 크라운 탑은 M1이다.
- 보존 구조물은 Floor(걷는 윗면)·Shell(가파른 면) 한 쌍이 함께 닫힌 형상이다. 이 메시 한 쌍에 석교·부유섬 2개·대전당·산괴 동굴 5곳이 모두 들어 있다.

## 7. V007: M1 모노리스 외피(A안, GX-040)

사용자는 지각·충돌 절개 없이 시각 외피만으로 하는 A안을 골랐다. M2 서쪽 면은 손대지 않기로 했다.

| 몸체(`Bodies_V007/`) | 내용 |
|:---|:---|
| `SM_Ridge_MonolithShroud_V002` (12,736삼각형) | 지각+보존 구조물 높이장을 높이별 반경(서쪽 0.45m → 동쪽 1.7m, 층 띠 2.7m 주기)으로 넓힌 두꺼운 닫힌 외피. 꼭대기를 V자 균열(y = -2650 + 0.15(x+7300))로 갈라 남 조각 +6m·북 조각 +11m(최고 68.6m)로 세웠다. 평지·착지면에서는 기존 면 30cm 아래로 묻힌다 |
| `SM_Ridge_MonolithCap_V002` | 북 조각 위 3.5m(아래면 72.1m)·남 조각 위 2.6m에 뜬 모서리 깎은 각진 판 2장 |
| `SM_Ridge_MonolithSlabs_V002` | 동쪽 면에서 2~2.2m 떨어져 뜬 석판 2장(높이 33m·43m). Unlit에서는 벽과 섞여 잘 안 보인다 |
| `SM_Ridge_MonolithChips_V002` | 북 판 아래 틈 둘레의 둥근 파편 3개 |

- 생성: `Scripts/Blender/BuildRidgeMonolithSkin.py -- build V002`(입력 `Monolith_V001/Dump_M1_V002.json` = 에디터 `InspectRidgeMonolithTargets.py dump`). V001(변화가 약하고 둥근 판이 알처럼 보임)은 기각 이력이다.
- 배치: `GenerateRidgeMonolithSkin.py V007 V002` → `IsleSkin_V007.json`(V006 배치·몸체 그대로 + 몸체 4개, 원본 맵은 V006) → UE `BuildRidgeIsleSkin.py build V007` → 다음 Cmd `verify V007`. `GenerateRidgeIsleSkin.py`는 V006 Acceptance 보존 대상이라 고치지 않았다.
- 보호 51공간·16,552표본: 새 점유 0, 기존 형상 안 표본 0. StairRamp2~4 중앙 통로 띠(반폭+1.1m) 안은 외피를 묻었다. 기존 StairRamp3 31표본 REVIEW는 다른 몸체 소관이라 그대로다. 기존 형상 판정은 보존 구조물 Floor+Shell+지각에 대한 현지 Up 광선 홀짝이다.
- LandmarkTop 사선(WestEntry·CrownSummit·DeepBasin) 3개 모두 가리지 않는다(최소 거리 13.3m). 위상 결함 0.
- Verify V007 PASS(배치 오차 0, 몸체 8개 경계 오차 0.0033cm, dirty 없음, PIE=false). 보존 해시 `CheckRidgePreservation.py`: V008 721항목(중복 제거 703쌍)·V006 Acceptance 7·V007 Acceptance 14파일 불일치 0.
- 화면: `Saved/Profiling/Graphics/Phase01/Monolith_V001/Shots/`(V007_Unlit_*, 선택 윤곽 V007_Selected_*). V006 같은 시점은 `Monolith_V001/V006_*_Cur.png`다.

## 8. 남은 것과 다음 단위

- **Lit 비교:** 완료(§9). IsleSkin LVI 맵 자체에는 조명이 없으므로 Lit 판정은 `IsleSkinLit_V001` 미리보기 레벨에서 한다.
- **스카이라인:** 북쪽 부유 판 꼭대기가 약 81m로 기존 56m보다 높다. 사용자가 이 상태로 유지를 결정했다.
- **충돌:** 외피·부유 판은 NoCollision이다. 탑 꼭대기와 판은 착지면(38m)에서 닿지 않는 높이지만 그래플 접근은 확인하지 않았다. Shell 파생은 U5b(GX-031·032)에서 다룬다.
- **다음 단위 후보:** 사용자가 1(Lit 미리보기) → 3(충돌 Shell 파생) → 2(섬 윗면·엄폐 각기둥 외피) 순서로 1을 승인했다. 1은 §9에서 끝났다. 3은 3a 측정 → 3b Shell 파생·반입 → 3c PIE 접근 확인으로 나눴고 3a는 §10, 3b는 §11, 3c-1(베이크·검사용 풀)은 §12, 3c-2(1인 PIE)·3c-3(복원)은 §13에서 끝났다. 2는 사용자 확인 뒤 진행한다.

## 9. Lit 미리보기(2026-11-27)

| 항목 | 내용 |
|:---|:---|
| 레벨 | `EDTerrain_V001/IsleSkinLit_V001/IsleSkinLit_V006`·`_V007` · TestMap03 조명 사본 6개 + 시험 전용 회색 Sky Light 50(GX-025, TestMap03 미반영) + IsleSkin LVI Level Instance 하나(원점, LVI는 월드 좌표 저작) |
| 재현 | `Z1AssemblyTest_V001`을 연 다음 Cmd에서 `BuildRidgeIsleSkinPreview.py build <V006\|V007>` → `verify <판>`(결과 `Saved/.../IsleSkin_V001/Preview/Build_*·Verify_*.json`). `view <시점>`·`sky <0\|1>`(Sky Light 임시 숨김)은 확인용이다 |
| 시점 | `Scripts/Profiling/Graphics/IsleSkin_V001/PreviewViews_V001.json`(Z1Overview·EastHigh·WestStairEye·LandingEye·NorthEast, 월드 cm, FOV 90) |
| 촬영 | `EditorAppToolset.CaptureViewport`(1127×724)를 시점 좌표로 호출한다. 레벨을 연 직후 첫 장은 버리고 Lit은 같은 시점을 두 번 이상 찍어 마지막 장을 쓴다. 보기 모드는 **뷰포트 툴바 콤보**에서 바꾼다(Cmd `viewmode`는 레벨 뷰포트에 적용되지 않았다). VRAM 초과 화면 경고는 촬영 중 `DisableAllScreenMessages`로 끄고 끝나면 되돌린다 |
| 결과 | `Saved/.../IsleSkin_V001/Preview/Shots/<판>_<Unlit\|Lit>_<시점>.png`, 2×2 비교 시트 `Preview/Compare/<시점>.png` |
| 검사 | 두 레벨 Verify PASS. 작업 전후 보존 해시 불일치 0. 원본 맵·공유 에셋 변경 없음 |

- 모노리스의 V자 균열·층 띠·면 석판은 Lit에서 Unlit보다 훨씬 잘 읽힌다. Unlit에서 벽과 섞이던 동쪽 면 석판이 Lit에서는 돌출 선반으로 보인다.
- **사용자 판정 대상:** NorthEast에서 층 띠 돌출이 첨탑 면에 얇은 판처럼 여러 장 붙어 보이고, LandingEye에서 꼭대기 윤곽이 각진 계단처럼 보인다.
- 석교 B 아치는 Lit에서도 양 끝이 두껍고 밑면이 처진 하나의 바위로 읽힌다. 섬 밑면과 부유석은 그늘이 짙다(구 안쪽 광원 + Sky Light 50).
- 구조물에 보라색은 없다. 먼 보라색은 다른 옥탄트 이음매 띠로 GX-036 범위 밖이다.
- 확인하지 않은 범위: 미리보기 레벨은 VRAM 예산을 약 490MB 넘겨 텍스처 스트리밍 품질이 떨어질 수 있다. Sky Light 없는 TestMap03 그대로의 Lit은 찍지 않았다(`sky 0`으로 확인할 수 있다). 패키지 성능·PIE는 범위 밖이다.

## 10. 충돌 측정 3a(2026-11-27, 읽기 전용)

외피 몸체 9개를 V007 맵에 실제로 있는 충돌 5개와 비교했다. 맵·에셋은 바꾸지 않았다.

| 항목 | 내용 |
|:---|:---|
| 기존 충돌 | 충돌 지각 `Z1MassifCover_V013`(숨김)·`Z1Collision_V003` Floor/Shell(숨김, 예전 V037 배치에서 구움)·`PreservedRocks_Floor/_Shell`(보임). 외피 9개는 모두 NoCollision이다 |
| 재현 | IsleSkinLit_V007(또는 IsleSkin_V007)을 연 다음 Cmd `InspectRidgeIsleSkinCollision.py dump V007`(몸체 묶음 경계 + 30m 상자와 겹치는 충돌 삼각형 33.8만 개, float32) → `blender --background --factory-startup --python Scripts/Blender/MeasureRidgeIsleSkinCollision.py -- V007`(약 12초). `census`는 충돌 컴포넌트 목록이다 |
| 판정 | 몸체 삼각형 중심마다 기존 충돌까지 최근접 거리 d와 기존 형상 안/밖(현지 Up 광선 홀짝, §7과 같은 규약). 맞음 d ≤ 25cm(GX-031 p95 목표), 노출 = 밖·d > 25cm(보이는데 충돌이 없는 면), 묻힘 = 안·d > 25cm. 위를 보는 면은 법선·Up ≥ 0.71 |
| 결과 | `Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision/Measure_V007.json`(입력 해시 포함)·`Dump_V007.json`·`Census_Current.json` |

| 몸체 | 노출 넓이 | 노출 틈 p50 / p95 / 최대 | 위를 보는 노출 성분(최대) | 이음매 최소 |
|:---|---:|---:|---:|---:|
| 골짜기 섬 몸체 | 3,345㎡ | 1.7 / 3.9 / 4.2m | 1.3㎡ | 57.9m |
| 골짜기 부유석 9개 | 1,200㎡(전부) | 15.4 / 20.1 / 24.3m | **56.2㎡(바위 1개)** | 51.2m |
| 서쪽 섬 몸체 | 2,868㎡ | 1.5 / 3.9 / 4.2m | 0 | 20.3m |
| 서쪽 부유석 9개 | 695㎡(전부) | 14.9 / 20.8 / 21.8m | 28.6㎡ | **12.4m** |
| 석교 B 몸체 | 993㎡ | 1.9 / 4.1 / 6.7m | 10.0㎡ | 101.6m |
| 모노리스 외피 | 3,047㎡ | 1.6 / 6.7 / 11.9m | 9.9㎡ | 85.0m |
| 모노리스 판 2·석판 2·파편 3 | 506㎡(전부) | 판 14~25m, 석판 3.4~5.1m, 파편 15~17m | 13.8㎡ | 78.8m |

- **보이는데 통과되는 면이 크다.** 섬 옆·밑면은 기존 보존 구조물보다 최대 4.2m, 석교 B는 6.7m, 모노리스 첨탑은 11.9m 바깥에 보인다. 부유석·판·석판·파편은 충돌이 전혀 없다. 걷는 윗면(섬 윗면·석교 상판)은 d ≤ 25cm로 맞는다(섬 몸체의 맞는 면 1,228~1,563㎡).
- **50㎡ 이상 위를 보는 면은 골짜기 부유석 1개(56.2㎡, 옥탄트 로컬 약 4,870, -2,384, H3,763)뿐이다.** GX-031·032 규칙대로면 Floor가 되어 Nav Layer가 1개 늘고, 적이 닿을 수 없는 부유석에 Support가 생긴다. 모노리스 첨탑 꼭대기는 79개 성분 모두 10㎡ 미만이다.
- **이음매 20m:** 서쪽 섬 몸체는 20.3m로 겨우 통과한다. 서쪽 부유석 3개(로컬 x 약 -18,150~-18,700, 이음매 12.4·15.3·15.5m)는 후속 충돌 20m 금지에 걸린다(시각 5m는 통과).
- **보호 51공간·16,552표본:** 몸체 안 표본은 이전 기록과 같은 자리다(골짜기 섬 1·서쪽 섬 1·석교 B 62, 경계 위 1표본 추가로 63). 다만 "기존 형상 안" 판정을 V011 해석 형상 대신 실제 충돌로 하면 석교 B의 새 점유는 41 → 38이다(새 점유 → 기존 7개, 기존 → 새 점유 4개. 모두 상판 남쪽 끝 V011/V012 형상 차이 약 67cm 안이다). 골짜기 섬 1표본(섬 중심 H3,200)은 보존 구조물 Shell 표면 위(거리 0)라 홀짝이 밖으로 나왔지만 실제로는 기존 형상 경계다. 다른 몸체의 새 점유는 0이고 StairRamp3 표본도 0이다.
- **사선:** LandmarkTop 3개·그래플 사선(앵커 6개의 from 노드 +170cm → 앵커)·착지점 +100/+170cm 모두 몸체 안 표본 0이다. 가장 가까운 것은 ValleyRise → ValleyIsleHub 2.1m(골짜기 섬 몸체), WestSkyIsle → Terrace3Hub 1.7m(서쪽 섬 몸체), 착지점 0.8~0.9m(섬 윗면)다.
- **참고(이번 범위 밖):** StairRamp3 중앙 통로 192표본 가운데 22개는 현재 기존 충돌 형상 안(20개가 예전 배치의 `Z1Collision_V003` Shell)으로 판정됐다. 외피와 무관한 기존 충돌 소관이며 U5a 몸체 기준 31표본 REVIEW와는 다른 판정이다. U5b에서 다시 본다.
- **확인 못 한 범위:** 틈은 삼각형 중심 표본이라 가는 돌출의 최대값을 조금 낮게 잴 수 있다. 홀짝 판정은 지각 절개 구멍 아래처럼 깊은 자리에서 틀릴 수 있다. 충돌 덤프는 RenderData LOD0라 Z1 Shell 삼각형 수가 기록(459,537)보다 72개 적다. 그래플 사선은 선분 표본이며 캡슐 두께와 실제 그래플 판정은 3c PIE 몫이다.
- 3b에서 정할 것 세 가지는 사용자가 권장안대로 정했다(GX-041): 50㎡ 넘는 부유석 윗면도 Shell로만, 석교 B 충돌은 B 외형대로(보호 띠 점유는 석교 자신의 외피로 따로 셈), 이음매 20m 안 서쪽 부유석 3개는 충돌 없음.

## 11. 충돌 Shell 파생 3b(2026-11-27, GX-041)

> **대체됨:** 이 절의 Shell V001·맵 V008은 Floor 높이 부호 오류로 섬 윗면 Shell이 보행 Floor보다 위에 남았다. 3c-1 베이크에서 발견해 Shell V002·맵 `IsleSkin_V009`로 바꿨다(§12). 아래 수치는 V001 기록이다.

외피 몸체에서 Shell(Blocker) 하나를 파생해 V007을 복제한 `IsleSkin_V008`에 놓았다. 외피·V007·공유 원본은 바꾸지 않았다.

| 항목 | 내용 |
|:---|:---|
| 산출 | `EDTerrain_V001/IsleSkin_V001/Collision_V001/SM_Ridge_IsleSkin_V001_Shell`(8,422삼각형, 단순 도형 0, complex-as-simple, Nanite 끔, 열린 변·비다양체 변 0) · 맵 `IsleSkin_V008/LVI_Ridge_EDTerrain_IsleSkin_V008`(V007 + Shell 액터 1개, 원점·숨김·`LNPStaticBlocker`·Blocker+Static 태그) · 미리보기 `IsleSkinLit_V001/IsleSkinLit_V008` |
| 재현 | Blender `BuildRidgeIsleSkinCollision.py -- build V001`(약 8초, 입력은 3a 덤프 `Dump_V007`) → UE `PlaceRidgeIsleSkinCollision.py import V001` → `build V008 V001` → 다음 Cmd `verify V008 V001`. 미리보기는 `BuildRidgeIsleSkinPreview.py build V008` → `verify V008` |
| 기록 | Git `Scripts/Profiling/Graphics/IsleSkin_V001/Collision_V001.json`(규칙·입력 해시·몸체별 결과) · 로컬 `Saved/.../IsleSkin_V001/Collision/`(Shell_V001/Build.json·Import_V001·Build_V008·Verify_V008·Dump_V008·Measure_V008, `Shots/`·`Compare/<시점>.png`) |

- **규칙:** 외피 몸체의 연결 덩어리마다 이음매 20m 안(서쪽 부유석 3개)과 가장 긴 축 100cm 미만은 뺐다(GX-031). 몸체마다 Decimate 비율을 낮은 쪽부터 시험해 외피 ↔ 프록시 양방향 p95 ≤ 25cm·최대 ≤ 75cm·위상 결함 0인 첫 비율을 골랐다(석교 0.3, 나머지 0.2). 맞춤 p95는 4.6~21.2cm, 최대 9.8~65.8cm다(부유석이 가장 크다).
- **걷는 면 아래로 내리기:** 프록시가 기존 Floor 역할 충돌보다 위 40cm ~ 아래 15cm에 있으면 그 면 5cm 아래로 내린다. 걷는 면 위 Blocker 턱은 모든 몸체에서 0㎡다. 최대 내림은 섬 8cm, 석교 17cm, 모노리스 42cm다.
  - 시행착오: 처음에는 정점만 내려 섬 윗면 턱 1,563㎡가 남았다. 단순화한 큰 평면 삼각형의 가운데가 구면을 따라 휜 Floor보다 뜨기 때문이다(현의 처짐). 면 표본 반복으로 내리자 섬 52cm·석교 107cm·모노리스 147cm까지 과하게 내려갔다. 걷는 면 근처 윗면을 3m 변으로 잘게 나누자 석교에 비다양체 변 19개가 생겼다. 최종안은 걷는 면 근처 윗면 정점을 단순화에서 지키고(정점 그룹) 정점마다 자기 높이만큼만 내린다.
- **검사:** 이음매 최소 20.3m(서쪽 섬 몸체). 보호 51공간·16,552표본의 새 점유는 석교 B의 SkyBridgeDeck 37표본뿐이다(GX-041 허용, 상판 위 +1m 표본 0). 다른 새 점유 0, LandmarkTop·그래플 사선·착지점 막힘 0. 위를 보는 50㎡ 이상 성분(섬 윗면 1,564·1,228㎡와 석교 윗면 175㎡는 기존 Floor 5cm 아래로 내린 면, 모노리스 85㎡, 골짜기 부유석 50.0㎡)은 모두 Shell이고 새 Floor는 없다. 따라서 Nav Layer는 늘지 않는다.
- **UE:** 반입 경계 오차 0.003cm. `Verify_V008` PASS(충돌 = 기존 5개 + Shell 1개, Shell 역할 계약·메시 검사 통과, 외피 충돌 0, 디스크 재로드 뒤 확인, dirty 없음, PIE=false). 미리보기 `Verify_V008` PASS.
- **효과(3a 측정을 V008 충돌로 다시 잼, `Measure_V008.json`):** 보이는데 충돌이 없는 면이 섬 몸체 3,345·2,868 → 0㎡, 석교 993 → 0.9㎡, 모노리스 3,047 → 0.1㎡, 판·석판·파편 → 0㎡로 줄었다. 남은 것은 이음매 제외 서쪽 부유석 3개(167㎡, 의도)와 골짜기 부유석 단순화 틈 12㎡(26~34cm)다.
- **화면:** 같은 5시점에서 V007 Lit · V007 Player Collision · V008 Player Collision을 세로로 붙였다(`Collision/Compare/<시점>.png`). 모노리스 자리 충돌이 사각 덩어리에서 갈라진 첨탑·판·석판으로, 섬 옆 부유석이 충돌로 바뀐 것이 보인다. Lit 외관은 Shell이 숨김이라 바뀌지 않는다.
- **찾은 것(이번 범위 밖):** 섬 외피 윗면 원본 삼각형의 가운데가 실제 보행 Floor보다 최대 약 19cm 위에 보인다(정점은 약 3cm 위). PIE에서 섬 윗면에 발이 조금 묻혀 보일 수 있다. 외피 윗면 분할을 늘리면 줄어든다(V007 외관 변경이라 따로 결정).
- **도구 함정:** 보기 모드를 Player Collision으로 바꾼 뒤 다른 레벨을 열면 숨긴 컴포넌트(충돌 지각·Z1 Floor/Shell)의 충돌 표시가 그려지지 않는다. 레벨을 연 뒤 보기 모드를 다시 고르고 찍는다.
- **확인 못 한 범위:** Support 베이크·Nav·PIE 보행/그래플(3c). V008 맵은 아직 베이크 대상 LVI가 아니다. 부유석 Shell은 단순화 틈 최대 67cm가 있어 가장자리에서 시각과 충돌이 조금 어긋날 수 있다.

## 12. 베이크·검사용 풀 3c-1(2026-11-27)

| 항목 | 내용 |
|:---|:---|
| 비교 기준 | `Z1Bake_V010`(지난 Z1 PIE 맵). 최상위 액터를 덤프해 대조했다. 마커 43개(Pod 10·앵커 6·검토점 27)·충돌 5개·Z1 Level Instance 15개의 자세와 충돌 설정이 모두 같고, IsleSkin 쪽은 충돌 없는 시각 메시·U4 조립체 21개와 Shell 하나만 더 있다(`Bake/Top_*.json`) |
| 재현 | Cmd `BakeRidgeIsleSkin.py open <LVI>` → 다음 Cmd `dump <이름>` → `bake <판>`(TestMap03을 열고 `LNP.SurfaceNav.BakeOctant`) → `pool <판>`. 결과 `Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Bake/`(Top·Bake·Candidates·Pool·ShellVsFloor) |
| 산출 | `IsleSkin_V009/DA_OctantSurface_LVI_Ridge_EDTerrain_IsleSkin_V009` · 검사용 풀 `IsleSkin_V009/DA_GraphicsRidgeIsleSkinPool_V009`(정식 풀 복제, Ridge 정의 index 5만 교체, 정식 풀 해시 그대로) |

| 수치 | Z1Bake_V010 | V008(Shell V001, 기각) | **V009(Shell V002)** |
|:---|---:|---:|---:|
| Support Layer | 82 | 82 | **82** |
| 스폰 후보 | 9,360 | 9,165 | **9,360** |
| Nav 셀 / 성분 / 포털 | 78,385 / 119 / 640 | 76,672 / 121 / 638 | **78,343 / 120 / 638** |
| ValleySkyIsle·WestSkyIsle Pod 같은 Layer 후보 | 104 · 95 | 1 · 3 | **104 · 95** |
| Meadow 이음매 | 일치 | 일치 | **일치** |

- **V008 실패 원인:** 3b 스크립트의 Floor 높이 함수 부호가 뒤집혀 섬 윗면 Shell이 보행 Floor보다 중앙값 9~13cm·최대 21cm 위에 남았다(`Bake/ShellVsFloor_V008.json`). 걷는 면 턱 검사도 같은 함수를 써 0으로 잘못 나왔다. 베이커의 스폰·Nav 캡슐(바닥 +1cm부터)이 이 턱에 걸려 섬 위 후보가 거의 사라졌다.
- **Shell V002(`BuildRidgeIsleSkinCollision.py -- build V002`, 기록 `Collision_V002.json`):** 부호를 고치고, 몸체 주변 Floor 역할 삼각형의 정점·중심(고유 점)에서 바로 위 위를 보는 Shell 면이 Floor 위 -2 ~ 40cm면 그 면을 내리고 남으면 실패로 하는 검사를 더했다. 면 표본 단계와 Floor 표본 단계를 번갈아 돌린다. 결과는 Floor 근처 Shell 0·걷는 면 턱 0, 보호 새 점유는 석교 B 37표본(GX-041 허용)뿐, 사선 막힘 0, 이음매 최소 20.3m, 위상 결함 0이다. 최대 내림은 섬 31·27cm, 석교 161cm, 모노리스 168cm다(석교 끝·모노리스 밑동처럼 기존 Floor가 바로 아래 있는 자리라 걷는 면은 Floor가 맡는다).
  - 시행착오: 벽이 바닥과 만나는 선을 턱으로 세던 검사를 위를 보는 Shell 면만 세게 고쳤다. 같은 Floor 점을 여러 삼각형이 공유해 중복으로 세던 것도 고유 점으로 바꿨다.
- **맵:** V007을 복제한 `IsleSkin_V009` + Shell V002(`Collision_V002/SM_Ridge_IsleSkin_V002_Shell`, 8,422삼각형). `Verify_V009` PASS. 3a 재측정(`Measure_V009.json`)의 노출 면은 V008과 같고 모노리스만 0.1 → 5.7㎡(걷는 면 아래로 내린 자리, 틈 37~61cm)다.
- **해석:** Layer·스폰 후보·Pod가 기준과 같으므로 새 Floor 없이 Nav 예산을 지켰다. Nav 셀 42개 감소·성분 1개 증가·포털 2개 감소는 부유석·모노리스 외피 같은 새 Blocker 몫이다. 어느 자리인지는 아직 찾지 않았다.
- 미리보기 `IsleSkinLit_V009`는 만들지 않았다(Shell은 숨김이라 외관은 V007과 같다). 풀 임시 선택·1인 PIE·복원은 §13이다.

## 13. 1인 PIE 3c-2·복원 3c-3(2026-11-27)

| 항목 | 내용 |
|:---|:---|
| 임시 선택 | 원래 값(정식 풀 `/Game/Maps/OctantPoolData`, EnemyDensity 1)·Config 기본 INI 8개·정식 풀·V009 4파일 해시를 `Bake/SelectionState.json`에 먼저 적었다. 검사용 풀 V009를 MCP `ObjectTools.set_properties`로 메모리에서만 골랐고(SaveConfig 없음) EnemyDensity는 0이다. 두 값을 다시 읽어 확인했고 선택 뒤 해시는 같았다 |
| 지점 계산 | V009 충돌 덤프(`Collision/Dump_V009`)에서 옥탄트 로컬 (x, y)의 현지 Up 광선과 만나는 모든 충돌 높이를 구했다. `TeleportToOctant` 프레임과 `RidgeOctantSpec.local`이 같음을 코드로 확인했다(H는 발밑, 명령이 +150cm). 계산은 `Saved/.../IsleSkin_V001/PIE_3c2/`의 `Columns.py`·`Profiles.py`·`ShellParts.py`(Shell 연결 성분)·`MonolithScan.py`(읽기 전용, Blender 동봉 Python+numpy)다. 지점 목록은 `Points.json`이다 |
| PIE | TestMap03 1인. slot 0 = `LVI_Ridge_EDTerrain_IsleSkin_V009`, 검증 slot 8개, 막힌 이음매 노드·변 0, maxSeamRadiusDelta 0.000cm, 적 수 배율 0.00, 오류 0건. 워닝은 기존 Z1 북벽 `ASM_Z1_WallNorth` AttachTo 1건(U4 계획 기록과 같은 기존 경고)·의도한 밀도 0 안내·기존 GameplayCue 경로 안내다 |
| 판정 | **사용자: P01~P14 모두 설명대로 동작** |
| 복원 | 정식 풀·EnemyDensity 1을 다시 읽어 확인. INI 8개·정식 풀·검사용 풀·V009 4파일 해시 동일, 보존 해시 `PIE3c3_After`(V008 703·V006 7·V007 14) 불일치 0, TestMap03·PIE=false·dirty 없음. `SelectionState.json`은 `restored` |

| 지점 | 명령(slot 0) | 확인한 것 |
|:---|:---|:---|
| P01·P02 골짜기 섬 남·서 테두리 | `0 7535 -8454 5500` · `0 4916 -5332 5500` | 외곽선 밖 0.5~1m에서 충돌이 끝나 옆면에 걸리지 않고 약 57m(지각)·72m(Z1 Shell) 아래로 떨어진다 |
| P03 골짜기 큰 부유석 | `0 4900 -2380 4140` | Shell 윗면 H4132(약 30°)에 선다(GX-041 Shell 예외) |
| P04~P06 그래플 | `0 8200 0 -2400` · `0 -14600 4800 6000` · `0 -17600 0 0` | ValleyRise→ValleyIsleHub·WestSkyIsle→Terrace3Hub·WestEntry→EntryToWestIsle가 걸림 없이 착지한다 |
| P07 서쪽 이음매 부유석 | `0 -18160 7468 5300` | 충돌 없이 통과해 지각 H-117에 떨어진다(GX-041 의도) |
| P08·P09 석교 B | `0 -4500 -7400 3845` · `0 -4101 -7421 3710` | 상판 Floor 3845에서 턱이 없다. 가장자리 1m 밖 어깨는 52~54° Shell이라 미끄러져 내려간다 |
| P10 모노리스 38m 착지면 | `0 -7800 -4100 3787` | 밑동을 따라 걸어도 턱·끼임이 없다(외피 Shell은 Floor 30cm 아래) |
| P11·P12 부유 판 | `0 -7400 -2100 8130` · `0 -7300 -3200 6970` | 북 판 H8122·남 판 H6962 위에 선다 |
| P13·P14 동쪽 석판 | `0 -6556 -3134 3810` · `0 -6439 -1931 4710` | 석판 윗면 H3803·H4705에 선다 |

- **사용자 제보(이번 범위 밖):** 외피가 아닌 다른 지형 여러 곳을 캐릭터가 그대로 통과한다. 위치는 기록하지 않았다. V009의 충돌은 충돌 지각 V013·예전 V037 배치에서 구운 `Z1Collision_V003`·보존 구조물·외피 Shell뿐이고, U4 V011 큰 배치 조립체 21개 같은 이후 시각 메시는 NoCollision이다(§12 대조). 이 영역의 충돌은 GX-034의 U5b(구역별 몸체·충돌) → U6(전체 베이크·PIE) 순서로 다룬다.
- **북벽 AttachTo 경고 원인(읽기 전용 조사, `Saved/.../IsleSkin_V001/AttachWarning/`):** `Z1Cull_V001` 15개 레벨의 액터는 `DefaultSceneRoot` → `Scene` → ISM 구조로 저장되어 있고 모두 Movable이다. 생성 스크립트가 지정한 Static이 남지 않았다. 북벽 `ASM_Z1_WallNorth`만 마지막 ISM `InstancedStaticMesh797`(SM_Cliff_08, 인스턴스 1개)이 Static으로 남았고, 로드할 때마다(Python 패키지 로드·베이크·PIE) Movable 부모에 붙지 못해 경고 1건이 난다. 부모와 그 컴포넌트의 상대 변환은 모두 원점·항등이라 떨어져 있어도 화면 위치와 충돌(NoCollision)은 같다. Z1Cull_V001은 V008 FrozenInputs(15파일) 보존 대상이라 제자리에서 고치지 않는다. 없애려면 새 판(Z1Cull_V002)과 그 판을 가리키는 새 LVI가 필요하다. **사용자 결정: U5b에서 함께 정리한다**([1-BR U5b](../phases/Phase01_RidgeReferencePatch.md)). 함께 찾은 것: ISM 컴포넌트 약 7,300개가 Movable이고, 북벽은 798개 가운데 386개가 인스턴스 1개짜리다(트랙 O 렌더 비용 후보, 측정 안 함).
- **남은 결정:** 섬 외피 윗면 삼각형 가운데가 보행 Floor보다 최대 약 19cm 위에 보이는 문제(§11, V007 외관 소관)와 다음 단위 2(섬 윗면 잔돌·식생·엄폐 각기둥 외피).

새 세션 시작 프롬프트는 다음과 같다.

```text
@D:/UnrealProjects/LootNPop/.context/Graphics/README.md
@D:/UnrealProjects/LootNPop/.context/Graphics/Current.md
@D:/UnrealProjects/LootNPop/.context/Graphics/analysis/Phase01_IsleSkinTrial.md

Phase 1-BR 외피 작업을 이어가자. 외관 기준은 IsleSkin_V007(석교 B안 GX-039 + 산괴 동쪽 탑 모노리스 A안 GX-040, Acceptance_V007.json), 충돌 Shell을 더한 맵은 IsleSkin_V009(GX-041, Shell V002)이고
V008·V006·V007·V009는 보존 원본이야. Decisions GX-034~041과 외피 시험 문서 §5·§9~13을 먼저 확인해줘.

먼저 실제 에디터/PIE·미저장 상태를 확인하고 CheckRidgePreservation.py로 보존 해시(V008 721항목·V006·V007 Acceptance)를 다시 맞춰줘.
Lit 미리보기(§9)는 끝났고 화면 비교는 IsleSkinLit_V001 레벨·PreviewViews_V001.json·CaptureViewport로 한다.
충돌 측정 3a(§10)·Shell 파생 3b(§11, GX-041)·베이크 3c-1(§12)·1인 PIE 3c-2와 복원 3c-3(§13, P01~P14 통과)도 끝났다. 현재 충돌 맵은 IsleSkin_V009(Shell V002), 검사용 풀은 DA_GraphicsRidgeIsleSkinPool_V009다(V008은 기각). 정식 풀·밀도는 원래대로다.
다음 단위는 2(섬 윗면 잔돌·식생·섬 위 엄폐 각기둥 ED 외피)야. 엄폐 외피는 V001에서 각기둥 자리가 섬 위 보호 표본과 겹쳐 빠졌으니(§5) 그 원인부터 확인해줘.
섬 외피 윗면 삼각형 가운데가 보행 Floor보다 최대 19cm 위에 보이는 발 묻힘 문제(§11)는 고칠 방법과 비용을 먼저 보여주고 내 결정을 받아줘.
외피 밖 지형의 관통(U4 큰 배치 시각 메시 NoCollision)과 북벽 AttachTo 경고(Z1Cull_V001)는 U5b 소관이니 이 단위에서 고치지 마. 그 경고는 레벨을 로드할 때마다 1건씩 나는 기존 경고야.

GX-038: 부유 표현은 기존 지형의 재해석·보강·중력 완화 장치로만 쓰고 관계없는 빈 공간에 새로 띄우지 마.
뾰족한 갈래 장식과 Cliff02 부유 파편은 피해줘. 공중 형상은 Blender 닫힌 몸체로, 재질은 MI_Ridge_IsleSurface를 기본으로 써줘.
M2(산괴 동굴 덩어리 서쪽 면)는 손대지 않기로 했어.

보호 51공간·16,552표본은 그대로 검사하고 기존 경로/보호 폭을 빼서 PASS로 만들지 마. StairRamp3 31표본 REVIEW·LandmarkTop 사선도 확인해줘.
CrownSummit·StairTerrace2 Pod는 자동 복원하지 말고 GX-033 산괴 배경(A안)도 유지해줘. 새 예외가 필요하면 내 결정을 받아줘.
작업은 V009를 복제한 새 맵(IsleSkin_V010~)이나 별도 미리보기 레벨에서 하고 V008·V006·V007·V009와 공유 원본은 바꾸지 마. PIE 점검이 필요하면 §13의 풀 임시 선택·복원 절차와 TeleportToOctant 지점 계산(PIE_3c2/Columns.py)을 재사용해줘.
GenerateRidgeIsleSkin.py는 V006 Acceptance 보존 대상이니 고치지 말고 필요하면 별도 스크립트를 만들어줘.
외피는 NoCollision, 구면 Up·이음매·Floor50㎡·Nav Layer256 계약을 유지해줘.
단위마다 Verify·보존 해시·같은 시점 화면 비교를 보고하고 다음 진행을 확인해줘. 수동 조작이 더 효율적이면 자동화 전에 바로 요청해줘.
```

## 14. 섬 윗면 단위 2(2026-11-27, GX-042)

시작 상태: TestMap03·PIE=false·dirty 없음, 보존 해시(V008 703쌍·V006 7·V007 14) 불일치 0. 사용자가 아래 네 가지를 모두 권장안으로 정했다(GX-042). 단위는 2a 덮개 재분할 → 2b 엄폐 각기둥(몸체·Shell·베이크·PIE) → 2c 잔돌·식생이다.

### 14.1. 엄폐 각기둥이 없는 원인

- V011 설계(`RidgeOctantSpec.ISLAND_COVER`)에는 섬마다 엄폐 각기둥 3개가 있다. 윗면에 1m 묻힌 낮은 각기둥이고, 높이는 윗면 위 2.8~3.5m다.
- **U3의 V012 기반면은 보존 구조물을 역할 Cave·Island·Bridge로만 골랐다**(`RidgeEDBaseSpec.ROCKS`, manifest `preserved_rocks` 5개). 그래서 각기둥은 그때부터 시각·충돌 모두 없다. V009 충돌 덤프에서 각기둥 중심 6곳의 맨 위 충돌은 모두 섬 윗면 Floor(H 5500/6000)다.
- `GenerateRidgeIsleSkin.py` V001은 "충돌은 기존 각기둥"을 전제로 ED 외피(NordicBoulder_02)만 덮으려 했다(잘못된 전제).
  - 6개 중 4개가 보호 표본 OBB 검사에 걸려 빠졌다. 원인은 각기둥 6개가 모두 섬 보호 공간(반경 29m 원판)의 14.5m 표본 고리 위에 있어서다. 각기둥 안에 +1m 높이 표본이 1~2개씩 들어간다. 서쪽 A는 Pod 9m 원판 표본도 걸린다. V001 검사는 "기존 형상 안" 표본을 따로 분류하지 않았다.
  - V002부터 `isle_shells: False`가 `isle()` 전체를 건너뛰어 남은 2개(C)도 사라졌다. 보호 판단이 아니라 부수 효과다.
- 원판 검사 근사(반경 = 크기 × 1.15, 높이 윗면 -1m~+4m): V011 자리의 새 점유는 Valley A·B·C 2·2·2, West A 2(+Pod 2)·B 2·C 1이다. 중심에서 19~23m 고리는 모든 각도에서 0이다(원판 근사 계산이며 2b에서 실제 몸체로 다시 잰다).

### 14.2. 발 묻힘 실측

V005 덮개는 중심 정점 하나 + 테두리 240정점의 부채꼴이다(한 장 반경 방향 약 28~30m). 평면 삼각형이 구면(반지름 약 24.5km)보다 위로 떠서 Floor 위로 보인다. Dump_V009의 PreservedRocks_Floor로 현지 Up 광선 높이를 다시 쟀다. §11의 19cm는 기록 근거 파일이 없어 다시 측정했다.

| 섬 | V005 덮개 − Floor 최대 / p95 / 최소 | V010 덮개 − Floor 최대 / p95 / 최소 |
|:---|---:|---:|
| 골짜기 | +26.3 / +24.3 / +16.0cm | **−2.4** / −2.8 / −4.0cm |
| 서쪽 | +21.7 / +20.0 / +11.3cm | **−2.5** / −2.8 / −3.9cm |

V005에서는 섬 윗면 거의 전체에서 외피 덮개가 Floor보다 위에 보였다(무게중심·변 중점 표본). V010은 모든 표본이 Floor 아래다.

### 14.3. 2a: 덮개 재분할 → IsleSkin_V010

| 항목 | 내용 |
|:---|:---|
| 형상 | Blender `Scripts/Blender/BuildRidgeIsleTop.py -- cap V010`(약 8초). 동결된 `BuildRidgeIsleBody.py`를 라이브러리로 불러 V005 규칙·난수 흐름(부유석 재시도 횟수는 Geometry_V005 기록)으로 몸체를 재현한다. 정점 수·삼각형 수가 같고 경계 오차 0.0006cm, 부유석 중심 오차 0을 확인한 뒤 덮개만 바꾼다. 동심 고리 f = 1, 0.9 … 0.1(각도 수 240/120/60×4/30×2/15×2) + 중심 부채꼴 15장이고, 모든 덮개 정점은 V011 윗면 3cm 아래 구면 위에 있다 |
| 결과 | `Saved/.../IsleSkin_V001/Geometry_V010/`(FBX 2개·Build.json). 섬마다 8,640 → 9,540삼각형(덮개 240 → 1,140), 열린 변·비다양체 변 0, 보호 51공간·16,552표본 새 점유 0(기존 V011 섬 안 표본 1개씩은 V005와 같음). 옆면·밑면·부유석·석교는 그대로다 |
| UE | `Scripts/Profiling/Graphics/BuildRidgeIsleTop.py build V010` → 다음 Cmd `verify V010`. `IsleSkin_V001/Bodies_V010/SM_Ridge_IsleBody_<섬>_V010` 반입(같은 반입 설정·재질 MI_Ridge_IsleSurface), V009를 복제한 `IsleSkin_V010/LVI_Ridge_EDTerrain_IsleSkin_V010`에서 몸체 액터 2개의 메시·라벨만 바꿨다 |
| Verify | PASS. 몸체 원점·NoCollision·경계 오차 0.1cm 미만·재질·태그, 나머지 최상위 액터 전부 복제 직후와 같음(Level Instance 월드 에셋 포함), 충돌 컴포넌트 6개가 V009와 같음(충돌 지각 V013·Z1 Floor/Shell·보존 Floor/Shell·외피 Shell V002), 외피 충돌 0, dirty 없음 |
| 충돌·베이크 | 충돌은 바뀌지 않았다(Shell V002의 섬 윗면은 이미 Floor 아래). 2a만으로는 재베이크하지 않는다. 2b에서 각기둥 Shell과 함께 V010 계열을 다시 굽는다 |
| 미리보기 | `IsleSkinLit_V001/IsleSkinLit_V010`(`BuildRidgeIsleSkinPreview.py build V010`, Verify PASS) |
| 화면 | 시점 `IsleSkin_V001/PreviewViews_V002.json`(V001 5시점 + 섬마다 윗면 눈높이 170cm·16m 위 4시점). V007(V009와 외관 같음)과 V010 Lit을 시점마다 두 번 찍어 둘째 장을 썼다. `Saved/.../IsleSkin_V001/IsleTop/Shots/`·`Compare/<시점>.png`. 섬 윗면이 이제 보존 Floor 면으로 그려져 재질 무늬만 조금 다르고(차이 30 넘는 화소 0.8~4.4%), LandingEye는 0.03%다. 윤곽·색은 같다 |
| 보존 | 해시 `IsleTop_V010_AfterBuild`·`IsleTop_V010_After2a`: V008 703·V006 7·V007 14 불일치 0. V006~V009 맵·공유 원본은 바꾸지 않았다 |

- 촬영 도구: `CaptureViewport` 결과는 크기 초과로 도구 결과 파일에 저장되며, 그 JSON의 base64 PNG를 풀어 쓴다. 카메라 위치로 시점을 판별한다.
- 확인 못 한 범위: PIE에서 발이 실제로 윗면에 닿아 보이는지는 2b PIE에서 함께 본다. 테두리 고리는 외곽선의 1.029배라 가장 바깥 약 85cm 띠는 Floor 밖 외피 가장자리로 그대로 보인다(V005와 같음).

### 14.4. 2b 엄폐 각기둥: 몸체·Shell·맵(외관 확인 대기)

| 항목 | 내용 |
|:---|:---|
| 형상 | Blender `BuildRidgeIsleTop.py -- cover <판>`(약 6초). V011 각기둥 외곽 4점·꼭짓점별 윗면·바닥(섬 윗면 1m 아래)을 따르고, 윗면은 V011과 같은 중심 부채꼴 보간이다. 둥근 모서리(외곽 안쪽으로만), 안쪽으로만 줄이는 지층·요철, 윗면 모서리 둥글리기로 몸체가 V011 각기둥 안에 들어가게 했다. 섬마다 3개를 한 메시로 묶었다(섬당 7,776삼각형, 각기둥당 2,592) |
| 보호 검사 | 몸체 안 표본은 V011 각기둥 안(외곽 안·바닥~가장 낮은 꼭짓점 윗면)이면 "V011 엄폐 복원", 아니면 새 점유로 센다. 두 판 모두 복원 11(Valley A·B·C 1·2·2, West A 1 + Pod 2·B 2·C 1), **새 점유 0**. LandmarkTop·그래플·착지점 사선 막힘 0, 이음매 최소 29.9m |
| Shell | 각기둥마다 Decimate 비율을 낮은 쪽부터 시험해 맞춤 p95 ≤ 25cm·최대 ≤ 75cm·위상 결함 0인 첫 비율(모두 0.08, 206삼각형)을 합쳐 `SM_Ridge_IsleCover_<판>_Shell`(1,236삼각형)로 만들었다. V001 맞춤 p95 3.7~5.2cm·최대 8.6~14.5cm, V002 p95 2.3~3.1cm·최대 8.0~13.1cm. 위를 보는 면 5.4~13.4㎡(50㎡ 미만이라 새 Floor 없음), Floor 위 -2~40cm 걷는 면 턱 0 |
| 맵 | UE `BuildRidgeIsleTop.py build/verify`. `IsleSkin_V011` = V010 + Cover V001, `IsleSkin_V012` = V010 + Cover V002. 시각 몸체 2개(NoCollision, IsleSkin 태그, `Bodies_Cover_<판>/`) + Shell 1개(`Collision_Cover_<판>/`, 원점·숨김·Shell 역할 태그·LNPStaticBlocker·Static, `PlaceRidgeIsleSkinCollision.import_shell`로 반입). 두 맵 Verify PASS(기존 액터 변경 0, 충돌 = V009 6개 + 엄폐 Shell, 외피 충돌 0, Shell 계약·메시 검사 통과, dirty 없음) |
| 미리보기·화면 | `IsleSkinLit_V011`·`_V012` Verify PASS. `IsleTop/Compare/Cover_<시점>.png`(V010·V011·V012 세 장). V001은 55cm 주기 띠가 규칙적이라 쌓은 타이어처럼 보였고, V002는 띠 주기 흔들기·폭 축소, 3·5갈래 요철, 14cm 기울기, 윗면 요철로 다듬었다(여전히 통 모양이 조금 남는다) |
| 보존 | 해시 `IsleTop_2b_Look` 불일치 0 |

- V001 재실행 확인: 같은 입력으로 다시 구우면 Build.json이 FBX 해시를 빼고 같다. UE에 반입한 V001 에셋은 첫 FBX에서 만든 것이다.
### 14.5. 2b 더 다듬기: 각진·갈라진 사암 덩어리(V013~V015)

사용자가 "더 다듬기"에서 네 방향(각진 사암 덩어리·갈라진 2~3조각·윗단 돌출과 아랫단 파임·밑동 잔돌 무리)을 모두 골랐다.

| 판 | 내용 | 결과 |
|:---|:---|:---|
| Cover V003 · 맵 V013 | 평면 반공간으로 깎은 볼록 덩어리. 각기둥마다 수직 균열(틈 12~18cm, ±3° 기울기)로 3조각, 조각마다 좁은 밑동(외곽 45cm 안쪽, 섬 윗면 위 50~85cm) 위에 넓은 윗단(외곽 8cm 안쪽, 면 6개가 3~9° 바깥으로 기울어 돌출), 모서리 깎기 7회, 잔돌 4~7개(25~70cm, 높이 3~35cm, NoCollision, Shell 제외, Pod 중심 9m 밖) | 위에서 보면 윗면이 사라지고 가파른 면만 칼날처럼 보였다 |
| Cover V004 · 맵 V014 | V003과 같은 형상. 삼각형을 조각 중심 바깥으로 직접 감고 법선 재계산을 하지 않는다 | 화면이 V013과 같았다. 형상·방향은 원인이 아니었다 |
| **맵 V015** | V004 + 엄폐 컴포넌트 재질만 `IsleSkin_V001/Surface_V002/MI_Ridge_IsleRock`(IsleSurface 부모 복제, 경사 판정 Custom 노드를 `float2(1, 0)`으로 고정 → 모든 면 절벽 무늬). 메시 에셋 기본 재질·원본 재질은 그대로 | **덩어리로 읽힌다(외관 확인 대기)** |

- **칼날 원인:** `MI_Ridge_IsleSurface`는 경사로 지면/절벽 무늬를 고르고 월드 정렬로 입힌다. 각진 덩어리의 평평한 윗면이 섬 바닥과 같은 지면 무늬·밝기로 그려져 바닥과 이어져 보였다. 진단 순서: Blender 재반입 조각별 부피·면 방향(35조각 모두 바깥) → UE 메시 삼각형·감는 방향·법선 일치, LOD 1개·Nanite 꺼짐 → 같은 카메라 Blender 렌더(정상) → 재질.
- 검사(V004 = V003 형상): 보호 표본 V011 엄폐 복원 11·새 점유 0, 사선 막힘 0, 이음매 최소 29.7m, 걷는 면 턱 0, 각기둥당 위를 보는 면 13.4~27.3㎡(50㎡ 미만). Shell = 잔돌을 뺀 조각 36개(1,328삼각형, 시각과 같은 형상이라 맞춤 오차 0). 시각 메시 섬당 1,024·1,136삼각형. V013·V014·V015 맵 Verify PASS(V015는 컴포넌트 재질 = IsleRock까지 확인), 미리보기 3개 Verify PASS.
- 화면: `IsleTop/Compare/Cover2_<시점>.png`(V012 둥근 통 ↔ V015), 근접 `Shots/V014|V015_Lit_WestCoverAClose.png`.
- 보존 해시 `IsleTop_2b_V015` 불일치 0. 에디터는 IsleSkinLit_V015·PIE=false·dirty 없음.

- 사용자가 V015 외관을 골랐다(§14.6).

### 14.6. 2b 베이크·1인 PIE·잔돌 충돌(V015 → V016, GX-043)

| 항목 | 내용 |
|:---|:---|
| 베이크 V015 | `BakeRidgeIsleSkin.py dump IsleSkin_V015 → bake V015 → pool V015`. V009 대비 최상위 액터 차이는 섬 몸체 교체 2·엄폐 시각 2·엄폐 Shell 1뿐이다. Layer 82·성분 120·포털 638·Meadow 이음매 일치, Nav 셀 78,343 → 78,162, 스폰 후보 9,360 → 9,348(골짜기·서쪽 섬 Pod 같은 Layer 후보 104·95 → 97·90) |
| PIE V015 | §13 절차(`PIE_2b/SelectionState.json`: 원래 값·INI 8·정식 풀·V015 해시 → 풀 메모리 선택·밀도 0 → 다시 읽기). TestMap03 1인, Ridge slot 1, 오류 0, 이음매 막힘 0, 경고는 기존 북벽 AttachTo 1건. 지점 `PIE_2b/Points.json` Q01~Q07(엄폐 뒤·균열 틈·윗단 위·돌출 아래·섬 윗면 발·잔돌·그래플). **사용자: Q01~Q05·Q07 통과, Q06 "잔돌을 발이 통과하는 게 눈에 띈다. 형상은 마음에 든다 → 형상 유지·충돌"** |
| Cover V005 | `BuildRidgeIsleTop.py -- cover V005`. V004와 같은 규칙·시드라 시각 형상이 같다(삼각형·경계·조각·잔돌 일치 확인). Shell = 엄폐 조각 36 + 섬 윗면 위 8cm 이상 잔돌 28(2,112삼각형, 형상 그대로). 3.3·6.1cm 잔돌 2개는 제외. 보호 새 점유 0·복원 11 |
| 맵 V016 | V010 + Cover V004 시각(IsleRock) + Cover V005 Shell. Verify PASS(충돌 = V009 6 + `SM_Ridge_IsleCover_V005_Shell`) |
| 베이크 V016 | Layer 82·성분 120·포털 638·이음매 일치, Nav 셀 78,134(V015 −28), 스폰 후보 9,345(서쪽 섬 Pod 90 → 87). 검사용 풀 `IsleSkin_V016/DA_GraphicsRidgeIsleSkinPool_V016`(정식 풀 해시 그대로) |
| PIE V016 | Ridge slot 0, 오류 0, 이음매 막힘 0. 지점 `PIE_2b/Points_V016.json` R01(서쪽 엄폐 C 잔돌)·R02(골짜기 엄폐 A 잔돌 무리). **사용자: 통과** |
| 복원 | 두 PIE 모두 정식 풀·밀도 1을 다시 읽어 확인, INI·정식 풀·V015/V016 해시 동일(`SelectionState*.json` restored), 보존 해시 `IsleTop_2b_Final` 불일치 0, TestMap03·PIE=false·dirty 없음 |
| 재현 기록 | Git `Scripts/Profiling/Graphics/IsleSkin_V001/IsleTop_V016.json`(판 사슬·재현 명령·출력 해시·베이크·PIE 판정) |

- 확인하지 않은 범위: 적 밀도 0이라 엄폐·잔돌 주변 적 지상 이동(`blocker_rejects`)은 보지 않았다. 패키지 성능은 범위 밖이다.
- V011~V015 맵·에셋은 비교 이력으로 남겼다(지워도 된다). 충돌·베이크 기준 맵은 V016이며, 2c 시각 배치를 더한 최종 단위 2 채택 맵은 V018이다(§14.7).

### 14.7. 2c 잔돌·식생(V017 → V018, 2026-11-28 사용자 채택)

| 항목 | 내용 |
|:---|:---|
| 도구 | Blender `Scripts/Blender/BuildRidgeIsleTop.py -- scatter <V017\|V018>` (실제 `PreservedRocks_Floor` 높이 투영·구면 현지 Up 정렬·보호 51공간/16,552표본 전수 OBB 검사·그래플/랜드마크 사선 150cm 여유·Pod 9m·착지 4m·엄폐 및 GX-043 밑동 잔돌 회피) · UE `Scripts/Profiling/Graphics/BuildRidgeIsleTop.py build/verify <V017\|V018>` (섬당 Static 루트 + 메시별 Static NoCollision ISM, 태그 `LNP.Surface.Decoration`·프로필 `LNPDecoration`·액터 태그 `Decoration`/`LNPDecoration`/`IsleSkinTrial`) |
| V017(1차) | `IsleSkin_V016` 복제본 + 섬당 **206개**(Fern 24·스케일 0.55~0.80, Clover 74, SmallStonesPack 108·최대 38.4cm·돌출 14.5cm). Verify·미리보기 `IsleSkinLit_V017` PASS. 근경은 자연스러웠으나 16m 부감·`WestTopEye`에서 다소 절제되어 보여 사용자가 더 풍성한 판을 요청했다 |
| **V018(채택)** | `IsleSkin_V016` 복제본 + 섬당 **244개**(GX-042 150~250개 범위): Fern(`SM_Fern_01~06` + `MI_Ridge_Fern`) **44개**(스케일 0.72~0.98, 엄폐 주변·외곽 8포켓 2~3포기 군집, 최대 높이 84.3/93.7cm) · Clover(`SM_CloverVarieties_01~03`) **98개**(01/03 비중·스케일 ×1.22, 3~5장 패치) · 작은 ED 잔돌(`SmallStonesPack_01/02/07/14`) **102개**(앵커 35% 스케일 2.0~2.8·최대 치수 50.6cm < 100cm·돌출 최대 18.6cm, 약 42% 매몰로 바닥 무늬 융화 방지). **사용자 V018 채택** |
| 검사 | Blender `Scatter_V018/Build.json` PASS(보호 51공간·16,552표본 새 점유 0, 사선 0, Pod 9m 0, 착지 4m 0, GX-043 잔돌 겹침 0, 100cm 이상 잔돌 0, 이음매 최소 21.7m). UE `Verify_V018` PASS(기존 액터 변경 0, 충돌 7종 V016과 동일, 외피 충돌 0, Static 루트·ISM 12/13 컴포넌트·인스턴스 244×2 위치/축/스케일 오차 0.0, dirty 없음). `IsleSkinLit_V018` Verify PASS |
| 화면 비교 | `IsleSkinLit_V016`·`_V017`·`_V018` 같은 5시점(`PreviewViews_V002.json`, Lit, `CaptureViewport`, `DisableAllScreenMessages` 후 복원). `IsleTop/Compare/Scatter_V016_V017_V018_<시점>.png` · `Scatter_V016_V018_<시점>.png`. V016↔V018 차이>30 화소: `ValleyTopEye` 4.15%, `ValleyTopHigh` 1.15%, `WestTopEye` 3.28%, `WestTopHigh` 2.49%, `LandingEye` 0.12% |
| 충돌·베이크 | 2c 식생·잔돌은 모두 NoCollision이라 충돌이 바뀌지 않는다. 재베이크하지 않았다(충돌·SurfaceData는 V016과 동일). 검사용 풀을 V018로 올려 PIE에서 식생까지 함께 확인하려면 V016 SurfaceData를 참조하거나 `BakeRidgeIsleSkin.py`로 V018 풀을 만들면 된다 |
| 보존·기록 | `CheckRidgePreservation.py IsleTop_2c_V018`·`IsleTop_2c_Final` 불일치 0(V008 703·V006 7·V007 14). V006~V016·V017·공유 원본 미변경. Git 재현 기록 `Scripts/Profiling/Graphics/IsleSkin_V001/IsleTop_V018.json` |

- 확인하지 않은 범위: `V018`의 1인 PIE 육안·바람(`MI_Ridge_Fern`) 확인과 패키지 GPU 비용은 이번 단위에서 수행하지 않았다(충돌은 V016과 동일).
- 이로써 **부유섬·석교·모노리스 외피 + 충돌 Shell·베이크·PIE + 섬 윗면 단위 2(2a·2b·2c)**가 모두 마감됐다. 다음 Phase 1-BR 본 트랙은 [Phase01_RidgeReferencePatch.md](../phases/Phase01_RidgeReferencePatch.md)의 **U5b(구역별 몸체·충돌 + `Z1Cull_V002` Static/AttachTo 정리)** → U6(전체 베이크·PIE) → U7(잔돌·식생 마감)이다.

### 14.8. 다음 세션(U5b) 인계 및 시작 프롬프트

- **출발 맵:** `IsleSkin_V018` (`/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_V018/LVI_Ridge_EDTerrain_IsleSkin_V018`, V008 + 외피 V007 + 외피 Shell V002 + 섬 윗면 2a/2b/2c 완료). U5b 작업은 `IsleSkin_V018`을 복제한 새 맵(예: `U5bBody_V001` 등)에서 진행하며, V006~V018·U5aBody_V008·Z1Cull_V001 및 공유 원본은 변경하지 않는다.
- **U5b 핵심 범위:**
  1. `Z1Cull_V002` 재생성(15레벨 루트·ISM 전부 Static 저장 후 디스크 재로드 확인, 북벽 AttachTo 경고 제거, 인스턴스 1개짜리 ISM 합치기 검토).
  2. U4 V011 채택 추가 21소유자(현재 NoCollision 시각 메시라 PIE에서 관통됨)와 U5a 채택 몸체 5 + 별도 연결 1을 포함해 구역별 닫힌 몸체·외피 이음·최종 충돌 프록시(`_Floor`/`_Shell`)·기반면 절개·내부 잔존 Support 제거.
  3. U5a-7에서 남은 R01/StairRamp3 REVIEW와 구역별 구도·51보호 공간(16,552표본)·Nav Layer 상한 256·이음매 0cm 계약 유지.
- **U7 이월 사항:** 잔돌·식생 전체 바닥 보강은 U5b → U6 이후 U7에서 에디터 타임 생성 + 셀 분할 Static ISM + 거리 컬링/WPO 제어(A안)로 한 번에 진행한다([Phase01_RidgeReferencePatch.md](../phases/Phase01_RidgeReferencePatch.md#u7-배경-프랍-채우기ed-스타일)).

```text
@D:/UnrealProjects/LootNPop/.context/Graphics/README.md
@D:/UnrealProjects/LootNPop/.context/Graphics/Current.md
@D:/UnrealProjects/LootNPop/.context/Graphics/phases/Phase01_RidgeReferencePatch.md
@D:/UnrealProjects/LootNPop/.context/Graphics/analysis/Phase01_IsleSkinTrial.md
@D:/UnrealProjects/LootNPop/.context/Graphics/analysis/Phase01_U5aBodyHandoff.md

부유섬·석교·모노리스 외피와 섬 윗면 단위 2(2a V010·2b V016·2c V018 채택)가 모두 완료됐어.
이제 Phase 1-BR 본 트랙의 U5b(구역별 닫힌 몸체·외피 이음·최종 충돌 프록시·기반면 절개 + Z1Cull_V002 Static/AttachTo 정리)를 시작하자.
Decisions GX-034~043도 먼저 확인해줘.

먼저 에디터/PIE·미저장 상태를 확인하고 CheckRidgePreservation.py로 보존 해시(V008·V006·V007)를 맞춰줘.
현재 출발 기준 맵은 IsleSkin_V018(충돌·SurfaceData 기준은 V016)이고, V006~V018·U5aBody_V008·Z1Cull_V001과 공유 원본·동결 스크립트는 보존 대상이니 수정하지 마.
U5b 작업은 IsleSkin_V018을 복제한 새 판에서 진행하고, 먼저 U5b 전체 범위(Z1Cull_V002 Static/AttachTo 정리, U5a 몸체 5+연결 1 충돌화, U4 V011 구역별 21소유자 몸체·충돌·기반면 절개)를 조사해서 구현 단위와 진행 순서를 제안해줘.
보호 51공간·16,552표본, Floor 50㎡ / Nav Layer 256, 이음매 0cm, 부유석 유지(GX-037·038) 계약을 지키고, C++ 변경이 필요하면 빌드 전에 꼭 먼저 확인해줘.
잔돌·식생 전체 마감은 기존 계획대로 U5b → U6 이후 U7에서 진행할 거야.
```
