# Ridge 정식 풀 통합·최종 검증

2026-11-11. 사용자는 V011 전체 동선(적 밀도 0)과 교전 느낌(적 밀도 1)을 PIE로 확인한 뒤 풀 통합을 요청했다. 이때 V011 지형, 앵커 6개, Pod 12개, 검토점 27개를 그대로 채택했다.

이번 통합에서 하지 않은 일:
- C++ 변경, Live Coding, 에디터 빌드
- 전역 그래픽스 설정 변경
- DefaultGame.ini 변경

절차는 Grove 정식 통합([Grove Production](../Grove_00/Production.md))과 같다.

## 정식 경로와 보존

| 역할 | 경로 |
|:---|:---|
| LVI | `/Game/Maps/Ridge_00/LVI_Octant_Ridge_00` |
| SurfaceData | `/Game/Maps/Ridge_00/DA_OctantSurface_Ridge_00` |
| 지각 | `/Game/Maps/Ridge_00/SM_Octant_Ridge_00_V011_R30000` |
| 암반 외피 | `/Game/Maps/Ridge_00/Authored/SM_Ridge_00_V011_Rocks_Floor`, `SM_Ridge_00_V011_Rocks_Shell` |
| 재질 | `/Game/Maps/Ridge_00/M_Ridge_Ground`, `M_Ridge_Rock` |

**복사와 보존**
- 원본은 `_WorkInProgress/Ridge_00/V011` 작업본이다. 원본은 옮기지 않고 정식 사본을 만들었다(`Scripts/FinalizeRidgeOctant.py`, commandlet).
- 액터 48개의 변환·태그·역할별 속성은 경로 치환 외에 원본과 같다. 비교한 속성은 Pod의 `EnemyFormations`, 앵커의 사거리·클래스, 메시 충돌 설정이다.
- 정식 패키지의 의존 그래프 10개에 WIP 참조가 없다.
- 원본 작업본 해시는 바뀌지 않았다.

**풀 등록**
- 실제 풀에는 Ridge 정의 1개를 추가했다(`Scripts/IntegrateRidgePool.py`, 라이브 에디터 저장).
- 기존 5개 정의(Bastion/Hollow/Acropolis/Pyramid/Grove)와 `octantPool` 목록은 그대로다.
- 새 정의는 회전 255, signature `DysonSphere_R300m_V1`이다. 전체 6종이 됐다. 정의 원문은 [OctantDefinition.txt](OctantDefinition.txt)에 있다.
- 추적 파일 중 바뀐 것은 실제 풀 `OctantPoolData.uasset` 하나다.

## 베이크와 자동 확인

| Stream | 요소 | 바이트 |
|:---|---:|---:|
| Support | 593,257 | 4,202,801 |
| Navigation | 73,836 | 599,012 |
| Traversal | 1,457 | 22,279 |
| Spawn | 9,385 | 381,413 |
| ConditionalPatch | 0 | 4 |

- 요소 수는 V011 작업본 베이크와 같다.
- Grove 기준 seam 2,208개가 일치한다.

**전체 자동 검사**
- 실제 6종 풀로 `LootNPop.SurfaceNavigation` 전체 135개를 실행했다(`Scripts/RunRidgeValidation.ps1`).
- 결과는 130개 성공, 5개 경고 성공, 실패 0이다. 재실행이나 재베이크 없이 첫 실행에서 통과했다.
- `Nav.PodOverlay`와 `Nav.ProductionSpawnProjection`도 통과했다.
- 경고 5건은 Grove 때와 같은 알려진 소음이다. 외부 HTTP timeout 4건과 기존 B-option authoring world 현재성 안내 1건이다.
- 시험편 3개는 Git 원본으로 복원했다.

## 혼합 PIE

TestMap03에서 정식 6종 풀로 실행했다(적 밀도 1).

| 항목 | 결과 |
|:---|:---|
| 슬롯 배정 | Ridge slot0, Bastion 1·7, Pyramid 2, Acropolis 3, Hollow 4, Grove 5·6 |
| 슬롯·바인딩 | 8slot, 264bindings |
| seam | Nav seamLinks 4,428, 높이 차 0cm, 차단 node/edge 0 |
| Pod·적 | Pod 120/120, 적 2,223/2,223 |
| 로그 | 오류·경고 0 |

통행·교전은 V011 단독 풀에서 사용자가 직접 확인했다. 정식 사본은 런타임 스트림 요소 수와 액터 배치가 같아 그 확인을 그대로 유지한다. 혼합 풀에서 이웃 옥탄트 경계를 실제로 걸어서 왕복하는 확인은 따로 받지 않았다.

## 중간 산출물

정식 옥탄트에 연결된 것 외에는 모두 Git 무시 경로에 있다.

| 구분 | 위치 |
|:---|:---|
| UE 작업본 V001~V011, 검사용 풀 | `/Game/Maps/_WorkInProgress/Ridge_00/` |
| 이전 blend V001~V010, 이전 미리보기, V003 사양 사본 | `Art/Octants/_WorkInProgress/Ridge_00/` |

Art 폴더에는 다음만 둔다.
- 정식 형상 원본 `Ridge_00_V011.blend`
- V011 미리보기
- 단면도 `Sections.svg`
- 문서

근거는 `Saved/SliceOctant/Ridge_00/Production`에 있다.
- `integration.json`
- `actor-preservation.json`
- `bake-summary.json`
- `formal-validation.json`
- `SurfaceNavigationTests/`
- `pie-startup.json`
- `Before/`(풀·원본 백업)
