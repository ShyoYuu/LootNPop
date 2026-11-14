# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 1-B · 1-B4a 근경 시험 마감 · 다음 1-B4b 진행 확인 대기
> 마지막 확인/갱신: 2026-11-15

## 1. 이어받을 상태

- **1-A 출발 기준선 완료.** Ridge slot1·확정 A/B/C, Medium3회 중앙값 Frame/GT/RT/GPU=19.687/5.022/19.681/19.223ms·재현성 통과. 적0 센서 보완까지 마쳤다. [보고서](analysis/Phase01_StartBaselineReport.md). Phase0의 Meadow 포함 과거 수치와 섞지 않는다.
- **1-B1/2 반입 완료.** Electric Dreams의 14시작점·104패키지를 원래 `/Game` 경로로 반입했다. VT65/66·실효 `r.VirtualTextures=1`, 로드/메시/재질 미리보기를 확인했다. Migrate는 재직렬화하므로 원본과 반입본 해시는 다르다. [반입 실행서](phases/Phase01_EnvironmentImport.md) §7.
- **1-B3a~c 구현/기술 검증 마감·최종 채택 보류.** 구면 좌표/구역/경사 진단, 로컬 트라이플래너·월드 노멀, Texture_Bombing과 바닥의 큰 크기 색/거칠기 변화를 구현했다. AA 끄기 중립 노멀 비교는 내부29,043표본·전체/세 경사 구간 p95=1·최대2(RGB8)다. 한 +90° 회전 사례 후 복원했다. 원거리 잔여 반복과 **VT 조회 추정18→72·픽셀 명령1565→5367** 때문에 최종 화질/성능 채택은 보류다. 에디터 통계를 GPU ms로 해석하지 않는다. 실행서 §9~12.
- **1-B4a 소수 근경 시험 마감.** 사암2·클로버6은 자연스러움/부유·이질감 없음으로 사용자 확인을 받았다. DryRiverBed_02는 표면은 자연스럽지만 네모 외곽이 주변과 어울리지 않아 제외하고 시험2액터만 제거했다. 원본은 보존했다. [현재 팔레트](phases/Phase01_RidgeZonePalette.md) §10, 실행서 §13.
- **정식 Ridge·Support source·기본 편성·풀·확정 카메라 경로는 보존했다.** 원본104/보호12/V002재질4파일 해시와 기존 지각3메시 상태를 확인했다. 정식 적용·PCG·Support/Nav 베이크·새 빌드/쿡은 이번 1-B에서 하지 않았다. Phase1 전체는 미완료, 파라곤 선정도 남아 있다.

## 2. 로컬 에디터와 재생성

마지막 확인은 LootNPop(8000), PIE=false, Lit, `r.AntiAliasingMethod=4`다. Electric Dreams(8001)는 VRAM 확보를 위해 종료했다. 다음 세션에서는 실제 연결/현재 맵/dirty 상태를 다시 읽는다.

