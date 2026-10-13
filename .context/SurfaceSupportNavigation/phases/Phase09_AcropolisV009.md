# Acropolis_00 V009 — 부유섬 변형과 깊은 작은 동굴

현재 기준본은 **[V011 최종 접합 틈 수정](Phase09_AcropolisV011.md)**이다. 사용자가 아래 V009 전체를 PIE로 승인했고, 마지막 Pocket_05 지붕 접합 틈을 V011에서 수정했다. 아래는 V009 당시의 제작·검사 기록이다.

2026-10-13. 사용자가 V005를 PIE로 탐색한 뒤 전체 구성과 훅 배치는 유지하고, 섬의 크기·모양과 동굴 깊이·천고를 수정하도록 요청했다. 입구 방향은 보존 조건에서 제외했으며 옆으로 꺾이는 진입도 허용했다.

## 변경 범위

- 현재 원본: `Art/Octants/Acropolis_00/Acropolis_00_V009.blend`.
- LVI·SurfaceData·실제 풀 경로는 V005와 같다. 실제 풀에는 Acropolis가 이미 등록되어 있다.
- 섬 중심·기준 높이·훅 28개의 위치/회전/사거리/태그를 보존했다. 기존 훅/Pod 마커 ID 61개를 재발급하지 않았다.
- 중앙 언덕·보스 기단과 세 방향 진입, 지상 회복 동선을 유지했다.

| 부유섬 형태 | 수 | 변화 |
|:---|---:|:---|
| Small | 4 | V005의 팔각형 크기를 작은 섬 기준으로 유지 |
| Broad | 8 | 두 축을 1.5배·1.35배로 확장 |
| Long | 4 | 긴 축 1.95배, 짧은 축은 유지 |
| Elbow | 4 | 중앙 착지면에서 두 팔이 뻗는 ㄱ자 윤곽 |
| Terrace | 4 | 넓은 하층과 7m 높은 상층을 경사로로 연결한 단일 외피 |

긴 축은 섬 띠의 접선 쪽을 향한다. 모든 기존 중심 Pod는 같은 위치의 낮은 평탄부에 남는다. 확대된 착지면과 부딪히는 공중 암석은 제외하고 주변에 재분포해 53개를 배치했다. 새로운 훅을 추가하지 않았다.

동굴은 본실의 폭·길이를 유지하고 바닥 H=-7100, 천장 H=-3900으로 변경했다. 지상 H=-3500 기준 깊이 36m, 본실 천고 32m다. 구면 투영 전 파라미터 기준의 하강 경로 길이는 50m이며, 첫 15m 이후부터 천장으로 덮었다. Pocket_01은 직선형, Pocket_02/04는 +40도, Pocket_03/05는 -40도로 꺾인다. 입구는 기존보다 10m 안쪽이며 굽이의 양쪽 가장자리를 함께 나눠 안쪽 급경사를 완화했다. 본실 Pod 5개와 검토점을 새 바닥 위치로 이동했다.

V005 동굴의 베이크 Headroom은 800.014~800.082cm였다. 전역 편성의 비행형 적이 요구하는 약 2638cm를 충족하지 못해 generic authored Pod가 제외됐다. V009의 같은 GUID 5개는 3199.945~3200.079cm를 기록했다. 전역 NPC 설정이나 스폰 필터를 완화하지 않았다.

## 제작·검사

형상 생성·배치 원본 검사는 기존 Blender 스크립트를 V009로 갱신했다. 이미 존재하는 에셋 갱신은 `Scripts/ReviseAcropolisOctant.py`로 처리한다. 이 스크립트는 기존 Geometry Actor와 마커를 제거하지 않고 메시를 재반입하고 동굴 Pod/검토점만 이동한다. 최초 제작용 `AssembleAcropolisOctant.py`의 markers 재생성은 기존 마커 ID를 보존하는 갱신 방법이 아니다.

반입 후 `FinalizeAcropolisSeam.py` → `BakeAcropolisOctant.py` → `VerifyAcropolisReady.py` 순서로 열린 에디터 콘솔의 `py <절대 경로>`를 실행했다. 저장본 검사는 물리 장면이 있는 에디터 모드에서 수행했다.

