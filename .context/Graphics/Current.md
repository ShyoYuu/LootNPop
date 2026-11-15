# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 1-B · 1-B3e~g/1-B4e 마감 · 1-B5a는 사용자 요청으로 새 세션 이관·미착수
> 마지막 확인/갱신: 2026-11-16(에디터·복원·커밋 제외 대상 확인)

## 1. 이어받을 결과

- **1-A 출발 기준선 완료.** Ridge slot1·확정 A/B/C, Medium3회 Frame 중앙값19.687ms. 재측정 출발점은 [보고서](analysis/Phase01_StartBaselineReport.md)이며 Meadow를 포함한 과거 Phase0 수치와 섞지 않는다.
- **V005를 WIP 조건부 후보로 유지한다.** 1-B3e 비교에서 V003의 규칙적 바닥 무늬는 macro를 꺼도 남았다. 1-B3f는 절벽 V003 두 샘플을 유지하고 바닥만 삼각 격자3샘플로 바꿨다. 사용자 원경 개선·근경 자연스러움 확인, V004 대비 VT54→45·픽셀4170→3401·정점148 동일이다. 에디터 추정이며 GPU ms가 아니다. AA0 중립 노멀22,731표본·전체/세 경사 구간 p95=1·최대2(RGB8)를 통과했다. [실행서](phases/Phase01_EnvironmentImport.md) §18~19.
- **1-B3g 소수 회전 시험 마감.** slot5(Pitch180/Yaw90)와 적도 임의 회전(Roll54.337285)에서 사용자 재질/Fern 확인을 받았다. 메시15/광원1/카메라를 복원·대조·저장했다. 임시 시험 구역의 이진 경계는 의도된 범위 표시다. 경계 미관 수용 질문은 철회했고 완화하지 않았다. §20.
- **1-B4e/4e-1 런타임·수동 접촉 검증 마감.** WIP 전용 버전7 SurfaceData와 별도 풀을 만들었다. 첫 PIE WIP slot5/7·166 bindings, 사용자 EnemyDensity0 재시작 후 slot2/6·195 bindings로 모두8 validated slots·seam delta0cm였다. 사용자는 두 번째 실행의 slot2에서 보행·착지·카메라·사격 모두 문제없다고 확인했고 WhereAmI도 WIP/실제 지각을 확인했다. 종료 후 원래 풀 선택·WIP·카메라를 복원했다. §21~22.
- **Phase1 전체는 미완료다.** 정식 Ridge 적용·PCG·전체 밀도·파라곤 선정·콘텐츠 패키지 성능은 남았다. 원본104패키지·정식 Ridge/Support source·기본 편성·정식 풀·확정 카메라 경로는 보존했다. 재생성·검증 상세는 실행서, 완료 경과는 [Phase1 로그](history/Phase01_Log.md)가 소유한다.

## 2. 마지막 로컬 상태와 재현