| 항목 | 마지막 저장 상태 |
|:---|:---|
| 열린 WIP | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe` |
| 지각 오버라이드 | `/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V002/MI_Ridge_Surface` |
| 소수 장식 | `Graphics/GroundProbe_V001`의 사암2·클로버6, 라벨 `GraphicsGroundProbe_*` |
| 장식 계약 | Decoration 단독·LNPDecoration/NoCollision·Static·내비 영향 꺼짐. 그림자/RT/DF 기여는 기본값 유지 |
| 조명 | WIP의 에디터 전용 `GraphicsPreview_SurfaceLight` 점광원1개. 게임 조명/성능의 통과 근거가 아님 |
| 저장 | WIP와 생성 에셋 dirty=false, 원래3메시/광원 transform 복원 |

**현재 산출물이 있으므로 아래 빌더를 재실행하지 않는다.** 모두 저장소 `Scripts/Profiling/Graphics/`에 있다.

| 코드 | 역할·선행 조건 |
|:---|:---|
| `ElectricDreamsPalette.json` | 초기14시작점/104패키지 식별·크기·SHA256. 실패한 하상도 반입 이력으로 보존 |
| `BuildRidgeSurfaceProbe.py` | 진단 함수/재질/MI5개만 생성 |
| `BuildRidgeSurfaceMaterial.py` | 원본 부모/MI를 파생한 V001 2개 생성. 원본 반입·진단 함수 필요 |
| `BuildRidgeSurfaceAntitile.py` | V001에서 V002 함수2/부모/MI4개 생성. 최종 바닥 보완 후 전체 깨끗한 재생성은 미실행 |
| `BuildRidgeGroundProbe.py` | 열린 WIP에 최종 사암2/클로버6 배치·WIP만 저장. 실제 지각 hit/보호 가드 후 생성 |

깨끗한 체크아웃은 **반입→진단→V001→V002 생성**, 정식 LVI를 **수동 Save Current Level As로 WIP 생성**→지각 Element0에 V002 MI 적용→소수 배치 순서다. 세 재질 빌더는 맵을 조작하지 않는다. **World `duplicate_asset` 직후 `load_level`은 World Memory Leaks 크래시를 냈으므로 재시도하지 않는다.** 상세 재현/검사용 광원 설정은 실행서를 따른다.

원본/파생 에셋·WIP·Saved는 Git 제외다. 저장소에는 식별 목록·재생성 코드·문서만 남긴다. 로컬 데이터가 없는 새 체크아웃에서 에셋이 자동 제공된다고 가정하지 않는다.

## 3. 다음 구현 단위와 남은 검증

1. **다음 제안: 1-B4b 이끼 바위·절벽 부분 보강 소수 시험. 아직 미승인·미착수다.** 다음 세션에서 이 단위 진행 요청을 받으면 필요한 상태를 확인하고 착수한다. MossyForestBoulder_06의 MI는 **09**다. Cliff_01은 약13.59×9.33×14.19m여서 벽 전체 외장으로 쓰지 않고 노출/접촉을 함께 만족하는 작은 부분만 시험한다. 상세 팔레트/보호 원칙은 선정 문서 §8.5~8.7·10을 따른다.
2. Fern은 별도 구면 바람/WPO·변위 시험이 필요하다. 하상 재도입/다른 후보 자동 대체는 승인하지 않았다. 1-B0을 재선정하지 않는다.
3. 대표 구역 PCG/전체 확장 전 **재질 샘플 비용 축소·최종 화질·전체 bounds 보호 마스크·PIE 발/카메라/탄환·실제 SurfaceData 현재성/런타임 로드**를 확인한다. 현재 WIP의 런타임 SupportData 연결이나 독립 PIE 성공을 전제하지 않는다.
4. 원거리 반복·전체 밀도, 구역의 단단한 경계, 기존 지각의 큰 삼각형 음영, 모든 회전 slot/적도/이음매, 게임 조명과 패키지 성능은 미검증이다. 근경 확인이나 한 에디터 회전을 전체 통과로 확대하지 않는다.

## 4. 근거와 재발 방지

공통 상위는 `Saved/Profiling/Graphics/Phase01/`이며 Saved는 로컬 자료다. 현재 결과 요약/재현 절차는 추적되는 [환경 반입 실행서](phases/Phase01_EnvironmentImport.md)가 소유한다.

| 자료 | 위치·핵심 파일 |
|:---|:---|
| 반입 식별/검증 | `EnvironmentImport_20261114/`의 MigratedFiles·AssetVerification·FinalFileChecks/LiveChecks |
| 좌표 진단/맵 크래시 | `RidgeSurfaceProbe_20261114/`의 Completion·FinalFileChecks·Crash_20261114 |
| V001 투영/노멀 | `RidgeSurfaceMaterial_20261114/`의 Verification·NeutralNormalWorldComparison |
| V002 반복 완화 | `RidgeSurfaceAntitile_20261114/`의 Verification·FinalStatistics·NeutralNormalNoAAComparison·Views |
| 최종8개 소수 시험 | `RidgeGroundProbe_20261115/`의 Verification·FinalFileChecks·WipAfter·Views/Final_Close/A. BeforePlacement.umap은 배치 전 백업 |
| 커밋 제외 검사 | `Handoff_20261115/RepositoryAudit.json` |

- UE5.8 scalar setter는 성공해도 false를 반환하는 경우가 있어 getter로 검증한다. HLSL 예약어 `line`을 변수로 쓰지 않는다. MaterialFunction 노드 조회에는 Material 전용 API를 쓰지 않는다.
- 노멀 표시 비교는 TSR 켜짐에서 변경 없는 캡처끼리도 차이가 났다. 진단 AA0/구역 내부 마스크/동일 구도를 사용한 결과와 실패 자료를 분리하고 실제 노멀·AA4·Lit를 복원했다. 같은 실효값 복구이며 원래 콘솔 변수 우선순위까지 복원됐는지는 미확인이다.
- 소수 배치의 spec 높이 오차30cm 가드는 실제 지각 차이로 중단돼 **실제 hit component 확인**으로 고쳤다. 2m 경로 여유·bounds→spec 계수1.2는 시험용 가드이며 최종 PCG 규약이 아니다.
- **구현 단위마다 계속 진행할지 확인하고, 수동 PIE/GUI가 빠르면 즉시 요청한다.** 현재 다음 단위 승인은 받지 않았다. 기존 미저장 사용자 변경을 임의로 버리거나 저장하지 않는다.
- 성능은 패키지에서만 판정한다. 새 전체 빌드/쿡 전 병행 작업을 확인하고 AGENTS의 권한 확장 규칙을 따른다. 화면 녹화는 별도 사용자 확인이 필요하다.