| 검사 | 결과 |
|:---|:---|
| 원본 토폴로지 | 각 닫힌 몸체의 비정상 edge·중복·퇴화·방향 불일치 0 |
| 섬 간 교차 | 0, 24개 섬 모두 기존 세 진입점에서 연결 그래프상 도달 |
| 경계 | native 정점 13989개 복원, 베이크 seam 표본 2208개 Meadow와 일치 |
| 마커 | 위치 오차 0cm, Up 오차 약 3.6e-15, 훅 상태 28개·마커 ID 61개 보존 |
| Pod 바닥 | 엔진 광선 297개 통과, 최대 오차 약 0.246cm |
| 동굴 바닥·진입 | 엔진 광선 330개 통과, 원본 최대 경사 약 41.36도 |
| 동굴 천장 | 엔진 광선 5개 통과, 본실 높이 약 32m |
| 훅 | 수정 형상에서 시야 후보 91개 엔진 광선 통과 |
| 베이크 | Support 2344601 samples·16551612 bytes, Nav 136861 cells·1110924 bytes |
| 나머지 stream | Traversal 42703 bytes, Spawn 487816 bytes, ConditionalPatch 4 bytes |

네이티브 자동 검사 3개는 오류 0·경고 0으로 통과했다. `ProductionSurfaceDataDefinitions`는 32개 시드·16종 경계 조합·양방향 경로 2304개와 seam delta=0, `ProductionSpawnComposition`은 33개 배치의 NPC 84475/84475 전량 배정·부족 배치 0을 확인했다. `ProductionPoolCurrentAndDeterministic`은 Acropolis 단독 검사용 풀에서 저장된 원본 hash와 5개 stream이 두 번의 현재 베이크와 일치했다. 검사용 풀은 실제 풀의 Acropolis 중복 없이 생성하며 실제 풀·설정 파일을 수정하지 않았다.

마지막 짧은 PIE 초기화에서는 Acropolis가 slot 1/5에 배치됐고 8개 slot 검증과 seam delta=0을 확인했다. 실행 중이던 `LNP.Spawn.ConfigOverride`는 `DA_MassSpawnConfig_TestOnePerSpecies`, EnemyDensity는 1이었다. 이 기존 편성 설정은 유지했다. Pod는 requested/placed=120/120, generic authored=98, random=22, 적은 810/810 배정됐고 스폰 큐 완료를 확인했다. unused generic authored=8이 있으므로 전체 지하 Pod의 직접 시각 확인으로 확대하지 않는다. 다섯 Acropolis Pod의 높이 적격성은 위 GUID별 저장 DA 검사로 확인했다. 일반 PIE 로그에는 이전 사용자 플레이에도 있던 Mass의 CharacterMovementComponent 추출 오류가 있으며 이번 형상 변경의 해결 범위에 넣지 않았다. PIE는 종료하고 `TestMap03`과 Acropolis DA를 선택한 상태로 인계했다.

검사 보고서·배치·검토 이미지는 현재 `.blend` 옆에 복사했다. 자세한 로그와 초기화 기록은 `Saved/SliceOctant/Acropolis_00/V009/`에 있다. 이번 과정에서 다른 세션이 수정한 C++·공통 문서는 변경하지 않았다.

섬 교차·원본 경사·광선 검사는 실제 Mover 캡슐 통행과 관성 착지를 대신하지 않는다. 훅 위치가 같아도 넓어진 섬 외피 주변의 접근·착지는 다시 PIE로 확인한다. 독립 동굴 몸체의 의도된 암반 내부 겹침은 유지했으며 전역 Boolean 통합 외피 검사를 선언하지 않는다.

V006은 확대 섬의 교차 검사에서 제외했다. V007은 깊어진 동굴 외곽이 int16 좌표 상한을 넘어서 제외했다. V008은 동굴을 안쪽으로 옮긴 직선형 후보였고, 입구 방향 변경을 허용한 후 V009에서 굽이와 중간 천장을 적용했다. 이전 후보와 V005 복구본은 `Saved/SliceOctant/Acropolis_00/`에 보존한다.

## 다음 사용자 검토

`TestMap03`에서 PIE를 시작한다. 먼저 `LNP.Debug.TeleportToReview AcropolisUpper_03`으로 2단 섬의 훅 도착·경사로·다음 이동을 확인한다. `AcropolisUpper_02`는 ㄱ자 섬이다. 동굴은 `LNP.Debug.TeleportToReview AcropolisPocket_02`로 들어가 Pod 확인 후 굽이→덮인 경사로→지상까지 왕복한다. 문제 위치는 `LNP.Debug.WhereAmI`와 재현 동작을 기록한다.

V005 전체 구성·훅 배치의 사용자 확인과 V009 변경 구간의 사용자 확인은 구분한다. V009의 실제 조작·교전 검토는 아직 남아 있다.