| 항목 | 마지막 확인 상태 |
|:---|:---|
| 에디터 | LootNPop8000, PIE=false, Lit/AA4. Electric Dreams8001은 이전에 종료했으며 이번에 연결하지 않음 |
| 열린 WIP | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe` |
| 지각/Fern | `RidgeSurface_V005/MI_Ridge_Surface`, `RidgeFernWind_V001/MI_Ridge_Fern` 적용. 실제 노멀2개 복원 |
| 기존 메시 | 지각/암반3 + 장식12. 사암2/클로버6/이끼2/축소 절벽1/Fern1 유지. 이끼 MI09·절벽 scale0.15. DryRiverBed 제외·원본 보존 |
| 장식 계약 | Decoration 단독·LNPDecoration/NoCollision·Static·내비 영향 꺼짐. 실제 Support2소스·Blocker 전용1메시 유지 |
| 새 데이터 | WIP와 같은 폴더의 `DA_OctantSurface_LVI_Ridge_SurfaceProbe`, DataVersion7·SourceLevel=WIP1개 |
| 검사용 풀 | 같은 폴더의 `DA_GraphicsRidgeValidationPool`. 정식6개 정의 중 Ridge의 Level/SurfaceData 쌍만 교체 |
| 현재 풀 선택 | **원래 `/Game/Maps/OctantPoolData`로 복원됨.** 평소 PIE는 정식 Ridge를 사용함. SaveConfig 미호출 |
| 사용자 CVar | **`LNP.Spawn.EnemyDensity=0` 유지.** 다음 실행의 목적에 맞게 확인하고 임의로1로 되돌리지 않음 |
| 저장/보존 | WIP/V005 부모·MI/새 데이터/검사용 풀/정식 풀 dirty=false.15메시 변화0·보호142+WIP/INI 추가3파일 변화0 |
| 카메라/조명 | 베이크 전 사용자 경계 구도 복원. 에디터 전용 검사용 점광원1개 유지. 게임 조명/성능 승인으로 해석하지 않음 |

기존 빌더는 **이미 산출물이 있으므로 재실행하지 않는다.** `BuildRidgeSurfaceTriangle.py`는 V005 함수2/부모/MI를 생성하는 새 재생성 코드다. 기존 V001/V002→V003/V004→V005 의존성과 현재 WIP 베이크/풀 재현은 실행서 §19·§22를 따른다. 새 체크아웃에서는 원본 반입과 기존 소수 배치를 먼저 재현하며, 소수 배치 빌더의 V002 가드를 지키도록 새 지각 MI 적용은 마지막에 한다.

WIP를 정상 게임에서 다시 검토하려면 TestMap03에서 검사용 풀을 **메모리에서만 임시 선택**하고 PIE를 시작한다. 매 실행의 실제 WIP slot을 로그에서 다시 찾고 snapshot 게시 성공 뒤 접촉을 본다. 종료 후 원래 풀을 복원한다. 정확한 tool 대상·값·베이크 명령은 실행서 §22에 있다. 기본 설정 파일이나 정식 풀을 수정하는 경로로 대체하지 않는다.

원본/파생 에셋·WIP·SurfaceData/풀·Saved는 Git 제외다. 저장소에는 식별 목록·재생성 코드·절차만 남긴다. 로컬 데이터가 없는 체크아웃에 에셋이 자동 제공된다고 가정하지 않는다. **World `duplicate_asset` 직후 `load_level` 크래시를 재시도하지 않는다.** WIP 생성은 수동 Save Current Level As 경로를 따른다.

## 3. 다음 세션: 1-B5a

**사용자 요청으로 새 세션에 넘겼으며 이번 세션에서는 PCG를 착수하지 않았다.** 다음 단위는 전체 bounds 보호 마스크를 포함한 대표 구역 PCG 소수 배치다. 상세 실행서는 다음 세션 착수 시 구체화한다.

1. `README.md`·이 문서·`Decisions.md`, 실행서 §19~22와 [현재 팔레트](phases/Phase01_RidgeZonePalette.md) §10을 읽는다. 에디터 현재 맵/PIE/dirty·V005/Fern 바인딩·원래 풀 선택·EnemyDensity0·복원 기록을 읽기 전용으로 대조한다.
2. 기존 대표 구역·보호 대상(동선/Pod/훅 등)을 유지하고 **배치점이 아닌 회전·스케일·WPO를 고려한 메시 전체 bounds**로 침범을 제한한다. 기존 소수 시험의2m 여유·bounds→spec 계수1.2·바람30cm는 최종 PCG 규약이 아니므로 그대로 확정하지 않는다. 새 여유 폭/밀도는 근거를 정한 뒤 고정 시드의 작은 배치로 검증한다.
3. 기존 수동12개와 새 PCG 산출물을 구분해 보존하고, Decoration/NoCollision·내비 영향 꺼짐과 구면 현지 Up 접지를 유지한다. PCG 작업에는 등록된 PCG 프로젝트 스킬/기존 도구를 먼저 확인한다. GUI/PIE가 더 빠르면 즉시 수동 요청한다.
4. 배치 후 전체 bounds 보호·접지/노출·시야/카메라/탄환을 확인한다. WIP 입력 변경 후 SurfaceData 현재성을 다시 확인하고 필요한 범위의 베이크만 판단한다. 작은 배치 통과를 전체 밀도나 Ridge 전체 확장 승인으로 확대하지 않는다.

새 세션에서1-B5a 착수를 명시하면 그 요청을 승인으로 보고 진행한다. 그 외에는 진행 여부를 먼저 확인한다. **구현 단위마다 다음 진행을 확인하며 기존 미저장 사용자 변경을 임의로 버리거나 저장하지 않는다.** 임시 시험 경계는 유지하고 최종 지형 간 전이는 필요할 때 별도로 판단한다.

아직 미검증인 범위는 전체8 slot/극점·바람 기준축 전환/모든 인접 조합,2인 호스트·게스트 접촉/적 전투, 전체 밀도·원거리 반복·게임 조명/패키지 GPU 시간이다. 성능 판정은 패키지에서만 하며 새 빌드/쿡 전 병행 작업을 확인하고 AGENTS의 권한 확장 규칙을 따른다. 화면 녹화는 별도 사용자 확인이 필요하다.

## 4. 근거와 복구 기록

공통 상위는 `Saved/Profiling/Graphics/Phase01/`이고 Saved는 로컬 자료다. 추적되는 재현 절차/판정 원본은 실행서 §7~22다.

| 자료 | 핵심 기록 |
|:---|:---|
| `RidgeSurfaceRepetition_20261116/` | macro 유무8캡처·진단 MI2개·WipBefore/Restored·TechnicalVerification·FinalLiveVerification/FileChecks·Verification. BeforeComparison.umap은 V004 |
| `RidgeSurfaceTriangle_20261116/` | Creation·GraphVerification·NeutralNormalComparison·NormalDiagnosticState=`restored_saved`·UserSaveVerification·Verification. 이전 umap2개는 모두 V004이며 V005 사본으로 오인하지 않음 |
| `RidgeSurfaceOrientation_20261116/` | slot5/equator Plan/Verification·Preview=`restored_saved`·BoundaryDiagnosticState=`restored`·Restoration·UserVerdicts·Views10개. BeforeRotation.umap은 사용자 저장 V005 |
| `RidgeRuntimeProbe_20261116/` | 베이크 전 초기 풀/WorldSettings·SurfaceData42개 중 WIP 일치0·CurrentWipContract. 이후 아래 선행 작업으로 해소 |
| `RidgeRuntimeValidation_20261116/` | Before·BakeHeader/Log·ValidationPool·SelectionState=`restored`·PIEStart/RestartLog·ContactLocationLog·ManualVerdict·FinalLiveVerification/FileChecks·Verification. 실제 게임 화면은 `Views/pie_game_shot.png`; 검은 native 캡처는 무효 |
| `Handoff_20261116/` | 새 세션 인계·소스 구문/문서 링크·원본/설정/복원·Git 포함/제외 최종 검사 |

이전 반입·재질·소수 배치/Fern의 로컬 근거와 원본/파생 복구 사본은 실행서 §7~17을 따른다. UE5.8 scalar setter는 반환 false만으로 실패를 판정하지 말고 getter로 확인한다. 중립 노멀 수치 비교는 AA0/구역 내부 마스크/동일 구도를 사용하고 실제 노멀·AA4·Lit를 복원한다. PIE 게임창이 분리돼 있으면 native CaptureViewport의 검은 에디터 화면을 근거로 쓰지 않고 게임 콘솔 `Shot`으로 확인한다.

## 5. 새 세션 요청 예시

> `.context/Graphics/README.md`, `Current.md`, `Decisions.md`를 읽고1-B5a를 진행해줘. 1-B3e~g/1-B4e와 WIP 전용 베이크·검사용 풀·1인 수동 접촉은 마감했고 임시 풀/맵/카메라도 복원했어. 기존 구역·팔레트·수동12개·지각 V005와 보정 Fern을 유지하고, 전체 bounds 보호 마스크를 포함한 대표 구역 PCG 소수 배치부터 해줘. 기존 빌더는 재실행하지 말고 정식 풀/원본 데이터/설정을 보존해줘. 구현 단위마다 다음 진행을 확인하고 수동 조작이 더 효율적이면 즉시 요청해줘.
