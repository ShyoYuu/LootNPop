# Phase 9 로그 — 부유섬·동굴·입체 지형 Vertical Slice

## 2026-10-12 V008 — Development 격자 정밀도 수정·재패키징, 1P 잔여 실패 분리

- V007 Bastion Nav `(326,270)`을 같은 저장 payload/17자리 방향으로 재현했다. Editor 성공/Game 실패, Hollow Layer5 `(326,287)`도 Game에서 실패했다. Support `(244.5,202.5)` 경계의 위쪽 삼각형에만 NeedsExact가 있어 FP 재배치가 보간 가능 여부를 바꿨다.
- `ComputeCell`만 MSVC precise로 고정하고 전역 `/fp:fast`·세 꼭짓점 안전 규칙·에셋/schema를 유지했다. 새 `Runtime.SupportPrecision`의 Development 게임 회귀가 전체404,414Nav 노드와 인접NeedsExact 거부에 성공했다. 최초 테스트 flags 선언 오류 정정 후 빌드 성공, 최종Game30.23초·Editor26.06초다.
- 새 프로세스 전체126검사 통과(122성공+4경고 포함 성공·실패/미실행/진행 중0),492.086초. production 최신성/결정론266.359초·오류0·HTTP경고1, 저장5stream/sourcehash 일치로 재베이크하지 않았다. 경고4개는HTTP3/BOption1이며 엔진 시작 경고도 보존했다. 테스트 생성 에셋3개는 실행 전 실제 바이트로 복구했고 production3에셋/설정 SHA256 유지다.
- JSON 선행 게이트 뒤 Win64 Development BuildCookRun 성공exit0/64.649초·1229packages. 출력 `Saved/SurfaceNavigationPhase9V008Package`, 실제exe SHA256 `51B4E8DCD1E48EC88421B32AF0DF229A5B051335CD483AE34AABDDDEEA1797D5`. production3LVI/3SurfaceData·정식SpawnConfig cook 포함, 기존Lyra ShaderMap 경고 유지다.
- 동일 실제패키지1P가Nav graph를 생성하고generation1/8slot/358bindings를 게시했다. 패널11개/face2000방향/SurfaceData8/8 PASS, UnknownHits/EnvelopeEscapes/LayerJumps0·assert/ensure/crash표식0·자동종료exit0/43.575초다. 관통진단0이므로 관통Gate를 통과한 것으로 기록하지 않는다.
- **1P전체는 미통과다.** 기존 SourceKeys probe는 모든 slot의 source가 같아야 한다는 가정으로 혼합 풀에서 FAIL(중복0). 적2235요청/2130배치(105부족), exact P95 4.604ms>2ms FAIL도 확인했다. 기존CMC추출오류1/ShaderMap오류6 재현. 새 실패를 숨기거나 source검사를PASS로 치환하지 않았다.
- [V008](../phases/Phase09_NavPrecisionV008.md)·Current·단위5·격자/검증설계를 갱신했다. 근거는 `Saved/SliceOctant/NavPrecisionV008`에 실패/수정후 보고서·빌드/cook/1P로그·CSV·복구해시를 보존했다. 다음은 사용자 확인 뒤 SourceKeys 혼합풀 대조와105마리 부족 수정 단위다. 이후2P/late join은 다시 진행 확인한다. 별도NPC LoS와성능/메모리Gate 소유권 유지.

## 2026-10-12 V007 — cook 최신성 차단·Development 패키징 통과, 패키지 1P 실패

- Editor 전체 빌드 최신 상태 성공1.66초. production3source 최신성·저장5stream/두 차례 베이크 결정론 성공268.343초/테스트 오류·경고0. V006 고유125검사를 전체 회귀 근거로 유지했다.
- 별도 프로세스의 반지름만30000→30001cm로 바꿔 `OctantBakeDeterministic` 음성 검사를 실행했다. Crust/Regression fixture·Meadow의 저장 source hash/Support 불일치6개, 실패1/27.675초. 프로세스exit0이어도 JSON 실패를 판정해 로컬 실행기가UAT 호출을 차단했다. DefaultGame.ini SHA256 유지, 저장 에셋 무변경. plain BuildCookRun에 validator를 추가하지 않았다.
- 정식 설정 Win64 Development BuildCookRun 성공exit0/136.925초(UAT내부135.50초), Editor/Game 빌드63.57초·1229packages cook·stage/pak/archive 완료. production3LVI/3SurfaceData·정식SpawnConfig cook 포함 확인. 출력 `Saved/SurfaceNavigationPhase9V007Package`, 실제exe SHA256 `F6DDAB4C294AA8D6F608F1F9116DF5618796406B781A52AF14A7F95E9B05F743`. 기존 Lyra 누락 Material Function·ShaderMap 경고 재현.
- 패키지1P가 Meadow2/Bastion3/Hollow3의8slot을 로드하고716source/미분류blocker0을 등록했지만 `Nav slot 1 graph: Nav Layer 1 node (326,270) has no Support`로 snapshot 게시 실패. face/source/SurfaceData probe까지 도달하지 못했다. 초기화 대기 중인 소유 프로세스PID7768만 종료했으며exit−1은 이 종료 결과다. assert/ensure/crash 표식은 종료 전 로그에 없지만1P를 통과로 기록하지 않는다.2P·late join·성능·메모리는 미실행.
- 컴파일 응답 파일의Editor `/fp:precise`·Game `/fp:fast` 차이를 확인했다. `BuildAssetGraph`의 정규화 방향→`QueryLayer`/`ComputeCell`의격자floor·삼각형 선택 경계에서 판정이 달라지는 것이 유력한 후보이나, 실제 해당 노드의 양쪽 좌표/flags 비교 전이므로 원인 미확정이다. 코드·에셋·기본 설정 수정은 하지 않았다.
- [V007](../phases/Phase09_CookPackageV007.md)·Current·단위5 체크를 갱신했다. 로컬 근거는 `Saved/SliceOctant/CookPackageV007`. 검증 결과를 보고하고 별도 Nav 경계 수정 단위 진행을 사용자에게 확인한다. 기존 NPC LoS 세션 소유권과 나머지 Phase9 Gate를 유지한다.

## 2026-10-12 V006 — 정식 Spawn 편성 정정·fixture 회귀 정리 완료

- 사용자 에디터 종료 후 전체 빌드 성공36.60초/컴파일 오류0. 수정한 VolumetricFixture 컴파일·링크 완료, `SpawnConfigV006/full-build.log` 보존. 재연결 후 최종 검증을 마쳤다.
- 사용자 확인으로 ActorMix는 종류별 한 마리 테스트 편성, 정식 편성은 `DA_MassSpawnConfig`임을 정정했다. 사용자 복원 설정을 디스크·에디터에서 확인했고 override는 비어 있다.
- 기존 실패6개 재검사3통과/3실패126.378초. `Bench01_2` 미등록은 해소됐으며 남은 실패는 fixture2개의 저장 source/Spawn stale과 실제 비행9/테스트 기대3이다.
- 테스트 에셋을 `DA_MassSpawnConfig_TestOnePerSpecies`로 이름 변경·저장했다. 기존 참조0·옛 경로/redirector 없음·모든 편성 속성 보존을 확인했다. 정식 편성은 수정하지 않았다.
- Crust/Regression fixture2개를 복구본 보존 후 재베이크·저장했다. VolumetricFixture의 실제 공동 Pod 기대 수량은 정식 config 요청값을 읽도록 수정했고, 양수 편성과8slot authored Pod 검사를 유지했다. Meadow는 최초 정식 검사에서도 최신성 통과여서 추가 변경하지 않았다.
- 새 에디터 전체125검사124통과/1실패454.154초. 실패한 ConditionalPatch.RegressionFixture는 별도 Conditional 에셋의 저장본을 검사하므로 최초 대상 누락을 확인하고 이 에셋도 복구본 보존 후 재베이크했다. 영향 범위 ConditionalPatch7검사 전부 통과32.866초. 두 배치의 최신 결과를 합친 고유125검사 통과이며 단일125통과 실행은 아니다. 실행 MCP300초 timeout 후에도 자동화는 계속됐고 완료 결과를 GetTestResults로 확보했다.
- production 최신성/결정론248.666초·runtime32seed5.010초·VolumetricFixture0.353초 모두 오류·경고0. 전체 배치의 RHI 가상 리소스/HTTP/BOption 경고는 보존한다. 결과는 `full-results-before-conditional.json`·`conditional-results.json`·`combined-results.json`이다.
- 자동화 테스트 에셋3개 unload→HEAD 바이트 복구→SHA256 확인. dirty COption BP reload·임시 월드 GC·TestMap03 복귀 후 dirty content/map0, diff check 통과. 테스트 편성 속성 재확인·정식 설정/빈 override 유지. 별도로 변경된 `.gitignore`와 기존 InputHandler 변경은 보존했다. [V006](../phases/Phase09_OfficialSpawnConfigV006.md)·Current·단위4 체크리스트를 갱신했다. 이번 단위를 완료하고 사용자에게 다음 cook·패키지 단위 진행 여부를 확인한다.

## 2026-10-06 단위 0 — 실행 계획과 기준선

- 사용자 결정: 품질 기준은 실규모 레벨 디자인 블록아웃, 새 옥탄트 2개(지하 공동 입구 조합을 서로 다르게), 메시 생성·배치는 Claude, 사용자는 PIE로 검토한다.
- 기준선: 옥탄트는 `Meadow_00` 1개이고 아트 에셋은 없다. 풀 선택은 slot 순서 greedy다(전체 slot 허용 definition만 쓰면 D-043 선행 불필요). `Guide_OctantLevelInstance.md`는 25,000cm 시절 절차라 단위 1에서 갱신한다.
- 실행 문서 `phases/Phase09_VerticalSlice.md`를 만들고 Current·Roadmap을 착수 상태로 갱신했다. 코드·에셋 변경과 빌드는 없다.

## 2026-10-06 단위 1 — 제작 파이프라인 확정(평면 구성 확인 대기)

- 이음매 검사 확인: 런타임 `ValidateSeams`는 이웃 slot 이음매 샘플의 양자화 반지름·Valid 비트가 비트 단위로 같아야 게시한다. `BP_OctantGenerator`로 시드만 바꾼 새 지각은 Meadow와 섞일 수 없다(fixture 지각 seam hash `0f671c69…`도 Meadow `eb7e9631…`와 다르다).
- 코드 정의 빌더 `LNP.SurfaceNav.BuildSliceOctant <Name>`(`LootNPopEditor/SurfaceNavigation/LNPSliceOctant.cpp`)를 추가했다. Meadow 지각 MeshDescription을 읽어 이음매 평면 15m 안 정점은 그대로 두고 그 바깥 40m에 걸쳐 시드 fBm 목표 지형(Pad로 구조물 자리 평탄화)으로 넘긴다. 2층 건물(20×14m, 문 300cm 둘, 30° 경사로 + Decoration 계단 23칸)을 C++로 만들고 `/Game/Maps/<Name>/Authored` 메시를 접미사(`_Floor`·`_Coarse`·`_Shell`·`_Deco`)로 배치한다. 기존 LVI는 액터를 지우고 같은 패키지에 다시 채운다.
- Blender `Scripts/Blender/GenerateSliceOctant.py`: 옥탄트 로컬 좌표(피벗 = 옥탄트 원점)로 바위 메사(반경 9m, 윗면 지각 위 약 22m)를 만들고 Floor/Shell로 나눠 FBX로 냈다. UE 임포트 bounds 중심 (16182, 10551, 19961)이 계산값 (16171, 10471, 19903)과 맞는다. 메시 데이터 블록이 남으면 이름에 `.001`이 붙어 생성 전에 비운다.
- 베이크 `Bastion_00`: Support source 3·Layer 5(지각, 경사로, 2층 바닥 반지름 29,550, 지붕 29,100, 메사 윗면 27,650~27,800), split=0, portal 4, NeedsExact 0.03%, Nav components 99. `Hollow_00`(지각만): Layer 1, NeedsExact 1.79%.
- seam hash: 두 옥탄트 모두 세 변이 Meadow와 60바이트 일치. PIE(TestMap03, 리슨 2P 설정 그대로)에서 Meadow 2·Bastion 3·Hollow 3 조합이 `Published generation 1 with 8 validated slots`, `maxSeamRadiusDelta=0.000cm`, `minSeamNormalDot=0.95296`.
- 지각 Valid 271,174/271,216(Invalid 42)은 새 지각 문제가 아니다. 무변형 Meadow 복사본과 Meadow 원본 재베이크도 같은 값이며(4a 시점 기록은 271,216), Meadow 재베이크 결과는 저장본과 같아 git 변경이 없다.
- Live Coding: 기존 함수 본문 수정은 반영되지만 함수 안 `static` 테이블은 다시 초기화되지 않았다(spec 임시 변경이 무시됨). 옥탄트 spec을 호출마다 만드는 함수로 바꿔 레이아웃 반복을 Live Coding으로 할 수 있게 했다. 새 콘솔 명령 추가만 전체 빌드가 필요하다.
- `OctantPoolData`에 `Bastion_00`·`Hollow_00` definition(전체 slot 허용)을 추가했다. 현재 두 옥탄트의 구조물은 파이프라인 시범이며 단위 2에서 §3 구성으로 바꾼다.

## 2026-10-07 단위 2 — Bastion_00 제작(사용자 PIE 검토 대기)

- 사용자가 §3 구성과 이름을 확정했다. 사용자 지시: 지형을 그래플 거리에 맞추지 않는다. 지형과 재미가 먼저이고 앵커는 이동 수단이다.
- 앵커별 그래플 최대 거리: `ALNPPlacementMarker::GrappleMaxDistance`(0이면 `DA_WorldDeviceConfig` 값)를 `ALNPGrappleAnchor::MaxGrappleDistanceOverride`(Replicated, deferred 스폰 초기 번치)로 옮긴다. `CanInteract`도 `GetMaxGrappleDistance()`를 쓰게 바꿨다. 헤더 변경이라 전체 빌드했다(62.8초 성공).
- 빌더 spec을 구조체 묶음(건물 문·층수·지붕, 성벽 고리, 나선탑, 원판 섬, 공중 경사로, 공동·긴 경사로·수직 통로·기둥, 앵커, Pod, 패널, 바위)으로 넓혔다. 좌표는 옥탄트 공간 `Point(X, Y, H)`이고, 같은 X·Y 선이 구 중심을 지나는 평면 위에 있어 지각 절단 평면(`ColumnPlanes`)과 벽이 정확히 맞는다.
- Bastion_00: 건물 5(B1 2층, B2 3층, B3 긴 홀 문 3, B4 지붕 없는 폐허, 공동 안 C1 2층), 성벽 R62m·높이 8m·문 4·안쪽 경사로 2, 나선탑 24m 1.4바퀴, 섬 I1(북문 위 공중 경사로)·I2(탑 꼭대기 셔틀 패널), 공동 70×50m·바닥 40m 아래·천장 18m(CoarseSupport)·기둥 3, 서문 밖 긴 경사로(구면 30°, 69m)·광장 수직 통로 S1. 앵커 7(S1 위쪽 하나 40m), Pod 앵커 8, 바위 28. Mesa01은 Blender로 (85, −55)m에 다시 만들어 재임포트했다.
- 빌더 결과: 지각 삼각형 283개 절단, 메시 액터 28, 공동 지붕 22m, 경사로 통로 최소 지붕 4.2m.
- 첫 베이크는 메사 꼭대기 Pod 앵커가 `clearance 69.1cm (requires 250cm)`로 막았다. 윗면 반경 약 7.6m라 Pod 자리로 맞지 않아 뺐다.
- 베이크: Support source 14·Layer 27, Valid 271,070/271,216(입구 절단 구멍만큼 감소), NeedsExact 0.09%, Nav components 74·portal 61, 수동 Pod 앵커 8 투영 성공. seam hash 세 개가 Meadow와 바이트 일치.
- PIE(TestMap03, 리슨 2P): slot 0·3 Bastion, 1·4·7 Meadow, 2·5·6 Hollow. 호스트·게스트 모두 `Published generation 1 with 8 validated slots`, `maxSeamRadiusDelta=0.000cm`, runtimeLayers 90, 마커 요소 34개 스폰(0 skipped).
- slot별 선택 옥탄트를 `LNPOctantSpawnSubsystem: slot N (rotation …) <- LVI_…` 로그로 남기고, 레벨 검토용 `LNP.Debug.TeleportToOctant <Slot> <X> <Y> [H]`(서버, 전원 리스폰)를 추가했다. 새 콘솔 명령이라 전체 빌드가 필요하다.

## 2026-10-07 단위 2 반복 1 — 거인의 성채로 개편

- 사용자 첫 PIE 피드백: 구조물이 아기자기하다. 폭은 5배, 성벽은 몇 배 높게 여러 단으로 쌓고 계단을 많이 둔다. 높이·깊이를 크게 쓴다(기준 지면 위 200m·아래 100m, 이후 옥탄트마다 +240/−150, +180/−130도 시험). 닫힌 건물은 줄이고 대부분 1~2면이 무너진 폐허로, 지붕에 오를 수 있게 한다. 건물·다리·섬을 빽빽하게 얽고 모든 건물·다리에 앵커를 둔다. 옥탄트마다 멀리서도 보이는 메인 무대(보스 자리) 하나를 둔다. 나선 기둥과 성벽에서 다리로 잇는 섬은 유지한다.
- 물리 한계를 확인했다. Mover 낙하 종단속도 2,000cm/s라 20초 낙하는 약 390m가 필요하다. 사용자는 "느낌의 예"라고 정리했다. 옥탄트는 구 중심이 꼭짓점인 원뿔이라 높이 H에서 실제 크기가 (300 − H)/300배다. 같은 (X, Y)를 따라 세운 벽은 중력과 나란하다.
- 빌더를 다시 썼다. 새 구조물: 계단식 동심 성벽(`FTieredRing`, 띠 윗면 통로·노출 면 벽·관통 문·경계면 경사로·20cm 계단 칸), 나선 기둥(`FSpire`, 축에서 실제 거리 유지, 높이별 메시 분할), 심연과 원형 공동(`FAbyss`, 지각을 48각 기둥 평면으로 절단), 고리 섬, 실제 폭 다리, 무너진 면이 있는 거인 스케일 건물(층고 8m, 문 7×6.5m, 경사로 폭 6m).
- Bastion_00 구성: 메인 무대는 150m 상공 고리형 아레나(실제 반지름 40m, 엄폐 석주 6). 중앙 나선 기둥은 공동 바닥 −100m에서 195m까지 약 7바퀴(폭 9m, 25°)이고 아레나 구멍을 지난다. 심연(반지름 32m)이 공동(실제 반지름 약 60m, 천장 −65m, 기둥 6)으로 떨어진다. 성채 4단(20/40/60/80m, 문 4개 14×18m, 단마다 경사로 2), 외성벽 2단(20/40m, 문 4개 20×30m, 경사로 8), 안뜰 큰 건물 8(5채 폐허)과 탑 4, 공중섬 12개가 외성벽 40m에서 아레나 150m까지 나선으로 오른다(2개에 폐허). 다리 43, 앵커 95(모든 건물·다리·섬, 아레나 둘레, 기둥 꼭대기, 심연, 성채 단), Pod 12, 셔틀 패널 1(성채 꼭대기 ↔ 섬 5).
- 메사 Authored 메시는 안뜰과 겹쳐 지웠다(Blender `MESAS["Bastion_00"]` 비움).
- 첫 베이크는 성채 60m 단 Pod가 `clearance 239.9cm (requires 250cm)`로 막혀 20m 단으로 옮겼다. 60m 이상 단은 실제 폭 약 8m라 Pod 자리로 좁다.
- 베이크: Support source 78·Layer 153, 지각 NeedsExact 1.47%(심연 절단), SupportPayload 11.6MB(이전 4.2MB), Nav cells 133,610·components 345·portals 1,635, 수동 Pod 앵커 12. seam hash Meadow와 일치.
- PIE(1P, `LNP.Spawn.EnemyDensity 0` 에디터 세션 임시값으로 적 0·Pod 유지): slot 2·5 Bastion, 8 slot 게시, `maxSeamRadiusDelta=0.000cm`, runtimeLayers 342, navResident 18.56MiB, 마커 요소 210개 0 skipped.

## 2026-10-07 단위 2 반복 2 — 메인 무대 접시·외성벽 바깥 단·섬 폐허 개방

- 사용자 피드백: 스케일·밀도·높이·이웃 옥탄트 거리감 모두 만족. 도넛 아레나는 어색하다 → 기둥 꼭대기가 나팔처럼 벌어진 접시를 메인 무대로. 엄폐 석주는 유지. 섬 위 폐허가 섬을 꽉 채워 다리 쪽 통행을 막는다. 외성벽 바깥에도 단을 둔다. 천장 아래(심연 포함)가 너무 어둡다.
- `FSpire`에 접시(메인 무대)를 더했다. 꼭대기 180m 윗면은 높이 일정 구면(실제 반지름 40m)이고, 나팔 면은 150m에서 나선 바깥 반지름으로 시작해 177m에서 접시 반지름이 된다. 나선은 나팔 안을 지나고 마지막 6m 오르막 위의 접시 칸을 비워 틈으로 올라온다. 경사로 분할은 0·90m(Layer 반지름 폭 한계).
- 도넛 아레나를 지우고 엄폐 석주 6개를 접시 위로 옮겼다. 공중섬 나선은 40m → 180m, 위로 갈수록 바깥(호 92 → 128m)으로 퍼져 나팔·접시를 피하고 마지막 섬이 접시로 건넌다. 폐허 섬 반지름 18m, 폐허 18×14m에 다리 쪽 두 면(±Y)을 무너뜨렸다.
- 외성벽에 바깥 20m 단(호 128~136m)과 바깥 계단 8개를 더했다. Pad를 145m로 넓혔다.
- 베이크: Support source 77·Layer 164, SupportPayload 12.3MB, Nav cells 140,923·portals 1,636, seam hash 일치. PIE 1P: slot 0·5·7 Bastion, 8 slot 게시, runtimeLayers 527, 마커 요소 306개 0 skipped. 개편 전 생성 메시 13개를 지웠다.
- 어두운 지역은 조명 방식 선택을 사용자에게 물었다(지형 조명 배치 추천, 자동 노출·플레이어 랜턴·환경광 하한은 대안).

## 2026-10-07 단위 2 반복 3 — 등불·플레이어 랜턴·섬 폐허 되돌림

- 사용자 결정: 어두운 지역은 A(지형 등불) 기본 + C(플레이어 랜턴) 보조. 메인 무대 접시·공중섬 나선·외성벽 바깥 단 만족. 섬 폐허는 섬을 꽉 채우던 크기가 낫고, 다리 쪽 두 면이 뚫려야 하며 턱이 없어야 한다(점프로 1.5m 잔해 턱을 넘지 못함). 계단은 다리 방향이 아니라 옆면에.
- 등불: 빌더가 `FLamp`마다 그림자 없는 Movable 포인트 라이트(cd 단위, 따뜻한 색)와 발광 구슬(`/Game/Maps/SliceOctant/M_SliceLamp`, Unlit 발광, Decoration)을 둔다. Bastion 99개 — 심연 벽 고리 3단×6, 공동 천장 둘레 8·기둥 밑동 4, 접시 나팔 안 8, 성채·외성벽 문 터널 24, 건물 층마다 천장 밑 37.
- 랜턴: `ALNPPlayerCharacter::LanternLight`(머리 위 150cm, 3,000cd, 반경 15m, 그림자 없음). 헤더 변경으로 전체 빌드(48.3초 성공).
- 무너진 면은 잔해 턱 대신 양 모서리 3m 벽 토막(벽 높이 55%)만 남긴다(안뜰 폐허 포함). 섬 폐허는 섬 반지름 14m·폐허 18×16m로 되돌리고, 로컬 X를 앞 섬→뒤 섬 방향으로 두어 무너진 ±X 면으로 두 다리가 들어오고 경사로는 -Y 옆벽을 따른다.
- 베이크: Support source 77·Layer 164, SupportPayload 12.2MB, Nav cells 140,352·portals 1,634, 수동 Pod 앵커 12. PIE 1P(적 0): slot 1·5·6 Bastion, 8 slot 게시, 마커 요소 306개 0 skipped. 공동 바닥 캡처에서 나선·벽·등불이 식별된다.

## 2026-10-07 단위 2 반복 4 — 레벨 구성 확정·등불 구슬 Lumen 제외

- 사용자 판정: 섬 폐허·턱 제거 등 레벨 구성은 충분하다. 등불은 지형 식별 기준으로 적당하다.
- PIE 화면에 `Lumen surface cache atlas oversubscribed`(약 19%, 시점마다 변동)가 떴다. 아틀라스(기본 4096²)를 키우면 VRAM이 약 4배라 가급적 피하고, 먼저 수요를 줄인다. 등불 구슬에 `bAffectDynamicIndirectLighting = false`를 줘 Surface Cache 카드에서 뺐다(빛은 포인트 라이트가 낸다). 카드 밀도(`CardTexelDensityScale`)·아틀라스 크기는 단위 5 실측 뒤 판단한다.
- Live Coding 후 재생성·재베이크: 수치는 반복 3과 같다(Layer 164, SupportPayload 12.2MB). PIE 1P: slot 2·5·7 Bastion, 8 slot 게시.
- 사용자 관찰: 등불 근처를 지나는 캐릭터 표면이 너무 밝다. 원인은 메테리얼이 아니라 역제곱 감쇠다(벽 6m 약 420 lux, 1.5m 캐릭터 약 6,700 lux). 사용자 요청으로 감쇠 곡선만 먼저 바꿔 체감을 본다: 등불을 역제곱 없이 `(1 − (d/R)²)^4` 감쇠(unitless)로 두고, 세기는 반경의 25% 거리에서 spec cd 값의 역제곱 조도와 같게 환산했다. 등불 바로 옆 최대 조도가 그 값의 약 1.3배에서 멈춘다. 랜턴·지형 메테리얼은 그대로다.
- PIE 1P: slot 0·3·7 Bastion. 공동 바닥 캡처에서 캐릭터가 하얗게 튀지 않는다.

## 2026-10-07 단위 2 반복 5 — 빈 구조물 갇힘 수정(두께 있는 벽)

- 사용자 보고: 벽에 붙어 걷다가 틈처럼 보이는 곳으로 들어가면 빠져나오지 못한다(2회). 좌표를 옥탄트 로컬로 환산하니 성채 바깥 단(호 65.8m·108°)과 외성벽 바깥 단(호 135.5m·38.3°, 바깥 경사로 시작점 37.8° 옆) 내부였다. 두 번 다 바깥에서 벽에 붙어 경사로 밑동으로 올라서던 중이었다.
- 원인: 성벽 단은 속이 빈 껍데기이고 벽은 두께 0인 양면 삼각형이었다. 턱 오르기 sweep이 벽에 살짝 겹친 채 시작하면 Chaos trimesh가 겹침을 놓치거나 뒷면 법선을 내 벽을 통과한다(Phase 14 벽보행 관통과 같은 함정). 띠 윗면은 위로만 그려져 속에서 천장이 투명했고, 그 너머 계단·앵커 프롬프트가 "틈"으로 보였다.
- 수정: `Wall`에 솔리드 쪽(`SolidSide`)을 받아 `WallThickness`(실제 100cm) 두께의 닫힌 판(앞뒤·양 끝, 필요할 때 윗면·밑면)을 만든다. 성벽 경계면·문설주·경사로 옆면·끝면, 섬 둘레, 심연·공동 벽(심연 테두리 50cm 턱은 윗면 마감), 공동 기둥, 나선 기둥 몸통(안쪽 면 추가)에 적용했다. 건물 외벽은 바깥쪽으로 두께를 주고(방 크기 유지) ±Y 벽을 두께만큼 늘려 모서리를 채웠다. 성벽 띠 윗면 50cm 아래에 밑면을 붙였다.
- 재생성: partTris 118,946 → 138,790. 베이크: Layer 164, SupportPayload 12.2MB, Nav cells 137,956(이전 140,352)·components 398·portals 1,580. PIE: slot 1·3·7 Bastion. 벽에 붙어 경사로 밑동을 오르는 재현은 사용자 확인 대기.
- 사용자 확인: 경사로 입구 구멍은 없어졌다. 추가 요청 두 가지 — 경사로 끝에서 단 위로 이어지는 평지가 없어 불편하다(경사로 폭과 같은 정사각형 평지), 외성벽 20m 단 위 40m행 경사로가 단 폭 8m 중 6m를 차지해 경사로를 피해 단을 돌 공간이 캐릭터가 겨우 지날 정도다.
- 수정: 모든 성벽 경사로 꼭대기에 경사로 폭과 같은 정사각형 층계참(높이 H1, 옆면·끝면은 두께 있는 벽)을 잇는다. 외성벽 안쪽 20m 단 11000~12400(폭 14m), 40m 단 12400~13400(10m), 바깥 20m 단 13400~14800(14m)로 넓혀 경사로 옆에 8m를 남겼다(안쪽 면은 안뜰 건물 때문에 유지). 경사로 시작 각도는 사분면 가운데에 오도록 계산한다. 첫 공중섬 다리·외성벽 Pod·문 터널 등불 반지름을 새 단에 맞췄다.
- 재생성 partTris 141,672. 베이크: Layer 164, SupportPayload 12.3MB, Nav cells 146,989·components 403·portals 1,731. PIE: slot 1·4·7 Bastion.
- 사용자 확인: 층계참 크기·단 폭 적절. 추가 요청 — 외성벽과 성채 사이 안뜰이 건물 하나로 꽉 차 답답하고, 그 건물들은 성벽 쪽으로만 트이고 통로 쪽은 막혔으며, 큰 건물·탑·큰 건물이 연달아 붙어 이동이 불편하다.
- 공간 여유: 외성벽 바깥 면은 좁은 방향(옥탄트 경계 쪽 45°·60°·180°·300°)에서 이음매까지 약 47m였고 이미 Meadow→평지 전이 띠(15~55m) 안이었다. Bastion만 `SeamBand`를 40m→20m로 줄이고(이음매 15m 안쪽 정점 불변, seam hash `eb7e9631…` ×3 일치 확인) Pad를 145m→160m로 넓혀 외성벽을 8m 밀었다(호 118~156m, 바깥 경사로 꼭대기도 경계에서 약 40m).
- 안뜰(호 76~118m, 폭 42m): 건물 12채(큰 건물 8·탑 4)를 45° 간격 8채(22.5°+45°k, 호 97m)로 줄였다. 건물을 90° 돌려 면 0·1(문·주로 무너진 면)이 통로 방향, 면 2가 외성벽, 면 3이 성채를 향한다. 둘레 26m × 반지름 22m. 다리는 이웃 지붕끼리 한 바퀴 8개 + 건물마다 외성벽 20m 단·성채 20m 단. 탑 생성 메시 12개를 지웠다.
- 재생성: parts 69·partTris 133,720·건물 10·다리 39·앵커 87·등불 86. 베이크: Support source 69·Layer 138, SupportPayload 12.1MB, Nav cells 147,376·components 332·portals 1,411. PIE: slot 1·4 Bastion, 8 slot 게시, 마커 요소 194개 0 skipped.
- 사용자 확인: 안뜰이 훨씬 나아졌다. 추가 요청 — 안뜰 건물 층고 1.5배.
- 안뜰 건물 층고 8m→12m(지붕 2층 24m·3층 36m, 1층 경사로 길이 약 21m로 건물 둘레 26m 안에 든다). 3층 지붕이 첫 공중섬(22.5° 근처, 바닥 35m)과 겹쳐 22.5°·67.5° 건물의 층수를 맞바꿨다. 3층 지붕에서 20m 단으로 가는 다리가 약 40°로 가팔라져 3층 건물은 외성벽 40m 단(호 137m)·성채 40m 단 바깥 4m 띠(호 62m, 경사로 호 54~60m를 피함)로 잇는다. 건물 등불은 층고 비율 K로 반경 ×K, 세기 ×K²(바닥 밝기 유지). 문 크기(7×6.5m)는 그대로.
- 재생성 partTris 137,484. 베이크: Layer 138, SupportPayload 12.4MB, Nav cells 148,082·components 338·portals 1,390. PIE: slot 2·4·7 Bastion, 마커 요소 282개 0 skipped. 3층 건물 1층 캡처에서 등불·두께 있는 문이 보인다.

## 2026-10-07 단위 2 완료 — Bastion 1차 확정과 제작 과정 개선

- 사용자 판정: 층고·다리 느낌 괜찮다. Bastion은 1차 마무리. 후기: 수정 한 바퀴(컴파일→생성→베이크→PIE 3~5분)는 빨랐지만, 경사로 옆 2m 통로·층계참 부재·1.5m 턱·두께 0 벽·섬 폐허 통로 차단처럼 숫자로 알 수 있는 규칙 위반을 사용자가 플레이로 찾았다. 위치 확인은 좌표를 읽어 손으로 환산했다. 사용자가 개선안 중 1(규칙 검사)·5(검토 도구)·6(치수표)만 채택했다. 2(베이크 성분 리포트)·3(평면도 자동 출력)·4(수치 파생)는 필요해지면 다시 본다.
- 치수표(PIE 실측): 캡슐 42/192cm, 턱 40cm, 걷기 경사 44.8°, 점프 140cm(750cm/s, 중력 2000). 규칙: 벽 두께 100cm 닫힌 판, 경사로 30°+층계참, 경사로 옆 통로 ≥300cm, 다리 ≤35°, 다리 끝이 경사로 위 금지, 위아래 구조물 틈 ≥400cm, 지면 구조물은 이음매 전이 띠 밖·Pad 안, 문 ≥250cm. 표는 `Guide_OctantLevelInstance.md` §6.
- 규칙 검사 `LintSpec`: 생성마다 실행해 로그 `lint=`에 더하고(두께 0 벽 수 포함), `LNP.SurfaceNav.LintSliceOctant <Name>`은 생성 없이 검사만 한다. 첫 실행에서 높이 180m 공중 다리(D36)가 평면상 외성벽 경사로 위라 걸린 오탐을 높이 조건으로 고쳤다.
- 검증: 현재 Bastion `lint=0`. 이번 세션 문제를 되돌린 변형(외성벽 단 폭 8m, 전이 띠 40m, 22.5° 건물 3층, 3층 지붕→20m 단 다리, 성채 경사로 위 다리 끝, 문 200cm, 기둥 두께 0)에서 7개 규칙이 모두 경고했다(Walkway 187cm ×8, BridgeSlope 42.5° ×5, Clearance B01-I01 −100cm와 옛 외성벽-건물 겹침 ×8, SeamBand 54.8m, DoorHeight ×20, ZeroThickness 24, BridgeOnStair ×3 — 마지막은 경계값 1cm 차이로 놓친 뒤 허용폭을 바닥 두께로 넓혀 확인). 변형을 되돌리고 재생성·재베이크(Layer 138, Nav cells 148,082, 이전과 동일).
- 검토 도구: 빌더가 spec 검토 지점을 에디터 전용 `ATargetPoint`(태그 `LNP.ReviewPoint` + 이름)로 둔다(Bastion 11개). 런타임 `LNP.Debug.TeleportToReview [Name] [Slot]`(인자 없으면 목록), `LNP.Debug.WhereAmI`(slot·LVI·로컬 X/Y/H·극좌표·발밑 메시). `TeleportToOctant`와 공용 프레임·리스폰 함수를 나눴다. PIE: 목록에 11개 × slot 2·5·6, `OuterLanding 5` 이동 후 WhereAmI가 slot 5·r=15901·deg=52.5·`SM_Bastion_00_W01_Coarse`를 출력했다(회전 slot에서 로컬 환산 일치).

## 2026-10-08 — 단위 3 Hollow 초기 검토 구역

- README·Current·Decisions·Phase 9 실행 계획·옥탄트 기술/레벨 디자인 가이드를 읽었다. 최신 레벨 디자인 가이드의 순서대로 대표 전투 구역부터 제작하고 사용자 PIE 검토 뒤 전체로 확장한다. 단위 3 완료로 표시하지 않는다.
- 복구본: `Saved/SliceOctant/Hollow_00/BeforeInitial`에 작업 직전 LNPSliceOctant.cpp와 Hollow 에셋을 복사했다. 기존 Bastion 수정은 보존했다.
- Hollow spec에 협곡 바닥 -30m, 반경 22m의 서쪽 +20m Pod 전투장과 동쪽 +40m 착지 암반, 폭 12m의 서쪽 진입 경사로, +180m·반경 40m 독립 아레나 실루엣 후보를 추가했다. 전체 상승 경로·공동·두 수직 입구·분기·추가 섬·동적 장치는 미제작이다.
- 두 전투 암반은 바닥 아래까지 닫힌 측면을 내려 벽보행의 측면→윗면 합류를 검토한다. 횡단 앵커 2개는 사거리 140m, 낮은 회피 앵커 4개는 60m, 바닥 재조준 앵커 1개는 90m, 아레나 검토용 앵커 1개는 200m다. 전역 훅 설정은 바꾸지 않았다.
- 지정 Pod 1개에 비행 4·벽보행 6 편성을 넣었다. 검토 지점 5개: HollowEntry·HollowPodWest·HollowLandingEast·HollowBasin·HollowArena.
- Live Coding MCP 컴파일 성공. 현재 MCP에 콘솔 실행 API가 없어 AGENTS.md의 수동 조작 우선 규칙에 따라 사용자가 BuildSliceOctant·BakeOctant를 실행했다. 별도 도구나 콘솔 명령을 추가하지 않았다.
- 생성: 저장 성공, parts=4·partActors=8·partTris=4018, bridges=1·islands=3·anchors=8·spawns=1·reviewPoints=5, authoredErrors=0·lint=0. 지각 237,841 tris·moved=105,556·kept=13,988·cut=0.
- 베이크 및 저장 성공: Support sources=5·Layers=5·TotalSamples=929,907·SupportPayload=6,528,186 bytes. Nav cells=69,007·components=597·portals=18. Spawn authored=1·candidates=10,123.
- MCP로 TestMap03 PIE 시작: Hollow slot 1·4·7, Meadow/Bastion과 혼합한 8-slot snapshot 게시 성공, maxSeamRadiusDelta=0.000cm·minSeamNormalDot=0.95296. 마커 314개 생성·0 skipped. EnemyDensity=1 확인. PIE를 사용자 검토용으로 켜 두었다.
- 제한: 전체 월드 적 배치는 Bench01 540 요청/531 배치, Bench01_2 1080/1056, 나머지 세트는 전량 배치(총 33기 부족)다. 직전 PIE에도 전체 적 배치 부족이 있었지만 이번 변경과의 인과는 확인하지 않았다. 지정 Hollow Pod의 개별 NPC 수·절벽 합류·훅 시야·캡슐 이동·재조준과 전투 감각은 수동 검토가 남았다. 새 자동화 테스트는 만들지 않았다. git diff --check 통과.
- 다음: 현재 PIE 서버 콘솔의 `LNP.Debug.TeleportToReview HollowPodWest 1`에서 횡단·착지·Pod 회피와 복귀를 확인한다. 같은 slot의 HollowLandingEast·HollowBasin에서 역방향과 낙하 복귀를 확인한다. 피드백으로 크기/간격을 조정한 뒤 전체 탐색 순환·수직 통로 공동·동적 장치로 확장한다.

### 2026-10-08 — 초기 훅 확인과 웅장한 협곡 요청

- 사용자 확인: 협곡 바닥→한쪽 원기둥 위→반대쪽 원기둥 위 훅 이동 성공. 비행 전투·벽보행 합류·역방향/낙하 복귀까지 확인한 것으로 확대 해석하지 않는다.
- 사용자 피드백: 협곡이 더 웅장했으면 좋겠으며 옥탄트 크기가 제한인지 질문했다. 지형은 아직 변경하지 않았다.
- 현 spec의 구면 투영 실측 계산: 두 선반 중심의 월드 직선 거리 약 100.9m. 로컬 X ±85m·H=-30m인 바닥 끝점 사이 직선 거리는 약 179.9m(평탄화 자리의 실제 외곽 전체 폭을 뜻하지 않는다).
- 확대 여유 계산: 중심 로컬 X ±120m·Y ±80m 직사각형의 네 모서리는 H=0에서 이음매 평면과 최소 약 61.0m 떨어져 Hollow의 15+40=55m 보존/전이 띠 바깥이다. X ±140m·Y ±65m는 최소 52.4m로 일부가 전이 띠에 들어가므로 그대로 평탄화할 수 없다. 생성 메시 전체·노이즈·falloff와 충돌 검사를 대신하는 결과는 아니다.
- 제안 방향(미구현): 단순 원기둥을 길게 이어진 절벽/암반 선반으로 바꾸고 협곡 바닥 -60m·선반 +60/+100m 정도를 시험한다. 바닥 로컬 X ±100m·H=-60m 끝점 사이 직선 거리는 약 227.7m다. 수평으로만 키우기보다 연속 절벽과 120~160m 높이차로 규모감을 만든다. 기존 성공한 훅 연결의 역할은 유지하고 필요한 거리는 마커별로 조정한다. 옥탄트 반지름 확대는 제안하지 않았다.

### 2026-10-08 — 반지름 고정·연속 절벽 확대

- 사용자 명시: 반지름 확대는 후보가 아니다. 월드 반지름 300m 안에서 깊이·높이차로 규모감을 만들며 제작을 계속한다. 설정·Meadow·Bastion은 변경하지 않았다.
- 작업 직전 복구본: `Saved/SliceOctant/Hollow_00/BeforeCliffs`에 source와 Hollow 에셋을 복사했다.
- 바닥 -60m·서쪽 +60m·동쪽 +100m, 두 원기둥을 길게 이어진 볼록 암반으로 대체했다. FCliff/BuildCliff는 구면 윗면과 닫힌 밑면·옆면을 생성한다. W01/E01 기존 Floor/Shell 이름을 재사용한다. Pod는 서쪽 -2500/-5400/+6000으로 이동하고 비행 4·벽보행 6 편성은 유지한다. +180m 아레나 후보도 유지한다.
- 서쪽 절벽 바깥 보행 경사로와 선반 진입 층계참(폭 12m), 횡단·회피·절벽 길이 방향 앵커 13개, 검토 지점 6개(HollowWestLanding 추가)를 둔다. 주요 횡단 앵커는 윗면보다 25m 높고 사거리는 서쪽 180m·동쪽 220m다. 전역 그래플 설정을 바꾸지 않았다.
- 제작 좌표 계산: 두 외곽은 볼록하고 지면 기준 이음매 평면과 최소 W01 80.9m·E01 78.2m다. 밑면의 좌표 성분 최대 절댓값은 각각 약 29644cm·29374cm로 int16 캡 안이다. 바닥 검토 위치 1m 위→서쪽/동쪽 앵커의 월드 직선 거리는 약 147.4/191.1m, 서쪽 검토 위치→동쪽 앵커는 약 94.6m다. 동쪽 중앙에서 서쪽으로 쏘는 선은 자기 암반을 통과하므로 하강 출발은 안쪽 가장자리다.
- Live Coding 성공, 사용자 첫 재생성/베이크 확인: saved·lint=0·parts=5·partActors=10·partTris=36120·bridges=2·islands=1·cliffs=2·anchors=13·spawns=1·reviewPoints=6·authoredErrors=0. 지각 반지름 범위 29434~36000cm(월드 기준 반지름은 30000cm).
- 첫 베이크: Support sources/Layers=6·TotalSamples=1148833·SupportPayload=8067375 bytes. Nav cells=74715·components=401·portals=84. Spawn authored=1·candidates=10285. 저장 성공.
- 실제 LVI를 열어 MCP Visibility 선 추적: Pod·동쪽 선반·바닥·입구·층계참 및 선반 접합부 바닥을 검출했다. 층계참 중앙 경로 11개 지점은 모두 바닥을 검출하고 높이 오차 약 1cm 이내, 높이 1m의 횡단 선은 열렸다. 바닥→서쪽·바닥→동쪽·서쪽→동쪽·동쪽 안쪽 가장자리(X2500,Y3550,H10100)→서쪽 앵커도 모두 열렸다. 실제 캡슐 이동·착지를 보장하지 않는다.
- ProductionSurfaceDataDefinitions 자동화 1개 통과(실패·경고 0). TestMap03으로 복귀해 첫 확대본 PIE를 잠깐 시작했다.
- 경사로 중앙의 꼭대기 한 점은 검출을 놓쳐 주변/폭 방향 45개 지점을 추가 검사했다. 모두 검출됐으나 일부는 예상 바닥보다 약 126cm 높은 층계참을 맞혔다. 원인은 경사로 끝을 층계참 중심에 맞춰 층계참의 반폭이 경사로 위에 겹친 것이다. PIE를 멈추고 경사로 끝을 층계참 가까운 가장자리(X1250,H6000)에 맞추며 층계참을 Y-9250부터 시작해 경사로 전체 폭을 받치도록 수정했다. 수정본 Live Coding 성공.
- 현재: 수정본 재생성→재베이크를 사용자에게 요청했으며 실행 대기다. 수정본 에셋·접합부·PIE 게시 검증은 아직 하지 않았다. 다음은 재베이크 뒤 접합부 전후 높이와 폭 방향 검출, 데이터 통합 테스트, TestMap03 PIE 게시를 확인하고 플레이 검토로 넘긴다.

### 2026-10-08 — 절벽 확대본 접합부 수정 최종 검증

- 사용자가 수정본 생성·베이크 명령을 실행했다. 저장 성공·lint=0·partTris=36134. Support 6 Layers·TotalSamples=1149051·payload=8068991 bytes, Nav cells=74755·components=401·portals=101, Spawn authored=1·candidates=10287.
- 실제 LVI에서 경사로 폭 3방향·접합부 전후·층계참 바닥 총 63개 지점을 검사해 모두 검출했다. 기준 높이와 검출 높이 차이는 최대 1.679cm. 높이 1m의 접합부 양방향 횡단 선 6개는 모두 열렸다. 층계참이 경사로 위로 겹쳐 생기던 높은 턱은 제거됐다. 실제 캡슐 이동 검토는 남는다.
- ProductionSurfaceDataDefinitions 재실행 성공(1 passed, 오류·경고 0). 새 TestMap03 PIE 시작 후 Hollow slot 2·3, Meadow/Bastion과 혼합한 8-slot 게시 성공, maxSeamRadiusDelta=0.000cm·minSeamNormalDot=0.95296, 마커 452개·0 skipped를 확인했다. PIE를 사용자 검토용으로 켜 두었다.
- 전체 월드 적 배치는 이번 조합에서 Bench01 204/204, Bench01_2 1040/1080, Bench01_3 737/810, Bench01_4 499/515로 총 129기 부족했다. 앞선 조합에도 발생하던 후보 부족이며 Hollow 지정 Pod의 개별 수량을 확인한 결과가 아니다. 원인 분리와 편성 검증은 후속 검토 사항으로 둔다.
- 다음 사용자 검토: `LNP.Debug.TeleportToReview HollowBasin 2`에서 규모감과 바닥→선반 훅, HollowPodWest/HollowLandingEast에서 양방향 횡단·중도 해제·착지·전투/벽보행 합류, HollowEntry/HollowWestLanding에서 경사로와 층계참 양방향 보행을 확인한다. 공동·수직 입구·전체 탐색/보스 경로는 아직 미제작이다.

### 2026-10-08 — 굽어진 협곡과 주변 침식 지형

- 사용자 확인: 절벽 확대본이 훨씬 멋있으며 바닥→양쪽 절벽 훅 이동 문제 없음. 추가 요청은 바닥의 소폭 연장·심화, 바깥 빈 공간의 작은 언덕·구덩이, 그랜드캐니언 같은 인상이다. 이어 면적 안에서 길이를 늘리도록 협곡을 구부리는 방향을 제안했다.
- 복구본은 `Saved/SliceOctant/Hollow_00/BeforeCanyonLandscape`. 300m 반지름·Meadow·Bastion은 유지했다. Hollow의 이음매 보존 띠 15m는 유지하고 내부 전이 폭만 40→20m로 좁혀 주변 지형 공간을 확보했다.
- 바닥 -75m, 양 끝이 반대 방향으로 휘는 9개 중심점과 연속 암반 W01/E01. 중앙 Pod·횡단 구역과 +60/+100m 윗면, 폭 12m 진입 경사로·층계참은 유지했다. 경사로가 암반 속에 들어가지 않도록 서쪽 외곽을 다듬었다. 바닥 중심선의 구면 길이는 약 340.8m, 앞선 직선 검토 구간은 약 231.7m다.
- 주변 메사 8곳(+9~24m)과 얕은 구덩이 4곳(-10~17m), 바닥 가장자리 잔존 암반 3곳(바닥 위 4.5~7m)을 추가했다. 지정 Pod 편성은 유지하고 굽이 앵커 2개·검토 지점 3개를 추가했다.
- 볼록 다각형 중심 팬을 오목한 단순 다각형 삼각분할과 긴 변 이등분으로 교체했다. 구면 윗면·밑면의 경계는 절벽면과 같은 이등분 위치를 사용한다. 외곽의 교차 없음, 모든 암반 지면 외곽이 35m 보존/전이 띠 밖(최소 37.4m), 밑면 좌표 성분 최대 약 32560cm로 int16 상한 안이다.
- 기존 프로젝트 Material/MaterialFunction을 검색한 뒤 해당 용도의 사암 층무늬가 없어 `M_Hollow_Strata`를 만들었다. 구면 반지름을 층무늬 좌표로 사용하고 색 2개·층 간격·거칠기를 노출했다. 컴파일·저장 성공. 빌더가 Hollow 정적 지형에만 재적용한다.
- 첫 C++ 컴파일에서 FVector4 명시적 생성자와 문자열 포인터 비교 오류를 수정했다. 최종 Live Coding 성공. 사용자에게 기존 BuildSliceOctant→BakeOctant 두 명령 실행을 요청했다. 실제 생성·베이크·충돌·PIE 게시 결과는 아직 대기다.
- 사용자가 실행했다. 재생성 saved·lint=0·parts=16·partActors=32·partTris=52738·cliffs=13·anchors=15·reviewPoints=9·authoredErrors=0. 베이크 저장 성공: Support sources/Layers=17·TotalSamples=1456490·payload=10230808 bytes, Nav cells=80173·components=527·portals=87, Spawn authored=1·candidates=10078. 지각 반지름 범위 29349.61~37500cm.
- 실제 LVI에서 중심선 9곳, 주변 메사·양쪽 절벽 윗면, 주요 검토 지점, 경사로·층계참을 포함한 116개 광선 중 113개가 기대 바닥을 검출했다(최대 높이 오차 1.643cm). 메사01/바닥암반01의 정확한 중앙 정점 2곳은 놓쳤으나 1~10cm 주변 격자 24곳씩은 모두 검출됐고, 동쪽 끝의 나머지 1곳은 몸체 끝 경계에 놓였다. 캡슐 보행 판정으로 확대 해석하지 않는다.
- 바닥→서쪽·바닥→동쪽·서쪽→동쪽·동쪽 안쪽 가장자리→서쪽·북쪽 굽이→양쪽 절벽 6개 선이 모두 열렸다. 진입 경사로 접합부의 폭 3방향·양방향 횡단 선 6개도 열림. 실제 W01_Floor 컴포넌트에서 사암 머티리얼 override를 확인했다.
- ProductionSurfaceDataDefinitions 통과(1 passed·오류/경고 0). TestMap03 복귀·PIE 시작: Hollow slot 0·4, 8-slot snapshot 게시 성공, maxSeamRadiusDelta=0.000cm·minSeamNormalDot=0.95296, navResident=23.11MiB, 마커 456개·0 skipped. PIE를 플레이 검토용으로 켜 두었다.
- 이번 전체 적 편성은 Bench01 475/540, Bench01_2 449/449, Bench01_3 802/810, Bench01_4 752/810으로 총 131기 부족했다. 기존 조합에서도 발생하던 전체 후보 부족이며 이번 Hollow 변경과의 인과·지정 Pod 개별 수량은 확인하지 않았다. 해당 검증은 후속 전투 검토로 유지한다.
- 다음: `LNP.Debug.TeleportToReview HollowBasin 0`에서 새 깊이와 훅, HollowBendSouth/HollowBendNorth에서 굽이 연결과 길이감, HollowMesa에서 바깥 지형의 밀도를 검토한다. 공동·수직 입구·전체 탐색/보스 경로는 미제작이다.

### 2026-10-08 — 협곡 외형 채택·꼭대기 부유섬 접근

- 사용자 확인: 굽어진 협곡과 주변 지형이 아주 마음에 들며 그대로 진행한다. 꼭대기 부유섬으로 어떻게 이동하는지, 앵커가 보이지 않는다고 질문했다.
- 기존 아레나는 실루엣 후보였으며 상승 동선은 미제작이었다. 현재 LVI에서 기존 앵커(X110m/Y-18m/H192m)까지 동쪽 절벽 104.7m 경로는 31.5m, 서쪽 149.2m 경로는 80.0m 지점에서 섬 몸체를 맞혔다. 바닥 경로도 가렸다. 기존 앵커의 200m 사거리로는 시야/밑면 가림을 해결하지 못한다.
- 복구본 `Saved/SliceOctant/Hollow_00/BeforeArenaAccess`. 협곡·주변 지형과 기존 횡단 경로는 유지한다. 중간 부유섬 Ascent(X-55m/Y0/H145m, 반경 12m·두께 12m)과 앵커(X-55m/Y13m/H178m, 사거리 140m)를 추가하고 아레나 앵커를 접근 쪽 가장자리 X30m/Y0/H205m(사거리 160m)로 옮겼다. 동쪽 절벽→중간 섬→아레나의 두 단계 훅 접근이다.
- 중간 섬 생성 전 기존 형상에서 새 경로의 후보 선은 동쪽→중간 앵커 약 89.6m, 중간 착지→아레나 앵커 약 68.0m이며 둘 다 열렸다. 동쪽에서 아레나 앵커를 바로 당기는 선은 여전히 섬 밑면에 막혀 중간 섬에서 재조준해야 한다. 중간 섬 자체를 포함한 실제 검증은 재생성 뒤 수행한다.
- 검토 지점 HollowAscent·HollowArenaEntry 추가. Live Coding 성공, 사용자에게 BuildSliceOctant→BakeOctant 실행을 요청했다. 생성·베이크·실제 충돌·PIE 게시 검증은 대기다. 보스 전투·전체 탐색 순환·공동은 아직 미제작이다.
- 사용자 실행 후 생성 saved·lint=0·parts=17·partTris=53170·islands=2·anchors=16·reviewPoints=11. 베이크 저장 성공: 18 Support sources/Layers·TotalSamples=1487463·payload=10449403 bytes, Nav cells=80678·components=528·portals=87, Spawn authored=1·candidates=10103.
- 실제 LVI에서 동쪽 중앙/서쪽 구간→중간 앵커 2개 선과 중간 섬의 중앙/북쪽/동쪽→아레나 앵커 3개 선이 열렸다. 대표 두 상승 선의 양옆 ±50/100cm 광선 10개 모두 열림. 중간 섬·아레나 진입부의 18개 바닥 선 모두 검출, 구면 기준 높이 오차 최대 8.149cm. 실제 캡슐 훅 이동·해제·착지를 보장하지 않는다.
- 동쪽의 먼 북쪽 구간→중간 앵커는 기존 아레나 밑면에 가렸다. 접근 시작은 HollowLandingEast/그 서쪽 구간이다. 아레나 접근 가장자리→중간 앵커 하강은 열림. 중간 섬에서 바로 동쪽을 당기면 자기 발판에 막히지만 +Y 가장자리(Y2200~2350)에서 시작하는 복귀 선 4개는 열렸다.
- ProductionSurfaceDataDefinitions 통과(1 passed·오류/경고 0). TestMap03 PIE 시작: Hollow slot 0·3·7, 8-slot 게시, maxSeamRadiusDelta=0.000cm·minSeamNormalDot=0.95296, navResident=23.12MiB, 마커 468개·0 skipped. 전체 적 배치 부족은 이번 조합에서도 발생(Bench01 503/540·Bench01_2 432/432·Bench01_3 789/810·Bench01_4 793/810, 총 75기)했으며 지정 Pod 개별 검증과는 구분한다.
- 사용자 검토 시작은 `LNP.Debug.TeleportToReview HollowLandingEast 0`: 중간 섬 앵커→아레나 접근 앵커, 중도 해제·착지·하강 복귀를 확인한다. 협곡/주변 형상은 보존했고 PIE를 켜 두었다.
- 사용자 실제 플레이 확인: 중간 부유섬을 거쳐 꼭대기 섬까지 올라가는 데 성공했다. 두 단계 상승 연결을 현재 기준으로 보존한다. 하강 복귀·중도 해제·전투/NPC 합류까지 확인된 것으로 확대 해석하지 않는다. 추가 지형/에셋 변경은 하지 않고 인계 문서와 실행 문서에 확인 결과를 기록했다.

### 2026-10-08 — 공동·두 수직 입구·추가 Pod

- 사용자 “작업 계속하자” 지시에 따라 채택한 협곡/주변 지형과 확인된 꼭대기 상승 연결을 보존하며 공동과 탐색 순환을 제작했다. 복구본은 `Saved/SliceOctant/Hollow_00/BeforeCavern`.
- 바닥 -120m·천장 -90m의 자유 형태 공동. 중앙/서쪽과 북쪽으로 이어지는 두 갈래, 기둥 3개와 등불 7개. 협곡 바닥 X0/Y0/H-75m(로컬 반지름 10m)과 북쪽 Mesa04 X30m/Y113m/H24m(로컬 반지름 6m)에 수직 입구만 둔다. 지상과 지하를 잇는 경사로는 없다.
- FCavern/BuildCavern은 기존 구면 다각형 바닥과 절단을 재사용한다. CoarseSupport 바닥, 두께 1m의 Blocker 천장/외벽. 지각과 메사 윗면/몸체·천장은 같은 48면 방사형 둘레로 절단한다. 입구 윗마감은 지면 1cm 아래, 출구 발판은 지표에 겹쳐 맞춘다. 외벽 좌표 성분 최대 약 32470cm, 보존/전이 띠 밖이며 이음매 정점은 유지했다.
- 협곡 입구 탈출 앵커 H-45m·사거리 110m, 메사 통로의 중간 착지점 H-55m·반경 2m·두께 2m와 앵커 H-5m/150m, 메사 출구 앵커 H70m/170m. 지하 이동 앵커 3개. 기존 서쪽 Pod를 유지하고 동쪽 1개·지하 2개를 추가(총 4개, 각각 비행 4·벽보행 6 지정 편성). 검토 지점 6개 추가(총 17개).
- Live Coding 성공. 사용자 BuildSliceOctant/BakeOctant 실행 후 saved·lint=0·parts=21·partTris=93818·bridges=4·islands=3·anchors=22·spawns=4·blocks=3·lamps=7. 지각 원본 삼각형 1650개 절단 후 237511 tris. Support sources=22·Layers=28·TotalSamples=1502849·payload=10561117 bytes, Nav cells=104112·components=533·portals=140, Spawn authored=4·candidates=11328. 저장 성공.
- 실제 LVI에서 두 통로의 지표→바닥 위 수직 선, 3개 탈출 경로 및 ±40/80cm 범위 광선 15개가 모두 열렸다. 기존 동쪽→중간 섬→아레나·바닥→양쪽 절벽 선도 열림. 바닥 45개 중 44개 검출(오차 최대 0.458cm); 나머지는 협곡 출구 발판의 정확한 시작 경계(Y700) 광선이며 폭 방향 ±1cm를 바꿔도 경계 위에서는 놓쳤다. 실제 접합부 캡슐 보행 검토는 남긴다. 공동 천장 3개 모두 검출됐다.
- ProductionSurfaceDataDefinitions 통과(1 passed·오류/경고 0). TestMap03 PIE: Hollow slot 2·5, 8-slot 게시·maxSeamRadiusDelta=0.000cm·minSeamNormalDot=0.95296·navResident=25.25MiB, 마커 470개·0 skipped. 전체 적 배치는 Bench01 538/540·Bench01_2 1020/1080·Bench01_3 564/575·Bench01_4 312/312(총 73기 부족). 지정 Pod 개별 편성 수량과 NPC 지상/지하 분리는 확인한 결과가 아니다.
- 다음 플레이 검토: HollowCaveBasinEntry 2에서 낙하·협곡 탈출, HollowCaveMesaEntry에서 긴 통로·중간 착지·메사 탈출, HollowCavern/HollowCaveNorth에서 두 갈래 탐색·지하 전투와 추격 합류. 단위 3은 미완료이며 동적 연결 장치·추가 탐색 연결과 최종 검증이 남는다. PIE를 켜 두었다.
- 사용자 실제 플레이 확인: 지하에 떨어져 전투하고 탈출하는 흐름을 문제없이 진행했다. 공동과 탈출 동선을 현재 기준으로 보존한다. 사용한 입구/출구는 특정하지 않았으며 두 입구 각각의 왕복·접합부 보행·지상/지하 NPC 분리·개별 편성 수량까지 확인된 것으로 확대 해석하지 않는다. 지형/에셋 변경 없이 Current와 실행 문서에 확인 결과를 기록했다. 다음 제작은 동적 연결 장치·추가 탐색 연결이며 단위 3은 미완료다.

### 2026-10-08 — 이동 패널·탐색 섬 수정본

- 사용자 계속 진행 지시에 따라 기존 형상/동선을 보존하며 Rim 섬(X-85m/Y98m/H30m·반경 16m)과 Shuttle 섬(X50m/Y-125m/H60m·반경 22m)을 추가했다. 북쪽 Mesa03→Rim은 폭 10m 경사로, 서쪽 절벽→Shuttle은 폭 8m 승강장·기존 4m 패널(4m/s·끝점 3초 정차)과 훅이다. Shuttle에 Pod 1개·지정 비행 4/벽보행 6 편성 추가. 앵커 3·검토 지점 3개 추가, 기존 마커 순서를 유지했다.
- 에디터 PIE 종료 상태와 TestMap03을 확인하고 에셋 전체 저장 성공 후 `Saved/SliceOctant/Hollow_00/BeforeExplorationConnections`에 source/Hollow 에셋 복구본을 만들었다. 수정은 Hollow spec뿐이며 Bastion·장치 기능·Support/Nav 구조는 변경하지 않았다.
- 새 섬 둘레 지면 이음매 최소 계산 여유 Rim 80.6m·Shuttle 36.9m, 보존/전이 띠 밖. 경사로 규칙 계산 26.8°·최종 구면 중심선 28.4~29.5°. 패널 끝점 윗면 모서리와 정적 발판 높이 차 약 7cm 이내. 변경 전 LVI에서 새 접근/복귀 방향 4개 선은 열림(수정 후 재검사 필요).
- Live Coding 성공·git diff --check 통과. TestMap03 콘솔 BuildSliceOctant/BakeOctant를 사용자에게 요청했다. 생성/베이크·새 에셋 충돌·PIE 검증은 아직 대기다.
- 사용자 1차 생성/베이크 저장 성공: lint=0·parts=25·partTris=95388·섬 5·다리 6·앵커 25·패널 1·Pod 5·검토 20. Support sources 26·Layers 32·TotalSamples 1572860·payload 11055854 bytes, Nav 106995 cells·537 components·194 portals, Spawn authored=5·candidates=11464. 기존 Mesa04 입구의 좁은 Layer에 Atlas 유효 샘플 없음 경고 1건. 객체의 실제 staticMesh 참조를 읽어 새 섬/경사로가 아님을 확인했다.
- 현재 에디터 세션의 자동화 worker를 DiscoverTests로 초기화한 뒤 ProductionSurfaceDataDefinitions 통과(1 passed·오류/경고 0). 새 접근/복귀 5개 선 열림. 새 섬/승강장·경사로 바닥 78개 중 77개 검출, 경사로 끝 정확한 경계 한쪽 광선 누락. 접합부 45개 추가 검사에서 중앙은 맞닿지만 양옆의 20~40cm 구간에는 바닥이 없어 폭 10m의 평평한 RimLanding을 추가했다. 수정본 Live Coding 성공 후 사용자에게 두 콘솔 명령 재실행을 요청했다. 재생성·충돌 재검사·PIE 게시는 대기다.
- 사용자 수정본 재생성/베이크 저장 성공: lint=0·parts=26·partTris=95458·다리 7. Support sources 27·Layers 33·TotalSamples 1574588·payload 11068467 bytes, Nav cells 107072·components 538·portals 223, Spawn authored 5·candidates 11465. Mesa04 입구의 좁은 Layer 경고 1건은 동일하며 실제 mesh 참조를 재확인했다.
- 수정 후 접합부 45개 중 42개 검출(최대 높이 오차 9.69cm), 누락은 접합부 직후 1/5cm와 폭 방향 -300/-450cm 조합 3개다. 8/10/15cm 전진한 폭 방향 5곳씩 15개는 모두 검출됐다. 실제 캡슐 보행 확인은 남긴다. 패널 고정 자세의 몸체 위/아래·탑승자 머리 높이 경로 18개 모두 열림. 새 훅 5개·기존 상승/협곡 횡단/공동 탈출 7개 선 모두 열림. ProductionSurfaceDataDefinitions 재검사 통과(1 passed·오류/경고 0).
- TestMap03 PIE 시작: Hollow slot 2·5·7, 8-slot 게시·314 source bindings·maxSeamRadiusDelta=0.000cm·minSeamNormalDot=0.95296·navResident=25.26MiB. Hollow 패널 3개 모두 길이 1198cm·속도 400cm/s·끝점 3초 정차로 서버 활성화. 마커 368개·0 skipped. 전체 적 배치는 Bench01 199/199·Bench01_2 1052/1080·Bench01_3 487/498·Bench01_4 775/810(총 74기 부족)으로 개별 Hollow 편성 검증과 구분한다. 플레이어 CharacterMovementComponent 추출 오류도 로그에 있으며 2026-09-27·10-06의 기존 로그에서도 같은 오류를 확인했다. 이번 spec 작업에서 원인을 수정하거나 PIE 로그 전체 오류 0으로 주장하지 않는다.
- 다음 플레이 검토는 HollowShuttleDock 2에서 패널 탑승/하차·섬 전투·패널/훅 복귀, HollowMesa 2에서 Rim 섬으로 경사로/층계참 양방향 보행이다. 직접 섬 접근은 HollowRimIsland·HollowShuttleIsland. PIE를 켜 두었으며 단위 3 최종 검증은 남는다.
- 사용자 실제 플레이 확인: 이동 패널 왕복과 훅 복귀, 북쪽 메사 경사로에서 걸림 없이 부드러운 이동을 확인했다. 패널 접안·경사로/층계참 연결을 현재 기준으로 보존하며 코드/에셋 변경 없이 문서에 기록했다. 새 섬 Pod 전투·NPC 합류·개별 편성 수량까지 확인한 것으로 확대 해석하지 않는다. 단위 3의 남은 검증은 지상/지하 무리 분리(수직 통로 NoPath), 협곡 벽 벽보행 추격, 연결 없는 섬의 원거리 교전, Spawn 투영/headroom/편성과 최종 검증이다.

### 2026-10-08 — Hollow 경로·편성 검증

- 사용자 계속 진행 지시에 따라 NPC 검토용 PIE를 시작하고 HollowPodWest·HollowCavern·HollowShuttleIsland의 추격·지하 탈출 시 무리 분리·섬/절벽 교전 확인을 먼저 요청했다. 로그에 세 이동 명령 실행이 있으며 플레이 결과 답변은 아직 없다. 이후 사용자 조작으로 PIE가 재시작됐다가 종료된 기록을 확인했다. 최신 조합 Hollow slot 0·4, 이음매 차이 0cm·마커 478개/0 skipped·navResident 25.33MiB. 현재 PIE는 종료 상태다.
- `LNPHollowContentTest.cpp`에 실제 베이크/SpawnConfig를 읽는 `LootNPop.SurfaceNavigation.Bake.HollowContent`를 추가했다. 지형·에셋·게임 실행 기능은 변경하지 않았다. 새 파일이 첫 Live Coding의 빌드 캐시에 포함되지 않아 당시 실행된 테스트는 0개였다. 모듈 Build.cs의 내용 변경 없이 수정 시각을 갱신해 파일 목록을 재수집한 뒤 실제 컴파일 및 테스트 목록 등록을 확인했다.
- 최종 Live Coding 성공 후 테스트 통과: 1 passed·0 failed·오류/경고 0, 0.667초. 8개 slot 회전에서 Pod 5개의 Nav 투영·edge clearance 250cm 이상·편성에 필요한 headroom을 확인했다. slot 0의 실제 edge clearance는 모두 800cm, headroom은 서쪽/셔틀 23999cm·동쪽 3040cm·지하 둘 3000cm, 투영 거리 최대 49.3cm였다.
- 공동 두 Pod 사이 A* 보행 경로는 Found였다. 공동 두 Pod와 지상 세 Pod 사이의 양방향 경로, Shuttle Pod와 양쪽 절벽 Pod 사이의 양방향 정적 경로는 모든 slot에서 Unreachable이었다. 동적 패널 경로와 실제 벽보행 AI를 보증하는 검사로 확대 해석하지 않는다.
- 실제 스폰 계획 16개 seed에서 8개 slot 각각의 지정 Pod 5곳(총 40 encounter)이 모두 비행 4·벽보행 6 편성을 전량 배치했다. 출현점은 지정 접근 영역 안이며 Pod에서 4m 이상 떨어졌다. 기존 전체 랜덤 Pod의 후보 부족 문제를 해결한 결과가 아니다.
- Current와 실행 문서에 데이터 검증 결과를 기록했다. 남은 것은 사용자 실제 벽보행 추격·지하 무리 분리·Shuttle Pod/원거리 교전 결과 확인이며 단위 3은 미완료다.

### 2026-10-08 — NPC 플레이 결과와 별도 LoS 수정 인계

- 사용자 확인: HollowPodWest의 지정 벽보행형은 처음부터 절벽 윗면에 출현해 그대로 공격한다. 사용자가 일부러 절벽 아래로 내려가 벽으로 유인하고 윗면으로 텔레포트했을 때 적이 벽을 올라와 공격했다. 이 검토 조건의 벽→윗면 추격 합류는 성공이다. "협곡 벽보행 적"이라는 요청은 별도 벽 위 출현 집단이 아니라 이 Pod 편성을 뜻했으며 설명이 모호했음을 바로잡았다.
- HollowCavern의 "입구"는 수직 통로를 뜻한다. 탈출 후 비행형은 지상까지 추격하고 그 외 적은 추격하지 못했다. 사용자는 벽에 붙지 않은 수직 통로 입구 형상이라 벽보행형의 단순 steering 추격이 어렵다고 평가했다. 관찰한 통로를 두 입구 전체의 결과로 확대하지 않는다. 기존 "지상·지하 적 완전 분리" 표현은 GroundSupport 보행 경로 분리로 정정했다. Nav NoPath는 비행형·SurfaceCrawl의 추격 차단을 보장하지 않는다.
- HollowShuttleIsland는 서쪽 Pod에서 이동 패널 건너편의 섬이다. 서쪽 벽보행형이 섬의 플레이어를 인식하지만 벽 중간에서 돌며 공격하지 못했다. 승강장 입구 평면까지 온 적도 모서리를 왕복하고 공격하지 못했다. 반대 방향에서도 섬 윗면 대신 벽으로 내려가 사선을 확보하지 못했다. 사용자는 사거리 안으로 보았으나 실제 거리·LoS 판정·위치 선정 원인은 확정하지 않았다. D-017은 미통과로 남긴다.
- 사용자 지시로 벽보행 LoS 수정은 다른 세션에서 수행한다. 이 세션은 코드/에셋 변경 없이 Current·Phase 9 실행 문서·TerrainContract의 결과/범위 표현을 갱신했다. Hollow 제작·플레이 검토는 종료하며 단위 3 전체 통과는 별도 수정 뒤 동일 지형의 Shuttle/서쪽 절벽 양방향 교전 재검증까지 보류한다. 새 섬 Pod 자체 전투 성공은 별도 확정하지 않았다.

### 2026-10-08 — 절벽 전장·외곽 진입·숨은 뒷길·보스 랜드마크 확장

- 사용자 추가 요청으로 단위 3 제작을 재개했다. 외곽에서 협곡으로 들어오는 과정과 절벽 자체 전장이 부실하다는 피드백이다. 중간 턱/잔도·Pod 크기의 굴·절벽 횡단 장치·외곽 암릉/잔도·한쪽 절벽의 구멍 또는 U자·공동의 외곽 경사로 뒷길·보스 무대 식별성을 추가한다. 보스 공간은 기존 높이 180m Arena로 설명했다. 반지름 300m와 별도 세션의 벽보행 LoS 수정 소유권은 유지한다.
- 에디터 PIE 종료·전체 에셋 저장 성공을 확인하고 `Saved/SliceOctant/Hollow_00/BeforeCliffBattlefields`에 Hollow 에셋과 빌더/테스트 소스를 보존했다. `FPassage`는 기존 BuildBridge와 볼록 절단을 재사용해 지각·암반·공동 벽을 자르고 실제 1m 두께의 측벽/천장을 붙인다. 런타임 장치·NPC 기능은 변경하지 않았다.
- 잔도 16·암릉 6·Pod 전투 굴 3 추가. 동쪽 뒤편 U자 윤곽, 6m 횡단 다리와 기존 4m 패널의 협곡 횡단 경로, 외곽 다리, 50개 앵커(기존 절벽 직통 2개는 75m·나머지 해당 절벽 상단 6개는 90m로 축소), 총 Pod 8개로 확장했다. 기존 다섯 Pod GUID 순서를 유지했다. Arena 중앙은 비우고 반경 33m 가장자리에 높이 35/55m의 암주 5개를 추가했다. 보스 자체·보상/밸런스는 작업 범위가 아니다.
- 뒷길은 공동 서쪽(-45m,45m,-120m)에서 두 굽이를 거쳐 외곽(-155m,-108m,0)으로 열린다. 최초 계산 최대 경사 27.11°·외곽 동쪽 다리 32.79°, 뒷길 최대 좌표 성분 32046cm, 새 암릉의 지면 이음매 최소 3721cm로 3500cm 보존/전이 띠 밖이었다. 실제 곡률 경사와 좌표를 검사했으며 메시 접합/캡슐 이동을 보증하지 않는다.
- 최초 Live Coding·사용자 생성/베이크 성공: lint=0·parts 60·partActors 120·partTris 123801·cliffs 35·bridges 13·islands 5·anchors 50·panels 2·spawns 8·blocks 8·lamps 7·reviews 31. 베이크 61 Support sources·68 Layers·1885485 samples·SupportPayload 13288529 bytes, Nav 122663 cells·550 components·413 portals, Spawn authored 8·candidates 12149. 좁은 Layer Atlas 샘플 경고 1건은 확인이 필요하다.
- ProductionSurfaceDataDefinitions 통과(1 passed·오류/경고 0). 확장 HollowContent는 Pod 8개의 Nav 투영·edge clearance·headroom 및 16 seed의 전량 편성 검사를 만족했지만 공동/서쪽 진입 양방향 Reach 32건이 실패했다. 위치별 투영에서 첫 통로는 공동 group 457, 첫 굽이 뒤는 지표 group 0으로 분리돼 첫 급한 굽이가 단절점임을 확인했다.
- 두 굽이에 폭 14m 수평 층계참을 추가하고 경사 구간을 층계참 시작점으로 옮겼다. 같은 뒷길의 바닥은 절단에서 보존하고 맞닿는 벽만 열도록 수정했다. Live Coding 성공 후 TestMap03을 다시 열어 사용자에게 생성/베이크를 재요청했다. 수정본의 경로·충돌·PIE 검토는 대기다. 새 경사로는 지상/지하 보행 분리라는 이전 B 제작 기준을 의도적으로 바꾸며, 회귀 테스트도 NoPath 대신 양방향 Reach를 요구한다.
- 층계참 수정본에서도 첫 굽이의 경사 끝에 나중에 생성한 통로 측벽이 남았다. 기존 통로 절단 체적으로 새 통로 Shell도 상호 절단하도록 수정해 재생성했다. 2026-10-08 05:49 UTC 베이크: parts 64·partActors 128·partTris 125648·cliffs 35·bridges 13·islands 5·anchors 50·panels 2·Pod 8·blocks 8·review 31, 71 Layers·Support samples 1892762·payload 13340550 bytes·Nav 123237 cells/544 components/447 portals·Spawn candidates 12173/payload 489412 bytes. ProductionSurfaceDataDefinitions 통과, HollowContent의 공동↔서쪽 Reach 32건은 실패했다.
- 상호 절단 뒤 첫 경사와 두 층계참을 따라 150cm 이하 구간으로 나눈 몸통/머리 높이(88/180cm), 좌우 40cm 포함 372개 선 추적은 모두 열렸다. 그러나 경사 끝 98.1% 위치의 위에서 아래 추적은 의도한 경사보다 약 83cm 높은 수평 바닥을 검출했다. 되돌아가는 폭 14m 층계참이 들어오는 경사 바닥을 덮는 문제로 좁혀 두 굽이의 되돌아가는 층계참만 폭 8m로 조정했다. 들어오는 14m 층계참과 8m 경사로는 유지한다.
- 동쪽 하부 EastStep과 서쪽 외곽 WestBackStep 잔도/앵커를 추가해 상승 연결 공백을 보충했다. 실제 월드 거리 EastLow→EastStep→EastPocket은 53.5/55.4m, WestBackLow→WestBackStep→WestBackMid는 43.1/49.4m로 각 마커 사거리 안이다. 동쪽 외곽 중단→상단은 66.0m이므로 해당 상단 마커만 65m에서 75m로 조정했다. Live Coding 성공 후 TestMap03을 열고 사용자에게 재생성/베이크를 요청했다. 이번 폭 수정본의 실제 충돌·보행 연결·플레이 검토는 아직 대기다.
- 폭 수정 후 뒷길 7구간의 전 샘플과 공동 두 Pod·서쪽 Pod가 group 0으로 연결됐다. 남은 32건은 단절이 아니라 테스트가 30000회 확장 뒤 Running 상태에서 Found를 요구하던 문제였다. 콘텐츠 연결 검사의 확장 상한/한 번의 실행 예산을 전체 graph node 수로 정하고 끝까지 탐색했다. 실제 양방향 경로가 Found였으며 slot 0의 확장 수는 36388/45648 및 36388/51087이다. HollowContent 통과(1 passed·오류/경고 0). 런타임 A* 기본 요청 상한 30000은 그대로이며 이 길이의 우회가 그 상한을 넘는다는 점은 후속 경로 비용 검토 근거로 남긴다.
- 절벽 연속 훅을 실제 생성 충돌로 검사해 WestLow→WestPocket 바닥, WestPocket→WestMid 천장, EastStep→EastPocket 바닥, WestBackStep→WestBackMid 기존 진입 경사로, EastBackLow→EastBackMid 암반에 가리는 위치를 확인했다. 굴 전면/잔도 바깥으로 앵커를 옮기고 WestBackMid 잔도를 더 바깥에 배치했다. 바꿀 위치의 선 추적은 모두 열렸으며 WestPocket 출발은 굴 안쪽이 아니라 입구 가장자리에서 해야 천장을 피한다.
- 외곽 다리의 경사 바닥과 잔도 바닥 중첩을 추가 검사했다. 서쪽 약 45cm·동쪽 약 284cm 턱을 검출했다. 경사 끝을 잔도 바깥으로 옮기고 같은 높이 수평 층계참으로 이어 들어가도록 두 다리를 수정했다. 외곽 암릉↔잔도 양방향 실제 A* 검사를 HollowContent에 추가했고 수정 전 에셋에서 4건 실패를 재현했다. Live Coding 성공 뒤 사용자에게 재생성/베이크를 요청했다. 확인된 Atlas 빈 샘플 경고의 소스는 기존 수직 입구가 절단된 Mesa04_Floor이며 뒷길이나 새 잔도가 아니다.
- 수평 층계참 재생성 후 서쪽 암릉↔잔도는 양방향 Found(357/378 확장)였으나 동쪽 두 방향은 Unreachable였다. 위치별 Nav 투영으로 동쪽 다리 끝·층계참·잔도는 group 507, 암릉 중앙만 group 513이며 다리 시작점은 같은 암릉 Layer에서 group 507임을 확인했다. 경사 다리를 암릉 위에서 시작해 낮은 밑면이 암릉 보행을 가르는 문제다. 암릉 밖으로 나오는 수평 OuterEastDock을 추가하고 그 옆 가장자리에서 경사를 시작하도록 옮겼다(최대 실제 경사 30.40°). 끝 접합부를 더 늘리는 방식은 쓰지 않았다.
- 새 잔도/굴/정상과 뒷길을 검사한 최초 182개 바닥 선 중 EastPanel 네 점이 누락됐다. 해당 착지 바닥을 EastPocket이 절단하고 5m 낮은 굴 바닥으로 대체한 원인이므로 굴 바닥·Pod·검토점을 H=1500에서 2000으로 맞추고 해당 전면 앵커를 H=3700/사거리 6500으로 조정했다. 패널/접안 높이는 유지한다. 훅 21개 연결에서 서쪽 세 번째 암릉→상단 직행은 기존 Shuttle 섬 몸체에 가렸다. 해당 암릉은 바깥 중단 잔도를 거쳐 상단으로 가는 경로로 검토하며 별도 직행 앵커는 추가하지 않는다. Live Coding 성공 후 사용자에게 재생성/베이크를 요청했다.
- 최종 생성/베이크(2026-10-08 15:19 KST): lint=0·parts 69·partActors 138·partTris 127699·bridges 16·cliffs 37·islands 5·anchors 52·panels 2·Pod 8·blocks 8·lamps 7·reviews 31. 70 Support sources·86 Layers·1922271 samples·SupportPayload 13551836 bytes·Nav 124454 cells/551 components/554 portals·Navigation/Traversal payload 1011644/29895 bytes·Spawn 12201 candidates/490532 bytes 저장. 좁은 조각 Atlas 경고 6건은 Mesa04_Floor 1개와 굴 절단 후 EastPanel_Floor의 작은 잔여 조각 5개로 확인했다. 해당 착지 공간의 실제 바닥은 새 EastPocket 바닥으로 검출되며 조각 경고를 새 통로 단절로 해석하지 않는다.
- 최종 HollowContent/ProductionSurfaceDataDefinitions 2 passed·오류/경고 0. 서쪽 외곽 양방향 경로는 357/378 확장, 동쪽은 853/932 확장으로 Found. 뒷길 양방향 Found 및 Pod 8개/16 seed 전량 편성 기준도 유지됐다. 잔도/암릉/굴/정상과 뒷길·외곽 다리/층계참·협곡 횡단 다리의 좌우 242개 바닥 선 모두 검출됐다. 238개는 기준 10cm 이내, EastPanel 네 점은 구면 굴 바닥 삼각형 보간으로 약 14.3cm 높아 플레이어 턱 오르기 40cm 미만이다.
- 절벽/외곽 훅 21개 연결을 선 추적으로 검사했다. EastStep 중앙 X=2000/Y=2000에서는 EastPocket 앵커까지 접안 바닥이 가리지만 X=2200/2300/2500·Y=2050/2200의 여섯 출발점에서는 열렸다. 새 앵커를 추가하지 않고 잔도 가장자리에서 조준하는 경로를 유지한다. WestPocket→WestMid도 굴 입구 가장자리에서 조준해야 천장을 피한다. 선 추적은 실제 카메라 조준·캡슐 이동·착지·전투 성공을 보장하지 않는다.
- TestMap03 PIE 실행, Hollow slot 2·4 배치, generation 1의 8 validated slots/473 source bindings 게시, maxSeamRadiusDelta=0.000cm·navResident 26.14MiB 확인. 기존 PIE에서도 발생하던 BP_LNPPlayer의 CharacterMovementComponent 추출 LogMass 오류가 다시 출력됐다(플레이어는 Mover 사용). 이번 지형 작업에서 해당 시스템은 수정하지 않았다. 이번 확장본의 실제 이동·전투·보스 랜드마크 식별성은 사용자 플레이 검토 대기다.

### 2026-10-08 — 측면 U자·암반 왕복 경사로·굴 외형·추락 코너 수정

- 사용자 검토: 외곽과 잔도/다리, 숨은 뒷길의 방향은 좋다. 외곽 경사로는 지면까지 채운 암반이어야 하며 한쪽 산을 좌우 왕복 경사로로 정상까지 연결하라고 추가 지시했다. 전투 굴의 외부 박스 모양을 거부했고 절벽 안쪽 공간 또는 넓은 잔도를 요청했다. U자는 탑뷰가 아닌 측면 실루엣이다. 첫 잔도까지 훅은 성공했으나 다음 앵커를 찾지 못했다고 구체화했다. 뒷길 첫 굽이에서 협곡 바닥, 두 번째 굽이에서 옥탄트 밖으로 추락할 수 있는 메시 누락을 지적했다. Arena는 얇은 섬, 낮고 작은 엄폐물, 약간 굴곡진 지면을 요청했다.
- PIE 종료 상태·에셋 저장 후 BeforeSideProfileRevision 복구본을 만들었다. 외곽 경사로는 지각 아래 H=-20m까지 채운 넓은 밑동을 갖고, 서쪽 산은 3구간 왕복과 두 수평 회전 암반으로 정상에 연결한다. 뒷길 입구와 산 진입부도 암반으로 연결한다. 동쪽 평면 윤곽의 잘못된 노치를 제거하고 중앙 X=-75~-45m 구간만 H=45m로 낮춰 측면 안부를 만든다. 기존 무의미한 NotchLanding은 안부 바닥에 닿는 SaddleLanding으로 교체한다.
- 굴은 실제 암반 내부를 절단하며 새 천장/측벽/바닥을 원래 암반 입구 경계 안에만 남긴다. 넓은 입구 잔도로 접속한다. BackPocket Pod는 내부로 이동하되 D=8 GUID 순서와 비행 4/벽보행 6 편성을 유지한다. 정상섬은 H=180m 유지, 기본 두께 50m→8m와 좁아지는 밑동, 지면 명목 ±1.8m 굴곡, 암주 폭 3m·높이 3/5m로 변경한다.
- 뒷길 굽이를 14m 닫힌 회전 공간으로 만들고 경사 시작점까지 수평 연결을 길게 둔다. 접합 절단은 내부 높이만 열어 다른 구간의 바닥·천장 두께가 함께 사라지는 문제를 방지한다. 새 코너 50점 Nav 투영 검사를 수정 전 베이크에서 실행해 첫 굽이 7개·둘째 굽이 5개, 총 12개 바닥 누락을 재현했다. 사용자 추락 보고를 실제 회귀 검사로 확인했다.
- 앵커는 절벽 앞 시야를 유지하며 다음 발판 밑면이 비행 선을 가리지 않도록 좁은 암반 착지부를 연결한다. 동쪽 안부에 맞춰 기존 상단 복귀 앵커 2곳을 높은 쪽 암반 위로 옮겼다. 장치·이동/NPC 런타임 코드는 변경하지 않았다. Live Coding patch 16 성공, diff --check 통과. 사용자에게 TestMap03 콘솔 생성/베이크를 요청했으며 최신 에셋의 lint·코너 충돌·A*·편성·훅·PIE 검증은 대기다.
- 1차 새 생성은 lint=0·parts 93·partActors 186·partTris 144397·다리/암반 경사로 24·섬 5·절벽/잔도 51·앵커 52·패널 2·Pod 8·검토점 35였다. 베이크는 BackPocket Pod의 실제 캡슐 충돌로 중단해 이전 SurfaceData를 유지했다. 넓은 바닥을 실제 2m 이하 폭으로 세분한 2차 생성도 lint=0·partTris 148598이며 같은 Pod 충돌이 남았다. 분할 세분만으로 원인이 해결됐다고 기록하지 않는다.
- 새 두 코너의 바닥/천장 324개와 막혀야 할 측벽 7개, 총 331개 광선 모두 검출됐다. 첫 코너 바깥 대각선과 둘째 코너 바깥 대각선도 닫힌 측벽을 검출했다. 양쪽 주요 훅 11구간의 중심/머리/발/네 측면을 150cm 이하 선분으로 검사한 2828개 광선은 가림이 없었다. 다음 앵커 위치에서의 직접 조준과 실제 착지 감각은 사용자 검토가 남는다. 동쪽 상단 한 앵커는 출발 중심에서 64.8m라 주변 위치 여유를 위해 사거리를 65m→70m로 바꿨다.
- 캡슐 겹침 진단으로 실패 Pod의 충돌 대상이 WestLanding_Shell임을 확인했다. 지면까지 채운 경사로의 넓은 밑동과 BackPocket Pod가 겹쳤다. 굴 안에서 Pod/검토점을 Y=-60m→-53m로 옮겨 저장했고 같은 캡슐 검사에서 경사로 겹침이 사라졌다. 변경 전 바닥 위 0/2/10cm 모두 경사로와 충돌, 변경 후 모두 비충돌이었다. 위치·회전은 빌더 Point/Rotation 계산과 일치하게 적용했다. 임시 진단 코드는 제거했고 기존 콘텐츠 검사를 복원했다. Live Coding patch 21 성공, TestMap03에서 베이크 한 명령만 재요청했다. 최신 SurfaceData·보행/편성·PIE 검증은 아직 대기다.
- 최신 베이크는 103 Layers·126356 cells·673 components·1307 portals·8 authored Pods로 성공했으나 HollowContent 보행 38개가 Unreachable(확장 0)이었다. 코너 50점 바닥 투영과 편성 검사는 통과했다. 캡슐 진단으로 SecretJunction 벽의 바닥 띠, 단일 끝막음 평면이 곡면 바닥 위로 솟는 문제, 왕복 암반이 기존 외곽 길을 가로막는 교차를 확인했다. 끝막음은 바닥과 같은 2m 폭 분할, 굽이 절단은 보존된 바닥 아래까지 열기, 왕복 산은 Y=-185~-95m 외곽으로 이동, 길 주변 지각은 경사 높이를 따르지 않고 H=-5m로 낮추기를 반영했다. 사용자 에디터 재시작 후 patch 1 성공. 재생성/베이크 및 연결 재검사는 대기다.
- 접합부 수정 후 생성 lint=0·crust 236879 tris·parts 93/186 actors/148744 tris, 최신 베이크 103 Layers·129894 cells·660 components·1499 portals·8 authored Pods·12041 spawn candidates 저장. HollowContent/ProductionSurfaceDataDefinitions 각각 Success·오류/경고 0. 외곽 서쪽 357/378·동쪽 1022/1948·산 31978/11169 확장으로 양방향 Found, 공동↔서쪽 Pod 8 slot 양방향 및 16 seed 전량 편성도 통과. 산 전 구간 연결의 오프라인 검색은 graph 상한을 사용한 것으로 런타임 30000 상한의 비용 검증은 별개다. 새 코너 331개 광선은 누락 0, 바닥 199.75~200.00cm/천장 200.94~206.00cm에서 검출됐고 외곽 대각선 측벽도 유지됐다. CaptureViewport에 모든 annotations 필드를 명시해 뷰포트 촬영 성공, 암반 왕복 경사로·측면 안부·굴 내부·얇은 Arena와 작은 엄폐물을 관찰했다. TestMap03 PIE 실행, Hollow slot 2·5·6, generation 1의 8 validated slots/604 source bindings 게시, seam 높이차 0·Nav resident 26.54MiB. 베이크의 좁은 Layer 경고 1개와 PIE의 기존 CharacterMovementComponent 추출 오류/Mover 초기 위치 경고는 별도로 남긴다. 실제 플레이 검토는 대기다.
- 사용자 추가 검토: 연속 앵커로 바닥→절벽 정상 상승은 통과했고 전투 굴 외형도 개선됐다고 확인했다. 왕복 암반 경사로는 절벽에 완전히 붙어야 한다고 정정했다. 정상 접합 추락 1곳·입구 돌출 폴리곤 1곳·두 굽이 주변 큰 균열/작은 균열/머리 위 사다리꼴 구멍 7곳을 월드 좌표로 보고했다. 역산 결과 모두 슬롯 5와 일치했다. 두 굽이의 측벽 구멍을 뷰포트에서 재현했고 기존 331개 검사가 접합부 외피를 충분히 덮지 못했음을 확인했다. BeforeAttachedRampRevision에 원본을 복구 보관했다. 경사로를 서쪽 절벽 뒤편에 붙이고 절벽까지 암반 윗면/밑동을 채웠다. 마지막 경사 끝은 수평 도착부와 암반 정상 발판으로 접속한다. 회전 공간의 벽에 연결 문만 열고 연결 통로 외피는 방 내부에서 끝내며 천장은 따로 보존하도록 수정했다. 비밀 입구 바깥 경사로의 바닥까지 무조건 보존하던 예외를 제거해 통로에 삐져나오는 폴리곤도 절단한다. Live Coding patch 2 성공, 생성/베이크 대기.
- 절벽 부착 수정본 사용자 생성·베이크 확인: lint=0, parts 95/actors 190, partTris 157897, Support sources 96/Layers 108, Nav 137765 cells/691 components/2413 portals, Spawn authored 8/candidates 12587. 좁은 Atlas 경고 2건이 남았다.
- 두 회전 공간과 연결 층계참 6곳에서 바닥 위 50/150/400/700/950cm, 방위 5도 간격의 외향 광선 1,935개가 모두 외피에 충돌했다. 공동부터 외곽 입구까지 모든 중심선 Nav 투영이 같은 group 0에 연결됐다. ProductionSurfaceDataDefinitions는 통과했으나 HollowContent는 산 정상/서쪽 Pod 연결 34건에서 실패했다. 외곽 서쪽/동쪽 연결과 16 seed 편성·굴 바닥 검사는 통과했다.
- 실제 경사로/수평 도착부를 따라 캡슐·Nav를 검사해 마지막 상승 구간이 WestLanding 바닥과 측벽을 너무 일찍 만나는 것을 확인했다. WestLanding 시작 Y를 -9250→-8500으로 줄여 평평한 정상 접합부에서만 만나도록 수정했다. 임시 진단 코드는 제거했고 Live Coding patch 5가 성공했다. TestMap03을 열어 사용자 재생성/베이크를 요청했다. 최종 보행 연결과 PIE는 아직 확인 전이다.
- patch 5 사용자 재생성/베이크: 108 Layers, Nav 137739 cells/688 components/2389 portals, Spawn candidates 12580. ProductionSurfaceDataDefinitions 통과, HollowContent의 산/서쪽 Pod 양방향 연결 34건은 계속 실패했다. WestLanding 단독 수정은 겹침을 모두 제거하지 못했다.
- 50cm 간격의 경사/수평 연결로 캡슐 검사에서 마지막 경사 local(1972,-8674,H5918)가 MountainSummit_Shell/Floor와 충돌했고, 첫 경사 local(-5452,-13010,H260)부터 Mesa07이 길을 침범했다. MountainSummit 끝을 (2000,-7400,H6000)으로 안쪽에 이동하고 Mesa07 Y를 -15500으로 옮겼다. 사용하지 않게 된 LandingHalfArc와 임시 진단 코드를 제거했다. Live Coding patch 7 성공, 사용자 재생성/베이크 대기다.
- patch 7 사용자 생성·베이크·저장 완료: 96 Support sources/110 Layers, TotalSamples 2218521, SupportPayload 15657776 bytes, Nav 137826 cells/709 components/2424 portals, Spawn authored 8/candidates 12572. 좁은 Atlas 경고 2건은 남았다.
- HollowContent/ProductionSurfaceDataDefinitions 2 passed·오류/경고 0. 산 지면↔정상 A* Found(18281/12258 expansions), 공동↔서쪽 Pod도 양방향 Found, 두 외곽 진입로·코너 50점 바닥·16 seed 편성이 통과했다. 임시 캡슐 진단 코드와 테스트 기대값 완화는 남기지 않았다.
- 재생성된 통로 6곳 접합 외피 광선 1,935개 모두 충돌. 절벽 부착 왕복 지형을 에디터에서 확인했다. TestMap03 PIE 시작, Hollow slot 2/4/7 포함 8 validated slots의 generation 1 게시와 maxSeamRadiusDelta=0.000cm 확인. 실제 왕복 길 보행과 통로 균열/입구 외형의 사용자 재검토를 남긴다. PIE의 기존 Mass CharacterMovementComponent 추출 오류는 별도 사항이며 런타임 오류 0으로 기록하지 않는다.

### 2026-10-08 — 왕복 길 축소와 동쪽 외곽의 겹침/진입 정리

- 사용자가 부착 왕복 경사로와 비밀 통로의 처음→끝 통행을 눈에 띄는 균열 없이 확인했다. 폭은 전체 2m 축소로 해석해 경사/수평 도착부/정상 연결로 12→10m, 왕복 굽이 폭 20→18m로 수정한다.
- 반대 외벽의 새 보고 좌표 4곳은 slot 7로 역산됐다: local(-7568,9472,H4598),(-12346,10747,H2498),(-9659,11019,H3099),(-3449,12323,H1698). 앞 두 곳은 기존 OuterEast 경사/착지부와 암릉 출구 접합이고, 반원은 Rim 섬이 높은 암반 경사로에 가려진 부분, 마지막은 지면 진입 없이 Mesa03에서 끝나던 Rim 접근 동선이다.
- Hollow 저장 성공 후 에셋/빌더/테스트를 Saved/SliceOctant/Hollow_00/BeforeEastOuterRevision에 보존했다. 높은 경사가 Rim을 관통하지 않게 암릉 출구→H3000 경사→Rim 수평 착지로 재배치했다. 섬 동쪽 출구→H4500 상승→18~20m 수평 도착부→회전→안부로 연결하며, Mesa03에는 지면에서 시작하는 암반 경사·15m 수평 도착부를 추가했다. 경사 시작/끝은 가능한 한 같은 방향의 단면을 공유한다.
- 새 경사의 구면 실제 최대 기울기는 외곽 진입 23.40°, Rim→안부 34.84°, 지면→메사 31.16°로 35° 이하다. 비밀 통로·기존 훅·Pod/패널은 수정하지 않았다. HollowContent에 지면→Rim 양방향 A*를 추가했다. Live Coding patch 9 성공, TestMap03을 열어 사용자 재생성/베이크를 요청했다. 실제 접합/베이크/PIE는 아직 검증 전이다.
- patch 9 사용자 생성/베이크/저장 완료: lint=0, parts 103/actors 206/tris 158494, bridges 33, islands 5, cliffs 52, anchors 52, panels 2, spawns 8, reviews 36. Support sources 104/Layers 118, TotalSamples 2227786/SupportPayload 15729962 bytes, Nav 138045 cells/711 components/2560 portals, Spawn candidates 12486. 좁은 Atlas 경고 2건은 남았다.
- HollowContent/ProductionSurfaceDataDefinitions 2 passed·오류/경고 0. 동쪽 암릉→안부 양방향 Found(2498/3584 expansions), 새 지면→Rim Found(2615/2498), 서쪽 산·외곽/뒷길 기존 연결·16 seed Pod 편성도 통과했다.
- 새 동쪽 동선과 기존 Rim 접근 16개 구간을 50cm 간격/중심 및 좌우 2.5m에서 검사했다. 바닥 광선 2013개 중 2003개 충돌; 10개 미검출은 공유 정점/회전 모서리 바깥이었다. 각 지점 ±1/10cm의 보충 광선 250개로 고립된 정점 누락과 실제 외부 경계를 구분했다. 광선 검사를 실제 캡슐 이동 성공으로 기록하지 않는다. 에디터에서 Rim 위를 가로막던 높은 암반의 제거와 새 동선을 확인했고 TestMap03 PIE를 켰다. 새 동쪽 접합/양쪽 진입과 축소된 왕복 폭의 사용자 보행 검토가 남는다.
- 최신 PIE는 Hollow slot 0/4 포함 8 validated slots의 generation 1 게시, maxSeamRadiusDelta=0.000cm을 확인했다.

### 2026-10-08 — Rim 암반 사이 추락 틈과 서쪽 정상/패널 공간

- 사용자가 10m 왕복 폭을 채택했다. 정상의 의미 없는 턱, 패널 공간 침범과 Rim 주변 추락 후 자력 복귀 불가 4곳이 보고됐다. 모든 좌표는 slot 5로 역산된다. local(1301,-7877,H5812)은 마지막 상승 암반과 H6000 진입로 경계, (4953,-8519,H6099)은 마지막 굽이가 패널 통과 영역에 들어온 곳이다. 추락 뒤 좌표는 (-10910,10750,H-382),(-4634,10515,H-402),(-3636,7305,H3317),(-3398,8712,H189)다.
- 이전 길 중심/좌우 바닥 검사와 Nav 연결은 길 옆의 깊은 끼임 공간을 놓쳤다. 네 곳은 암릉/경사, 상승/메사 접근, 되돌아오는 길/절벽, 높은 왕복 길 사이에 남은 틈이다. 복구본을 Saved/SliceOctant/Hollow_00/BeforeRimTrapRevision에 저장했다.
- 마지막 왕복 경사의 수평 출발부는 30→22m, 수평 도착부는 18→34m로 조정해 local X≈612부터 H6000에 도달하게 했다. 최대 구면 경사 27.85°. 마지막 굽이 HalfX 1100→600으로 줄여 패널 측면에 local 150cm의 여유를 둔다. 채택된 10m 폭은 유지한다.
- 인접 길 사이를 닫힌 암반 4개로 채우며 `FCliff.SurfaceH`로 길 높이에 맞는 윗면을 1m 이하로 분할한다. 기존 길 안쪽에서는 윗면을 2cm 낮춰 보행면 침범을 줄인다. Live Coding patch 10 성공, TestMap03 사용자 생성/베이크 대기. 외피·실제 캡슐·패널 검사와 기존 경로 재검증은 반영 뒤 수행한다.
- patch 10 사용자 생성/베이크 완료: lint=0, parts 107/actors 214/tris 195413, bridges 33/cliffs 56, Support sources 108/Layers 124, TotalSamples 2274090/SupportPayload 16091555 bytes, Nav 138024 cells/759 components/2808 portals, Spawn authored 8/candidates 12473. HollowContent/ProductionSurfaceDataDefinitions 2 passed·오류/경고 0. 좁은 Atlas 경고 2건은 남았다.
- 실제 월드의 1034점 바닥/캡슐 진단에서는 바닥 미검출 0·겹침 96점을 확인했다. 급한 경사 접촉과 기존 벽 내부를 함께 검사하므로 겹침 수를 모두 통행 실패로 해석하지 않는다. 정상 접합 주변 121점은 H6000/캡슐 겹침 0. 런타임과 같은 고정 회전 및 직선 보간으로 패널 400×400×30cm 본체 81 자세·좌우/중앙 탑승 캡슐 243 자세를 검사해 정적 지형 충돌 0을 확인했다. 결과는 Saved/SliceOctant/Hollow_00/Patch10Capsules.csv에 보존했다.
- RimLowerRockJoin이 local(-5000,12000)의 기존 메사 H1600 바닥을 실제 H1889.56으로 덮고, X=-4300 분기 주변에 급한 턱을 만드는 것을 발견했다. 반평면 거리 대신 유한 경사로 폭/길이와 메사 팔각 윤곽·수평 진입로의 거리를 연속 보간한다. 메사 내부 채움 높이는 H1598로 낮추고, 기존 경사로와 메사가 겹치는 가장자리는 메사에서 멀어지는 만큼만 상승시켜 채움이 기존 메사 바닥을 높이지 않게 했다. 계산상 X=-4300±0.01의 높이 차는 0.006cm다. 임시 진단 코드 제거 후 Live Coding patch 12 성공, TestMap03을 열고 사용자 재생성/베이크를 요청했다. 새 형상의 실제 충돌·보행과 PIE는 아직 검증 전이다.
- 위 local(-5000,12000) 지점의 원인 귀속을 정정한다. 충돌한 Actor를 기록하니 H1889.56은 기존 RimApproach_Floor였으며 이 지점은 채움 윤곽 밖이었다. 실제 채움의 메사 침범은 local(-4500,11200) 등에서 확인됐다. 해당 지점은 patch 10의 H1996.24에서 patch 12의 H1600.16으로, (-4400,11200)은 H2037.04→1600.17로 복원됐다. 경사로 자체의 의도된 메사 위 높이는 보존한다.
- patch 12 사용자 생성/베이크 완료(20:55 KST): lint=0, Support sources 108/Layers 124, TotalSamples 2268224/SupportPayload 16051006 bytes, Nav 138114 cells/753 components/2776 portals, Navigation/Traversal payload 1124548/75951 bytes, Spawn candidates 12476/payload 501532 bytes. 좁은 Atlas 경고 2건은 남았다.
- 실제 월드 바닥/캡슐 2156점은 높이 변화에 맞춰 추적 범위를 보정한 뒤 바닥 미검출 0. 14개 구간의 중심·좌우 2.5m에서 1680점과 연속 캡슐 sweep을 검사했다. sweep 접촉 16건 중 11건은 40cm 높인 캡슐에서 해소됐고, 나머지 5건은 새 지면 진입 길의 오른쪽 가장자리를 Mesa08이 막는 충돌이었다. 회전벽 바로 옆의 캡슐 겹침을 메시 균열로 해석하지 않는다. 네 추락 좌표 주변은 암반 바닥으로 채워졌지만 경사면 슬라이딩/실제 자력 복귀는 사용자 플레이 검토가 남는다. 결과 Saved/SliceOctant/Hollow_00/Patch12Capsules.csv.
- 임시 진단을 Saved/SliceOctant/Hollow_00/HollowPatch12PhysicalProbe.cpp에 보존하고 테스트 소스는 원래 검사로 복구했다. patch 12 베이크의 HollowContent/ProductionSurfaceDataDefinitions 2 passed·오류/경고 0. Mesa08 X를 0→500으로 옮겨 8m 지면 진입 길의 오른쪽 가장자리도 비운 수정본은 Live Coding patch 15 성공, TestMap03 사용자 생성/베이크 대기다. 기존 10m 왕복 폭·비밀 통로·훅·Pod·패널은 유지한다.
- patch 15 사용자 생성/베이크·저장 완료(21:05 KST): lint=0, Support sources 108/Layers 124, TotalSamples 2268231/SupportPayload 16050998 bytes, Nav 138140 cells/749 components/2776 portals, Navigation/Traversal payload 1124756/75919 bytes, Spawn authored 8/candidates 12480/payload 501692 bytes. 좁은 Atlas 경고 2건은 남았다.
- 같은 실제 충돌 진단을 새 지형에서 반복했다. 전체 2156점 바닥 미검출 0·겹침 38·40cm 위 겹침 14(메사 아래의 기존 경사/벽 바로 옆 포함). 새 지면 진입 길 228점의 캡슐 겹침은 7→0, 40cm 높인 전체 보행 sweep의 잔여 막힘은 5→0. 일반 sweep 접촉 11건은 40cm 올린 검사에서 모두 해소됐다. 이는 Mover의 실제 턱 오르기/슬라이딩이나 네 추락 지점의 자력 복귀 검증을 대신하지 않는다. 결과 Saved/SliceOctant/Hollow_00/Patch15Capsules.csv.
- 임시 진단을 제거한 원본 HollowContent 테스트를 복구하고 Live Coding patch 17 성공, HollowContent/ProductionSurfaceDataDefinitions 2 passed·오류/경고 0. LVI/SurfaceData 저장 후 TestMap03 PIE 시작. Hollow slot 2/4/6, generation 1의 8 validated slots/557 source bindings 게시, maxSeamRadiusDelta=0.000cm·Nav resident 26.84MiB 확인. 기존 Mass CharacterMovementComponent 추출 오류와 초기 Mover 경고는 별도 사항이며 이번 지형 작업에서 수정하지 않았다. 정상 접합·패널 공간·림섬 주변 네 추락 지점의 사용자 재검토가 남는다.

### 2026-10-08 — 뒤편 Pod 외피·림 아래 지각 구멍·V자 접합·접안부 홈

- 사용자 좌표 역산: Pod 공간은 slot 6 local(2522,-7939,H1598), 그 공간에서 추락 후 갇힌 곳은 slot 4 local(3273,-8212,H-402). 나머지는 slot 6의 지각 구멍(-7223,7939,H-263), Falling 골짜기(-6079,10355,H2460), ㄱ자 홈(-12874,10054,H2498)이다.
- 수정 전 실제 충돌 진단에서 BackPocket 동쪽 벽 320개 중 80개, 서쪽 벽은 28개가 검출되지 않았다(서쪽 12점은 다른 암반으로 검출). 입구 밖 바닥 일부는 잔도 팔각 외곽이어서 미검출을 모두 구멍으로 해석하지 않는다. 굴 절단은 왕복 암반에도 적용됐지만 굴의 새 외피만 원래 절벽 MouthEdge에서 제거돼 열린 암반 공간이 남았다. 현재 큰 왕복 암반과 겹치지 않는 같은 절벽 X6000으로 BackPocket을 이동하고, 입구 잔도·Pod·검토점·훅을 함께 조정한다.
- 림 아래 255개 방사 충돌선 중 61개가 미검출됐다. 원본 Meadow 지각 자체의 local(-7400,7750) 방향도 삼각형 교차 0이며, 보고 지점(-7223,7939)은 원본 교차 1이므로 원본 구멍과 후속 절단/높이 변화의 영향 범위를 구분한다. Hollow에만 복제 지각의 닫힌 내부 경계 봉합을 적용한다. 원래 경계 정점을 이어 삼각분할하며 이음매 보존 띠에 닿는 외곽은 건드리지 않는다. 이후 공동/비밀 통로의 의도된 입구를 기존 순서로 절단한다.
- RimLowerRockJoin의 하부 경사 높이 제한이 메사에서 멀리 떨어진 경사 가장자리까지 낮춰 V자 접합을 만들었다. 메사 내부 H1598은 유지하되 메사에서 2m 벗어나면 경사 높이를 완전히 참조하는 연속 전이로 바꾼다. 보고 지점의 계산상 채움 높이는 H2278→2714로 올라 인접 경사와의 골짜기를 메운다. 접안부는 OuterEastRockJoin의 서쪽 경계를 X-12800→-13400으로 넓혀 Dock/Bridge의 ㄱ자 안쪽을 같은 H2400 바닥으로 채운다.
- 소스/테스트 복구본은 Saved/SliceOctant/Hollow_00/HollowBeforeExteriorRepair.cpp/HollowBeforeExteriorRepairTest.cpp. 임시 충돌 진단을 ExteriorRepairProbe.cpp에 보존하고 테스트 원본을 복구했다. 비교 결과는 ExteriorRepairBefore.csv. Live Coding patch 20 성공, TestMap03 사용자 생성/베이크 대기다. 새 뒤편 굴 외피·원래 지각 구멍·골짜기 접지·접안부 여유와 실제 베이크/PIE는 아직 검증 전이다.
- patch 20 사용자 생성/베이크 완료(22:00 KST): lint=0, 내부 지각 경계 1개 봉합, crust 236941 tris, parts 107/actors 214/tris 193502. Support sources 108/Layers 123, TotalSamples 2282286/SupportPayload 16147670 bytes, Nav 138672 cells/742 components/2854 portals, Navigation/Traversal payload 1129080/77423 bytes, Spawn authored 8/candidates 12509/payload 502852 bytes.
- 재생성된 BackPocket의 양쪽 벽 280점·뒤벽 350점·천장 490점에서 외피 충돌 누락 0. 림 아래 지각 방사선 255점도 이전 61점 미검출에서 전량 충돌로 바뀌었다. 굴 앞 바닥 40개 미검출은 잔도 팔각형 밖/입구 외부를 구분해야 하며 전량 구멍으로 해석하지 않는다. 결과는 Saved/SliceOctant/Hollow_00/ExteriorRepairAfter.csv다.
- 접안부 보고 지점 local(-12874,10054)는 H2400.2·경사 0.1°·캡슐 겹침 0. 주변 25점 중 6점은 Dock/채움의 남서 바닥 외곽이므로 임시 진단의 no-floor 오류를 보고 지점 미수정으로 확대 해석하지 않는다. 정식 HollowContent 검사는 임시 진단의 early return으로 아직 실행하지 않았다.
- Falling 골짜기의 실제 보고 지점은 H2718.6·경사 47.7°로, 플레이어 접지 한계 44.8°를 넘었다. 높이 채움만으로 해결하지 못했음을 확인했다. RimApproach와 RimLanding의 폭을 10→16m로 넓히고 하부 채움의 거리 계산도 half-width 8m로 맞춰 해당 위치에 기존 경사 바닥과 같은 보행 어깨를 만든다. 채택된 서쪽 왕복 경사로 10m 폭은 유지한다. 임시 진단을 별도 파일로 보존하고 정식 테스트를 복구했다. Live Coding patch 22 성공, TestMap03 사용자 재생성/베이크 대기다.
- patch 22 사용자 생성/베이크 완료(22:11 KST): lint=0, 내부 지각 경계 1개 봉합, crust 236941 tris, parts 107/actors 214/tris 193676. Support sources 108/Layers 123, TotalSamples 2287854/SupportPayload 16187085 bytes, Nav 138871 cells/734 components/2871 portals, Navigation/Traversal payload 1130720/77699 bytes, Spawn authored 8/candidates 12515/payload 503092 bytes. 좁은 Atlas 경고 2건은 남았다.
- 골짜기 보고 지점 local(-6079,10355)의 실제 바닥은 H2691.0·경사 28.9°·캡슐 겹침 0. 주변 25점 모두 경사 28.7~29.1°로 44.8° 접지 한계 이하다. 한 점(-5929,10205)은 인접 채움 바닥과 캡슐 접촉이 있어 실제 슬라이딩/보행을 확인해야 한다. 접안부 보고 지점은 H2400.2·경사 0.1°·캡슐 겹침 0이며, 6개 no-floor는 남서쪽 원래 바닥 외곽으로 구분했다. BackPocket 양쪽 벽 280점·뒤벽 350점·천장 490점 및 지각 255점의 충돌 누락 0도 유지됐다. 바닥 40점 미검출은 굴 측면의 폭 경계 바로 밖과 입구 잔도의 팔각 모서리 밖이다. 결과는 ExteriorRepairFinal.csv, 재사용 진단 소스는 ExteriorRepairFinalProbe.cpp다.
- 기존 실제 바닥/캡슐 2156점은 바닥 누락 0·겹침 62·40cm 위 겹침 12. 12점은 기존 절벽 내부/벽 바로 옆의 비보행 격자이고 새 진입 길 228점은 겹침 0이다. 동쪽 14개 경로의 일반 sweep 접촉 7건은 40cm 올린 검사에서 모두 해소됐다. 결과 ExteriorRepairRouteCapsules.csv. 실제 Mover 턱 오르기나 사용자의 자력 복귀 성공을 대신하지 않는다.
- 임시 진단을 제거하고 정식 테스트를 복구한 Live Coding patch 25 성공. HollowContent/ProductionSurfaceDataDefinitions 2 passed·오류/경고 0. 모든 slot의 Pod 투영·250cm 이상 여유·편성 요구 높이, 16 seed의 Pod 8개 전량 편성과 기존 양방향 보행 연결을 유지한다. 뒤편 Pod headroom 2646cm/투영 거리 40.5cm.
- 에셋 저장 성공, TestMap03 PIE 시작. Hollow slot 2/5를 포함한 8 validated slots의 generation 1 게시와 maxSeamRadiusDelta=0.000cm 확인. EnemyDensity 0 유지. 기존 Mass CharacterMovementComponent 추출 오류/Mover 초기화 경고는 이번 지형 검사와 별도이며 런타임 오류 0으로 기록하지 않는다. 뷰포트 캡처는 단색 암반만 보였으므로 외형 통과 증거로 쓰지 않는다. 뒤편 Pod 굴·림 지각·완만해진 접근부·접안부의 실제 추락/접지/자력 복귀와 외형은 사용자 재검토 대상이다.

### 2026-10-08 — 외피 재제작 전환 검토

- 사용자 재검토에서 서쪽 굴 입구의 깜박임/뒤쪽 메사 돌출, 동쪽 ㄱ자 홈 잔존, Rim 하부 일방 통과 후 갇힘, 외곽 구덩이의 탈출/진입 흐름 부재, 왕복 암반에서 Shuttle까지 기본 점프로 건너가 패널이 무의미해지는 문제가 보고됐다. 다섯 좌표 모두 slot 5이며 local(4431,-6952,H1598),(-12745,9977,H2533),(-7681,10270,H-402),(16054,-2930,H-1302),(4316,-9340,H6098)이다.
- 이전 충돌/데이터 통과를 실제 외피·통로 폭/자력 복귀·패널 역할 통과로 해석한 판단을 철회한다. 특히 ㄱ자 접안 굽이의 바깥을 no-floor 검사에서 제외한 것은 사용자가 원하는 안전한 굽이 공간을 검증하지 못했다. 깜박임의 정확한 중복 면 쌍과 Rim 통과 면은 미확정이다.
- 코드상 LVI 재생성은 ClearWorld 후 에셋 제자리 갱신이므로 이전 액터가 누적되지는 않는다. 그러나 최근 채움 암반 4개와 1m 이하 표면 분할을 추가했으며 기존 암반의 내부 겹침 면을 충분히 정리하지 못했다. 로그의 포털 검사 시간은 직전 173.124초(전전 172.063초), 후보 781478/727946/474964·간격 기각 472093개다. collect/extract/raster 시간만으로 전체 베이크 비용을 판단하지 않는다.
- PIE는 사용자가 종료한 상태다. Hollow 두 에셋 저장 성공 후 현재 코드/정식 테스트와 Content/Maps/Hollow_00 224개 파일(35493492 bytes)을 Saved/SliceOctant/Hollow_00/BeforeExteriorRebuild에 복구 보관했다. 새 형상은 생성하지 않았다.
- 재제작안을 phases/Phase09_HollowExteriorRebuild.md에 작성했다. 채택된 협곡/보행/훅/공동/아레나 동선을 설계도로 유지하고, 지각·외곽 암반·경사·굴 접합의 외피를 연속된 형태로 다시 만든다. 구덩이 탈출/옥탄트 진입과 Shuttle 횡단 간격도 국소 정리한다. 새 구역을 사용자에게 반복 플레이 요청하기 전에 대표 접합의 열린 경계/중복 면/실제 캡슐을 먼저 검증한다. 현재 단계는 제안/준비 완료이며 구현·생성·베이크 통과가 아니다.

### 2026-10-10 — 공통 제작 절차 개정과 Blender 반입 시험

- 사용자가 재제작/Blender 활용 판단을 맡기고, Hollow 조기 완료보다 이후 옥탄트의 설계 프로세스와 두 가이드 완성도를 우선하도록 지시했다. 복잡한 연속 암반/경사/굴은 Blender, 지각/이음매/Pod/장치는 코드로 관리한다. production 첫 교체 구역은 서쪽 BackPocket/절벽 부착 암반 접합으로 정했다.
- BuildOctantWorkflowPilot.py의 독립 V001에서 Boolean Union/Difference·구면 투영·역할 분리·FBX 반입을 시험했다. 2908 정점/5812 tris/연결 덩어리 1, non-manifold·방향 불일치·중복·퇴화 0, 구면 변 중점 오차 최대 0.166cm. Floor 1438/Shell 4374가 UE 5.8에서 그대로 반입됐고 bounds 차이는 최대 0.0037cm다. 두 BodySetup의 complex-as-simple/LOD0·별도 complex mesh 없음 확인 후 저장 성공. 기존 Blender 기본 객체를 보존했다.
- GenerateSliceOctant.py의 전역 객체/메시 삭제를 제거했다. 빈/미등록 spec은 scene 불변·출력 없음, 정상 메사 생성은 소유 collection에 Floor/Shell 출력, 반복 호출은 객체 불변으로 중단함을 실제 Blender에서 확인했다. 분류 전 face index 갱신을 추가했다. 시험편 V002의 형상 지표와 역할별 bounds는 V001과 동일했으며 같은 버전 재호출도 차단됐다. 실행 시 스크립트 폴더에 pycache를 남기지 않도록 했고 이번에 생성한 캐시를 제거했다.
- 설계 가이드를 평면/단면/연결표·자연 진입/회복·장치 역할·대표 구역·인계 양식 중심으로, 제작 가이드를 단일 외피 소유·원본/버전·구면 좌표·Blender/FBX·단계별 증거 중심으로 개정했다. 기존 Bastion 세부 이력은 Phase09_BastionGeometryNotes.md로 보존했다. Current와 재제작 계획을 갱신했다.
- 로컬 시험 산출물 Saved/OctantWorkflow/V001, UE staging /Game/Maps/OctantWorkflow/V001. production Hollow spec/LVI/SurfaceData·풀은 변경하지 않았다. 자기 교차·UE 캡슐·실제 지각 접합·베이크·PIE·정식 배경 에셋 변형은 미검증이다. 기존 다섯 사용자 문제는 미해결이며 비용 개선도 아직 주장하지 않는다. 이번 단계에서는 반복 생성/베이크 수동 요청을 하지 않았다.

### 2026-10-10 — BackPocket 서쪽 접합 구역 V012 재개

- 사용자가 서쪽 BackPocket/절벽 부착 암반 접합 하나의 재제작·검증을 요청했다. 로컬 X3800~8400/Y−8100~−3000 구역을 선택하고, 기존 10m 왕복 길·협곡 윤곽·Pod/앵커/패널·비밀 통로를 보존 대상으로 정했다. 관련 코드와 Hollow 에셋을 `Saved/SliceOctant/Hollow_00/BackPocketV001/Before`에 추가 보관했다.
- `BuildHollowBackPocket.py`로 W01·입구 잔도·부착 암반·경계를 가로지르는 WestPanel을 Union하고 BackPocket 공간을 Difference했다. 입력 삼각분할과 평면/퇴화 정리를 거쳐 V012의 닫힌 원본은 8682 verts/17364 tris/덩어리 1, 비정상 연결·방향 불일치·중복·퇴화 0이다. 네 경계의 끝막음 제거 후 11473 tris(Floor 6463/Shell 5010), 열린 경계 183개다. 최종 조립 경계가 아직 검증되지 않았으므로 전체 외피 폐쇄로 기록하지 않는다. 비인접 삼각형 BVH 교차 검출 0, 투영 오차 전체 최대0.913cm/보행면0.172cm.
- UE staging `/Game/Maps/OctantWorkflow/HollowBackPocketV012`에서 triangle 수 일치, bounds 오차 최대0.003174cm, complex-as-simple/LOD0/별도 complex mesh 없음·저장을 확인했다. V012 Floor/Shell을 production Authored 후보로 복사·저장했다. 기존 LVI/SurfaceData는 아직 갱신하지 않았다.
- 빌더에 두 Authored 메시 사전 검사와 해당 구역 안 Generated 부분 제외·묻힌 지각 절단을 구현했다. Live Coding Result: Success 확인. 제작 가이드 §6에 따라 사용자에게 TestMap03에서 생성 명령 한 번 실행을 요청했고 베이크는 아직 요청하지 않았다. 최종 조립·실제 42/96cm 캡슐·베이크·정식 데이터 검사·PIE·기존 다섯 문제의 해결은 미확인이다. 상세 인계는 `phases/Phase09_BackPocketV012.md`다.
- 사용자 생성 완료(03:19 KST): authored=2/authoredErrors=0/lint=0·LVI 저장 성공. 구역 내 Generated 절단 대상 6807 tris, crust 233467 tris, parts 107/Generated actors 208/Generated tris188029. Authored 포함 부가 지형199502 tris로 기준193676보다5826 증가했다. 같은 옵션의 베이크·포털 시간은 아직 비교하지 않았다. 기존 다른 Generated Shell의 nearly-zero normal/binormal 경고가 남았다. LVI를 열면서 발생한 거리장 빌드 시간은 베이크 시간과 구분한다.
- LVI에서 새 역할 액터 두 개의 identity·태그/profile·QueryAndPhysics·complex-as-simple/LOD0·양면 충돌 false를 확인했다. 뷰포트 캡처는 접합부가 충분히 보이지 않아 외형 통과로 기록하지 않았다. 베이크 전 실제 플레이 검토를 시도했지만 기존 SurfaceData의 액터 참조가 달라 missing runtime Support source 오류가 발생했다. PIE 종료 후 사용자에게 EnemyDensity 0 설정과 동일 옵션 베이크를 요청했다. 자동 캡슐/최종 조립 경계 미확인을 명시했으며 제작 가이드에 PIE와 베이크의 참조 의존 관계를 추가했다.
- V012 사용자 베이크 완료(03:28 KST)와 저장 확인: Support106/Layers126/TotalSamples2152613/SupportPayload15271183 bytes, Nav138037 cells/699 components/2872 portals, 후보760139/707193/466489, 간격 기각463617, 포털111.527초. Spawn authored8/candidates12459/payload500852 bytes. 같은 옵션이며 좁은 Atlas Layer 경고2건은 남았다. 기준 대비 삼각형·Layers는 증가했고 단일 베이크 시간은 감소했다.
- 정식 검사 결과는 1 passed/1 failed다. ProductionSurfaceDataDefinitions는 통과했지만 HollowContent의 Pod 천장 검사32건과 Pod8 비행 편성128건이 실패했다. Flyer01의 요구는2300+80+50+200=2630cm이고 V012의 구면 천장은 정확히2600cm로 베이크돼 기존 생성형 여유2646cm보다 낮았다. 검사 코드가 EnemyDensity를 읽지 않는 것을 확인했으므로 밀도0 때문으로 돌리지 않는다.
- V013은 바닥·입구·길을 유지하고 굴 천장만H4100→4150으로50cm 높였다. 닫힌 원본8680 verts/17360 tris/덩어리1·토폴로지 오류0; 공유 단면 끝막음 제거 후11469 tris(Floor6463/Shell5006), 열린 경계183개/예상 밖0/BVH 교차0, 투영 오차 전체0.909cm/보행면0.172cm. UE 반입 triangle 일치/bounds오차0.003174cm/complex-as-simple·LOD0·양면false 확인 후 저장했다. V012는 원본과 복구본을 보존하고 production 에셋을 UE move로 Authored/ArchiveV012에 옮겼다. 현재 Authored 바로 아래 후보는 V013 두 역할이다. 빌더 preflight/버전 로그를V013으로 변경했으며 재생성·베이크·최종 조립/캡슐·PIE는 아직 미완료다. 상세는 Phase09_BackPocketV013.md다.
- V013 Live Coding Result: Success(03:36 KST), 사용자 생성·베이크 완료. 생성 authored2/errors0/lint0·저장, crust233467/Generated188029/Authored11469 tris. 부가 지형199498 tris는 원래 기준보다5822 증가했다. 베이크 Support106/Layers126/TotalSamples2152613/SupportPayload15271126 bytes, Nav138037 cells/699 components/2872 portals, 후보760139/707193/466489·기각463617·포털151.500초, Spawn authored8/candidates12459/payload500852 bytes. 해상도·옵션 변경 없음, 좁은 Atlas 경고2건 남음. V012111.527초와의 편차도 있어 고정 성능 개선율로 주장하지 않는다.
- 정식 HollowContent/ProductionSurfaceDataDefinitions 2 passed/0 failed·검사 오류/경고0·1.711초. Pod8 layer86/headroom2650cm/edge800cm/투영11.3cm, 16 seed 전량 편성·기존 연결 통과. EnemyDensity0 확인 후 PIE 시작: Hollow slot0/4/6, 8 validated slots/551 source bindings/generation1·maxSeamRadiusDelta0.000cm. Support 참조 오류 해소, 기존 Mass CharacterMovementComponent 추출 오류/Mover 초기화 경고는 남는다. 사용자에게 HollowPocketBack0의 양쪽 입구 왕복/외형과 HollowShuttleDock0의 마지막 왕복 굽이·정상 접합 검토를 요청했다. 자동 캡슐·네 공유 경계의 전체 조립 폐쇄는 여전히 미확인이다.

### 2026-10-10 — V013 실제 통행 실패와 V014 구역 확장

- 사용자가 WhereAmI를 실행하고 한쪽 통과 후 갇힘을 보고했다. 새 에디터 세션에서 V013 로딩·8 validated slots를 확인했다. 이번 Hollow slot1/4/7이며 보고는slot4/local(3516,−9364,H2098)/발밑MountainDeparture01_Floor다. 정확한 진입 면은 사용자도 모른다. 화면에서도 암반 사이 갇힘을 확인했다. 원래 V013 영역 바깥이지만 같은 왕복 암반 접합이어서 이번 수정에 포함한다.
- PIE의 실제 WorldStatic overlap(캐릭터 중심±120cm AABB)은 MountainArrival01_Floor/Departure01_Floor/Turn03_Shell/Arrival03_Shell/Summit_Shell을 반환했다. 단순 actor bounds 조회와 구분했다. 발밑 바닥을 진입 면으로 단정하지 않는다. 기존 상층 솔리드의 겹친 벽과 내부 하층 바닥이 함께 남은 것으로 판단했다. 좌표 보존 후 PIE 종료·관련 에셋 저장, 코드/에셋을BackPocketV014/Before에 보관했다.
- V014는 기존 대표 구역의 경계를X3300~8400/Y−10000~−3000으로 확장했다. 원본10m 길·중심선·BackPocket H1500/4150·Pod/장치는 유지한다. 닫힌 원본10441 verts/20882 tris/덩어리1·토폴로지 오류0, 공유 단면 제거 후14044 tris(Floor7394/Shell6650), 경계184개/예상 밖0/BVH교차0, 투영오차0.913cm/보행면0.172cm. Blender 보고 지점 아래H2000 Floor 검출은 없어졌고 바깥→보고 지점 선은Y−9413.542에서 외향 정상 벽과 만난다. 실제 UE 캡슐/진입 경로 재현과 구분한다.
- UE staging 반입 triangle 일치/bounds오차최대0.003602cm/complex-as-simple·LOD0·양면false 확인·저장, V013을Authored/ArchiveV013에 UE move로 보존하고V014 두 역할만 바로 아래에 배치했다. C++와Blender의 동일 경계 갱신·Live Coding Result: Success 확인. 사용자에게 TestMap03의EnemyDensity0·생성 후 동일 옵션 베이크를 요청했다. V014 데이터 검사·최종 조립/자동 캡슐·실제 통행은 미완료다. 상세인계는Phase09_BackPocketV014.md다.
- V014 사용자 생성·베이크 완료 응답과 저장 로그 확인: authored2/errors0/lint0, 구역 안 Generated8823 tris 부분 제외, crust232433/Generated186054/Authored14044 tris, actors206/Pod8/anchors52/panels2. 부가 지형200098 tris는 원래보다6422/V013보다600 증가했다. Support105/Layers126/TotalSamples2139521/SupportPayload15180262 bytes, Nav137448 cells/681 components/2837 portals, 후보727967/675077/455453·간격 기각452616·포털151.595초. Spawn authored8/candidates12404/payload498652 bytes. 같은 옵션이며 좁은 Atlas 경고2건 남음. V013151.500초와 거의 같고 고정 성능 개선율로 주장하지 않는다.
- 새 세션에서 검사 발견 전 RunTests 결과0개는 통과로 세지 않았다. DiscoverTests 후 HollowContent1.579112초/ProductionSurfaceDataDefinitions0.153897초, 실제2 passed/0 failed·검사 오류/경고0. Pod8 layer120/headroom2650cm/edge800cm/투영42.2cm/group571, 16 seed 전량 편성과 기존 연결 통과다.
- EnemyDensity0 확인 후 PIE 시작: Hollow slot0/4, 8 validated slots/543 source bindings/generation1 게시, maxSeamRadiusDelta0.000cm/minNormalDot0.95296. 기존 Mass CharacterMovementComponent 추출 오류/Mover 초기화 경고는 남는다. 사용자에게 slot4/local(3516,−9900,H2000) 바깥에서 암반 접근·보행/점프·갇힘/복귀 확인과 HollowPocketBack4 입구 양 가장자리 왕복을 요청했다. 실제 응답·최종 조립/자동 캡슐·정상 접합은 미확인이다.

### 2026-10-10 — V014 통행 확인과 V015 남쪽 조립 경계 보정

- 사용자가 이전 갇힘 위치와BackPocket 입구 양 가장자리 모두 이상 없음을 확인했다. 이 범위의 실제 통행은 통과다. 이어 slot4/local(4619,−9954,H−397)/발밑Crust에서 캐릭터는 못 지나가지만 메시 내부가 보이는 세로 틈을 보고했다. 현재 화면과WhereAmI 로그를 확인했다.
- 남쪽 교체 경계Y−10000의Departure01 시작 끝면에서 기존 C++는 길 안쪽 윤곽과 절벽 채움을 나눠 생성하지만 Blender는 외측부터절벽까지 단순 사각형으로 처리했다. 낮은 높이의 교선이 약40cm 어긋났다. 독립 메시의 토폴로지·캡슐 통행 통과를 조립 외형 완료로 해석하지 않는다.
- PIE 종료, 코드/전체 Hollow 에셋을BackPocketV015/Before에 보관했다. 같은 구역·Boolean 몸체를 유지하고 해당 경계를 기존 BuildBridge의 폭 분할/삼각형 순서로 구한 실제 방사 절단선에 맞췄다. 최대 정점 이동42.899cm·꺾임3곳 추가, Floor7394/Shell6653/총14047 tris(V014보다3 증가), 경계187개/예상 밖0/방향·중복·퇴화0/BVH교차0, 보행면 투영 오차0.172cm다. 두 구간을 하나의 다각형 원본으로 바꾸려던 중간 시도는 토폴로지 검사에서 탈락해 채택하지 않았다. 최종 수정은 해당 경계 보정이다.
- C++ double 수식을 독립 재구성한4개 교선과 출력6개 경계 선분을 각21점씩 양방향 비교해 최대0.013667cm를 확인했다. 실제 UE 메시 정점 추출과 구분한다. UE staging 반입 triangle 일치/bounds차0.003602cm/complex-as-simple·충돌LOD0·별도complex없음·양면false 재조회/저장 성공. V014는Authored/ArchiveV014로 UE move했고V015 두 역할만 바로 아래에 둔다. C++ preflight/로그 버전만 갱신, Live Coding Result: Success. 사용자에게 TestMap03의EnemyDensity0·V015 생성/저장 후 같은 옵션 베이크를 요청했다. 생성·베이크·새 데이터 검사·실제 조립/통행은 미확인이다. 상세는Phase09_BackPocketV015.md다.
- V015 생성·베이크 완료/저장 확인: authored2/errors0/lint0, crust232433/Generated186054/Authored14047/부가 지형200101 tris. Support105/Layers126/TotalSamples2139522/SupportPayload15180269 bytes, Nav137448 cells/681 components/2837 portals·포털150.360초. Spawn authored8/candidates12404/payload498652 bytes, 좁은 Atlas 경고2건. 정식 검사2 passed/0 failed·오류/경고0·1.736018초, Pod8 headroom2650cm/16 seed 전량 편성 통과.
- EnemyDensity0 PIE의 Hollow slot1/3/6과8 validated slots/637 source bindings을 확인했다. 사용자에게slot1/local(4800,−9954,H−500) 외형·벽 접촉 재검토를 요청했고 “틈이 없어졌어. 이제 재제작 마저 진행하자.”라는 응답을 받았다. 해당 보고 틈의 실제 외형 통과이며 모든 벽 접촉 동작/네 경계 전체의 캡슐 검사로 확대하지 않는다. PIE 종료 후 동쪽 Dock/Rim→외곽 구덩이 복귀→Shuttle 패널 우회 순서로 이어간다.

### 2026-10-10 — 동쪽 후보 생성 중 Blender 크래시와 복구

- EastRimV001 후보 범위는X−14000~−7000/Y7800~12000이며, 동쪽 접안 굽이의 팔각 채움과Rim 하부 접합을 합친다. 수정 전 전체 Hollow 에셋/코드를후보 Before에 보관했다. UE 반입·새 Live Coding·생성·베이크는 아직 없다.
- 사용자 크래시 로그는 Blender5.2 Exact Boolean의trimesh_nary_intersect→boolean_mesh write-null 접근 위반이다. 분리 생성한 원본에서Ridge04 중복20474면/Apron 중복237514면과 Boolean 전 prism 검사 누락을 확인했다. 이 입력이 크래시를 유발했을 가능성이 크다. 중점 공유/명시적 삼각형 분할과 원본 전량 검사를 적용하고 원본·operand별 진행 기록을 남긴다.
- 백그라운드 순차 Boolean9회는 크래시 없이 완료했다. 구면 공면 합치기의 얇은 오류 때문에 local Exact Boolean→투영 경로를 채택하고 같은 정점/반대 법선의 내부4면을 쌍으로 상쇄했다. 최종 닫힌 몸체12540 verts/25080 tris/오류0·출력24064 tris(Floor4542/Shell19522)/경계146개·예상 밖0/BVH교차0, FBX·blend·manifest 저장 성공. UE/C++ 단면·투영 오차·최종 조립은 별도다.
- 사용자가 Blender와Codex를 재시작했고 MCP 읽기 호출 성공을 확인했다. 복구 장면만 append해 기존 기본 Scene을 보존했다. 두 역할을 합쳐 재조회한 비정상 변146개 모두 처방 경계이며 내부 비정상 변0이다. 화면 캡처는 시작 팝업과 아래쪽 시야 때문에 외형 승인 근거로 삼지 않는다. 상세는Phase09_EastRimV001.md다.

### 2026-10-10 — 동쪽V004 베이크 연결 실패와V005 경사 수정

- 재시작 후 Blender/UE 읽기 호출 성공,TestMap03/PIE 종료를 확인했다. 기존Rim 길3개의 밑면 폭 분할을 끝막음과 일치시키고 독립 double 단면을 재구성했다. V004는24951 tris/예상 밖 경계0/BVH 교차0, 단면 최대X0.353784cm/Y0.002037cm다. 반입 bounds 최대차0.003907cm/triangle 일치/complex-as-simple/양면false를 확인하고Live Coding patch_2 성공 후 생성·베이크를 요청했다.
- 사용자 생성·베이크 완료와 저장 로그 확인: authored4/errors0/lint0, crust231529/Generated177089/부가 지형216087 tris. Support101/Layers133/TotalSamples2128908/SupportPayload15106927 bytes, Nav137071 cells/667 components/2744 portals, 후보694113/643741/434216/간격 기각431472/포털138.868초. Spawn authored8/candidates12349/payload496452 bytes, 좁은 Atlas 경고10건이다. 단일 측정으로 고정 성능 개선율을 주장하지 않는다.
- HollowContent1.687440초 실패: Ridge04→안부 양방향2건이며 다른 외곽/비밀 경로와편성은 통과했다. 잘못된 이름으로 실행되지 않은 두 번째 검사는 통과로 세지 않고 올바른Runtime.ProductionSurfaceDataDefinitions를 실행해0.166275초에 통과했다.
- 실제NavigationPayload를codec 그대로 해석해 새Floor layer121의 주요component가X−10950의 경사 윗끝에서 갈라짐을 확인했다. 같은Nav 좌표41개에서50cm Support 샘플/양방향 고정Up의 하단 구 진단을 수행해V004의 침투35개/최근접 최솟값48.264cm(반경50cm)를 재현했다. Chaos 연속 캡슐/실제플레이어 검토와 구분한다.
- V005는OuterEastBridge 양 끝 높이·폭·중심선을 유지하고 cubic smoothstep 높이/100cm 분할로 경사를 완만하게 바꾸며옆 암반 높이를 맞췄다. 닫힌 원본오류0·최종26931 tris(Floor5044/Shell21887)/경계169/예상 밖0/BVH 교차0, 같은41개 진단 침투0/최솟값50.246cm, 단면 오차는V004 수준이다.
- 기존V004 조립/베이크와코드를V005/Before에 보관하고V004 에셋을Authored/ArchiveEastRimV004로 UE move했다. V005 반입/생산 후보 재조회: bounds 최대차0.00390625cm/triangle 일치/LOD1개/충돌LOD0/별도complex없음/complex-as-simple/양면false. C++ preflight/로그 버전만 변경해Live Coding patch_3 성공. 사용자에게 생성·베이크를 요청했고V005 생성 저장authored4/errors0/lint0·부가 지형218067 tris를 로그에서 확인했다. 새 베이크·정식 검사·PIE는 미확인이다. 상세는Phase09_EastRimV005.md다.

### 2026-10-10 — V005 시작부 연결 실패와V006 경사 연장

- V005 사용자 생성·베이크 완료와 저장 로그 확인: Support101/Layers129/TotalSamples2128908/SupportPayload15107260 bytes, Nav137075 cells/669 components/2744 portals, 후보694123/643751/434221·간격 기각431477·포털139.687초. Spawn authored8/candidates12348/payload496412 bytes, 좁은 Atlas 경고6건이다.
- 정식2개 실제 실행: HollowContent1.637988초 실패(같은 능선 왕복2건), ProductionSurfaceDataDefinitions0.160125초 통과. 새manifest에V005 반입을 확인했다. 새Nav는 윗끝 연결이 열렸지만X−12557~−12176 시작부가 분리됐다. 실패142개 진단 침투110/최근접 최소49.512cm로 재현했다. 이전 윗끝41개만의 진단 통과를 경사 전체로 확대하지 않는다.
- V006은 전체 진입축 양 끝/폭/Rim 치수를 유지하고 내부 경사 도착을X−10300으로 옮긴다. 상승1850→2500cm/수평 도착500cm/길이 분할50cm 이하, 옆 암반 같은 높이 함수다. 닫힌 원본오류0·최종30975 tris(Floor5279/Shell25696)/경계169/예상 밖0/BVH 교차0, 단면 오차는V004 수준이다.
- N669 경사 시작부~도착부 내부 보행 폭X−13100~−9950/Y9650~10350의565개 연결을 진단해하단 구 침투0/최소50.719cm를 확인했다. BVH 샘플 진단이며 최종 UE 캡슐/PIE와 구분한다.
- V005 조립/베이크를V006/Before에 보관하고V005를Authored/ArchiveEastRimV005로 UE move했다. V006 반입/생산 후보의triangle·bounds차0.00390625cm·LOD/충돌 설정 재조회, Live Coding patch_4 Success. 제작 가이드 §6의 수동 생성·베이크를 요청했다. 상세는Phase09_EastRimV006.md다.
- V006 사용자 생성·베이크 완료와 저장 로그: authored4/errors0/lint0, Generated177089/부가 지형222111/crust231529 tris. Support101/Layers133/TotalSamples2128909/SupportPayload15107897 bytes, Nav137090 cells/666 components/2744 portals, 후보694126/643754/434223·간격 기각431479·포털203.433초. Spawn authored8/candidates12348/payload496412 bytes, 좁은 Atlas 경고9건이다. V005보다포털63.746초 증가했고동시 distance field build 로그가 있어 단일 측정을 고정 형상 비용으로 단정하지 않는다.
- HollowContent1.912384초/ProductionSurfaceDataDefinitions0.191497초, 실제2 passed/0 failed·검사 오류/경고0. 동쪽능선↔안부 Found2363/3699 expansions, 다른 외곽/비밀 경로와편성 검사 통과, Pod8 headroom2650cm 유지다.
- EnemyDensity0 확인 후PIE 시작: Hollow slot2/5/6, generation1/8 validated slots/625 source bindings/registry generation2, Nav runtimeLayers910/localComponents3095/runtimeStaticComponents2249/seamLinks4330/blocked98nodes·4edges/navResident26.93MiB. 사용자에게HollowOuterEast2 능선→Dock 굽이→새 경사→Rim 섬 왕복/가장자리 점프와slot2/local(−7800,12500,H0) 하부 바깥 접근·복귀·외형 확인을 요청했다. 응답·최종 조립 전체/자동 캡슐은 미확인이다.

### 2026-10-10 — V006 사용자 지각 접합 실패와 부피 절단 후보

- 사용자 보고: 틈 입구 바닥의 작은 시각적 구멍, 이어진 벽 접근 시 통과 후 틈에 갇혀 Falling 지속/탈출 불가. WhereAmI는slot2/local(−10444,9248,H−428), floor none이며 좌표는Pawn 중심이다. 정식2개/경사 내부565개 진단 통과를 실제 외벽·지각 접합 통과로 확대하지 않는다.
- 동쪽은 경사 밑동의 넓은 방사 윤곽 전체에서 지각을 제거했다. 서쪽의3D 부피 절단 방식을 적용하고V006의25m smoothstep/50cm 길이 분할/도착부 경계를 맞췄다. 비공면 옆면의 두 삼각형 평면을 모두 쓴다. 기존Blender V006 에셋과서쪽V015는 유지한다.
- 단순 부피와 최종 외피의 비교는2,708개 중1cm 초과 바깥224개/최대10.926cm다. 옆 절단 평면을15cm 몸체 안쪽으로 넣은 후보는2,712개 중바깥0/광선 판정 불일치0/최소 매입3.626cm다. H−1000~1000·각 구간 중점 양옆의BVH 표본이며 실제 지각 조립/Chaos 캡슐 완료가 아니다. 원본 코드·LVI·SurfaceData·지각은V006/GroundJoinBefore에 보존했다.
- Live Coding patch_5 Success. PIE를 종료하고 제작 가이드 §6에 따라BuildSliceOctant만 수동 요청했다. 실제 지각 조립 확인 후 베이크·정식 검사·사용자 재검토가 남는다.

### 2026-10-10 — 사용자 지시로 Hollow 전체 단순화 재제작 전환

- 사용자는 정식 아트 전 프로토타입에서 정교한 쉐입/접합 보정과 높은 폴리곤 수를 우선할 필요가 없다고 판단했다. 후보 구역뿐 아니라Hollow_00 전체를 간결한 폴리곤 조립으로 재제작하도록 지시했다. 기존 쉐입의 큰 변경과세부 동선 조정을 허용한다. 잔도의ㅗ 단면은닫힌 직육면체 판/누운 삼각기둥으로바꿀 수 있다.
- 구멍·일방 통과·탈출 불가를 우선한다. 작은 보정 메시 반복 대신큰 덩어리의위치·폭·간격과동선을 바꾼다. 함정 해소에과도한 변경이필요하면 탈출 훅 앵커를허용한다.
- Blender 제작·반입 절차는검증됐지만최종조립·충돌완료는아니다. 전체 재제작 후생성·베이크·자동 검증 이터레이션을먼저 마치고사용자PIE는마지막에묶는다. 제작중정적검사와복구저장은유지한다.
- 직전 동쪽 국소 수정후보의수동Build 요청은보류했다. C++/Live Coding에는patch_5 후보가있고저장조립/베이크는서쪽V015+동쪽V006 사용자실패본인중간상태를명시했다. 추가생성·베이크·형상수정은진행하지않았다.
- `Phase09_HollowLowPolyRebuild.md`를새실행기준으로작성하고Current/기존외피계획/두가이드를갱신했다. 새세션재개를권장했다. 비교기준은지각231529+부가지형222111=453640 tris, Support101/Layers133/포털2744다. 폴리곤감소만으로성능향상을확정하지않고같은옵션비용을비교한다.

### 2026-10-10 — Hollow 전체 단순화 V010 반입, 베이크 오류 수정

- 작업 전 전체 Hollow 에셋과 두 C++ 파일을 `LowPolyV001/Before`에 보존했다. 내부 외피를 `BuildHollowLowPoly.py`의 단순한 몸체·경로 단면·순차 Boolean으로 대체하고 지각/4개 판형 부유섬/Pod8·장치는 코드로 분리했다. 옛 Hollow 부품 spec과 두 국소 교체 함수는 제거했으며 다른 작업의 dirty 변경은 유지했다.
- V003 생성 성공: crust111514/Generated3096/Authored8/errors0/lint0. 베이크는 공동 북서쪽의 Support 정점 좌표32767cm 초과로 실패했다. L자 공동과 뒷길 배치로 상한을 지켰고 모든 Support 출력 정점 검사로 고정했다. 버전별 원본·진행 기록을 보존했다.
- 경로 전체 제작 검사에서 회전 cutter의 단차·협곡 간섭·횡경사를 발견했다. 경로가 단면을 공유하도록 바꾸고 산의 수평 회전 구간, 동쪽 우회, 뒷길 고도 분배와 큰 협곡 윤곽을 조정했다. 런타임 이동/양면 충돌이나 작은 보정 메시로 우회하지 않았다.
- V009 생성 성공 뒤 Pod1 아래 Support 보간이 없어 베이크가 실패했다. 열린 역할 조각에서 공통 메시 생성 helper가 법선을 다시 계산한 것이 원인이다. V010은 닫힌 외피의 면 순서를 보존하고 모든 역할 면의 분리 전후 법선 일치를 검사한다. 제작 가이드에 이 실패 조건을 반영했다.
- V01064430 tris/닫힌 성분1/위상 오류0/공유 정점 없는 BVH 교차0, 역할 합계·winding·Support 좌표 상한 통과. 100cm 간격/5개 트랙 바닥6455곳 누락0·최대 면 경사34.393°. UE 역할8개 triangle 일치/bounds차 최대0.005555cm/충돌LOD0/별도complex 없음/complex-as-simple/양면false를 재조회·저장했다.
- Live Coding patch_3 성공 후 제작 가이드 §6에 따라 V010 생성·베이크를 요청했다. 이후 정식 HollowContent에 실제 Chaos 경로 캡슐 5개 트랙과 지각 접합 경계 왕복 검사를 추가해 patch_4도 성공했다. **V010 생성·베이크·정식 자동 검사·사용자 PIE는 아직 미확인**이다. 이전 SurfaceData를 새 형상 통과로 취급하지 않는다. 새 배치·연결표·한계는 `Phase09_HollowLowPolyV010.md`다.

### 2026-10-10 — 전체 V010 검사 실패 수정, V016 통과 후 V017 장치 접근 보강

- V010 생성/베이크 성공 뒤 HollowContent270건 실패. 지각 소유 경계 밖 램프, 산 정상과 겹친 Floor 띠, 비밀길 성긴 투영, 북쪽 출구를 덮은 램프 밑면, Pod8 천장2600cm를 실제 로그/충돌 좌표로 좁혔다. 통로 바닥6455/Chaos 캡슐12730은 통과했으므로 이 결과를 전체 접합 완료로 확대하지 않았다.
- V016은 외피 경계를 넓혀 램프를 감싸고 북쪽 축을(2500,9800)으로 옮겼다. BackPocket 높이3300cm, Floor 띠/좁은 진입부 격자, 가느다란 삼각형의 구면 법선 왜곡을 수정했다. 소스67564 tris/닫힌 성분1/위상 오류0/역할 winding 보존, 통로6455·접합2592·탈출 광선10 통과. UE bounds차0.005738cm/8개 충돌설정 확인.
- V016 생성 lint0/베이크 성공. 정식 HollowContent+ProductionSurfaceDataDefinitions 성공2/오류0/경고0/3.649초. 지각94224+코드3096+Blender67564=164884 tris, 기준453640 대비63.65% 감소. Support12/Layers24/TotalSamples2154328/payload15156224 bytes, Nav cells185566/components25/portals1155/candidates157971/152069/81893, seam1107. 폴리곤 감소와 샘플/payload 감소는 같은 의미가 아니며 성능 향상을 확정하지 않는다.
- 추가 장치 검사: 앵커26개 대표 접근 Visibility/개별 거리, 패널2개3트랙 양방향 탑승 캡슐. 패널264회 통과, 앵커6개 가림/1개 거리 실패. 북쪽 중간 판이 좁힌 입구 벽에 묻힌 것도 확인했다.
- V017에서 앵커 접근면/거리·동굴 하부 앵커·북쪽 판 전체를 품는 출구 폭을 수정했다. 원본67766 tris/정식 소스검사 통과, UE8개 반입·충돌·Live Coding patch_9 성공. 수동 생성/베이크 요청 중. 최종 자동 검사·PIE는 아직 미확인이다. 최신 실행 상태는 Phase09_HollowLowPolyV017.md이며 PIE는 전체 후보가 자동 검사를 통과한 후 한 묶음으로 요청한다.

### 2026-10-10 — V017 베이크·장치 포함 자동 검증 완료, 마지막 PIE 요청

- 사용자 V017 생성·베이크 완료. 생성 lint0/8 Authored/errors0, 지각94224+코드 판3096+Blender67766=165086 tris. 기준453640 대비63.61% 감소. Support12/Layers24/TotalSamples2154328/payload15156232 bytes, Nav185567 cells/25 components/1155 portals, 후보157971/152069/81893, seam1107, Nav/traversal1503884/37707 bytes, Spawn8/candidates12704/payload510652 bytes.
- 포털 베이크152.292→9.994초(93.44% 감소), Support payload는0.32%/TotalSamples1.19% 증가. 배치가 바뀐 동일 옵션 콘텐츠 비교이며 런타임 성능/메모리 감소로 확대하지 않는다.
- HollowContent와ProductionSurfaceDataDefinitions 성공2/오류0/경고0/합계3.606초. 경로 바닥6455/5트랙 양방향 Chaos 캡슐12730, 접합 바닥2592/왕복 캡슐, 탈출구2개, 앵커26 접근·개별 거리, 패널 탑승264회 통과. 8slot 회전·16seed의64개 encounter별4Flyer+6Crawler 편성 유지, Nav 왕복과Shuttle 정적 단절도 통과.
- Saved/SliceOctant/Hollow_00/LowPolyV017/unreal-verification.json에 검증 결과를 보존하고Current/실행 문서/두 가이드에 실패 원인과증거 범위를 반영했다. 전체 후보의 마지막 PIE 묶음을 요청했다. 실제 조준/당기기/해제/착지/낙하 복귀/패널 왕복/Pod 교전의 사용자 확인 전이므로 형상 확정·Phase9 단위3 완료로 기록하지 않는다.

### 2026-10-10 — V017 외피 사용자 확정, 장치 조정 V018 검증 완료

- 사용자는 맵 전체를 면밀하게 탐색해 메시 틈/한쪽 면/깨진 형상을 발견하지 못했고 외피를 확정했다. CaveLift 지하 착지/지상 복귀, Basin↔절벽 정상 왕복, Shuttle 패널 왕복도 확인했다. NPC 교전 성공으로 확대하지 않는다.
- 후속 요청에 따라 잔도별 훅과 이전 착지점→다음 훅의 사거리를 정리했다. 중앙 공동 Pod 전용 훅은 중앙 출구 아래 훅과 통합하고 Rim 복귀 훅을 추가해 전체26개를 유지했다. Shaft 위/아래, BackPocket/Dock, 양쪽 잔도는 서로 다른 착지 단계라 유지했다.
- Shuttle 섬/Pod5/검토점은(−9000,−14500,H6000)으로 이동해 Dock 검토점과 실거리111.85m를 확보했다. Hollow 전용 패널 BP로 두 패널만400×400→1200×1200cm/두께30cm, 접안 위치/높이를 조정했다. CaveMesa 검토점은 실제 지면(1800,9800,H0)으로 수정했다. 내부 V017 역할8개는 SHA256 변동0이다.
- Live Coding patch_11/사용자 생성 lint0/베이크 성공. Support12/Layers24/TotalSamples2154508/payload15157500 bytes, Nav185568/25/1155/seam1107, 포털10.483초. 정식 HollowContent3.469초+Runtime.ProductionSurfaceDataDefinitions0.191초 성공/오류0/경고0. 패널9트랙 양방향 캡슐2214회, 접안4끝/크기, 훅26+Rim3방향, Mesa 바닥/텔레포트 캡슐, 기존 통로/접합/8slot/16seed64편성/Nav/Shuttle NoPath 통과.
- 복구와 결과는 `Saved/SliceOctant/Hollow_00/DevicesV018/{Before,source-preflight.json,unreal-verification.json}`. 최신 실행 문서는 Phase09_HollowDevicesV018.md다. 자동 검증을 마쳤으며 변경 구간의 마지막 사용자 PIE 묶음을 요청한다. Phase9 단위3 전체 완료는 아직 기록하지 않는다.

### 2026-10-10 — V018 훅 PIE 실패, V019 착지·역할·패널 중력 회전 수정

- 사용자는 패널 크기를 채택했지만 협곡 훅이 중구난방이고 여러 훅 아래 잔도에 착지할 수 없다고 보고했다. V018 접근선 검사에는 방사 낙하/첫 바닥 검사가 없었다. 전역 시드 훅도 수동 훅에 섞였다. 자동 검사 통과를 실제 장치 완료로 확대하지 않는다.
- 협곡은 잔도9/정상2/중앙 축 탈출1, 나머지 외곽 역할7개로 전체26→19개. 역할 이름을 마커에 기록하고 중복/불명확한7개를 제거했다. Hollow에 수동 전용 태그를 붙여 같은 스트리밍 레벨의 추가 시드 훅을 제외한다. 런처와 다른 레벨의 정책은 유지한다.
- 사용자 좌표는 slot7의 서쪽 절벽이다. 섬 위치는 유지하고 패널을 그 절벽과 직선32.37m로 연결한다. 열린 왕복 판은 양 끝 중력 수평 quaternion을 거리 비율로 보간하며 body/snapshot 각속도·접촉점 이탈 속도를 반영한다. 기존 틱 순서와 복제 경로/시각을 사용한다.
- 원본19접근 가림0/거리초과0,16착지 첫 바닥 오차최대3.951cm/발 디딜64점, Shuttle→WestTop82.21m/가림0. 회전 판9트랙540점 최소 캡슐 축 거리72.4/73.2cm, 접안4끝8/10/5/5점. 승인 V017 외피는 수정하지 않았다.
- 첫 Live Coding의 런타임 패치는 반영됐지만 에디터 패치는 새 DLL export 링크 오류로 실패했다. 공유 자세 함수를 inline으로 두어 런타임patch_1/에디터patch_12 링크 성공을 확인했다. 패널 크기 CDO12/12/0.3 재조회도 일치한다. 8slot 자세/왕복/정차/각속도 검사는 통과했고 가장자리 속도 비교의 float 키 차분을 보완해 재검사 중이다. **생성·베이크·정식 콘텐츠 검사·수정 후 PIE는 대기.** 최신 기준은 `Phase09_HollowDevicesV019.md`다.
- 후속 런타임patch_2/에디터patch_13 성공. 가장자리 속도 비교를 허용오차0.1cm/s 그대로 ±10ms 중심 차분으로 보완한 패널 회전 검사도 성공0.028초/오류0/경고0이다. V017 내부8개 SHA256 일치, BP 크기 유지, PIE 종료를 확인하고 사용자 Lint/Build/Bake 묶음을 요청한다. 생성 후 실제 Chaos 콘텐츠 검사는 아직 대기다.
- 사용자 V019 생성·베이크 완료. lint0/authored8/errors0/hooks19/panels2/pods8/review29. Support12/Layers24/TotalSamples2154508/payload15157500 bytes, Nav185568/25/1155/seam1107, 포털10.834초. 정식 HollowContent3.430초+ProductionSurfaceDataDefinitions0.191초 성공/오류0/경고0. 훅19접근/16착지/발 디딜64점·낙하 캡슐, 회전 판9트랙 왕복1044회/접안4끝/크기, 기존 외피/접합/축/Nav/Pod 편성 검사를 통과했다. 승인 내부8개 SHA256도 일치한다. 결과는 `DevicesV019/unreal-verification.json`이다. 실제 시드 훅 제외/훅 궤적/회전 base 탑승은 아직 확인하지 않았으며 마지막 변경 구간 PIE 묶음을 요청한다.

### 2026-10-11 — V019 패널 사용자 채택, 잔도 관성 착지·추가 복귀 V020

- 사용자는 호스트·게스트 모두 패널 왕복/회전이 의도대로 동작한다고 확인했다. 훅 정돈은 개선됐지만 모서리에 몰린 착지점에서 종료 관성으로 잔도를 놓쳤다. 긴 정상2곳과 외곽 추락5곳에 추가 훅을 요청했다. V019 PIE 로그 authoredRegion146과 정돈 개선을 확인했으며 전역 anchors38/50을 Hollow 개수로 해석하지 않는다.
- V020은 기존 잔도9훅의 위치/높이·사거리를 조정하고 정상2/외곽 복귀4역할을 추가해 전체19→25개로 만든다. 가까운 동쪽 보고2곳은 안부 훅 하나로 통합했다. 모든 보고 좌표는 slot0이다. 벽 밀착 지점의 캡슐 접근을 위해 원점과2~4m 물러난 Launch 검토점을 구분했다. 서쪽 겹친 잔도는 짧게 걸어 접근 각도를 바꾸는 출발점을 기록했다.
- BP 정지150cm/종료600cm/s·중력2000·무입력 감속200·종단2000을 반영한 원본 캡슐 축5점 근사 낙하547사례 통과. 변경17훅 접근선/사거리·첫 착지면·주요13훅400cm 바닥, 대표 출발점 주변100cm 도착 여유를 검사했다. 접근 캡슐 축 최소 거리50.6cm(반경42cm). 합성 공중 진입은 몸체 착지만 확인하며100cm 추가 여유까지 통과했다고 기록하지 않는다.
- 승인 V017 내부8개 SHA256 일치. 섬/패널과 이동 런타임은 유지했다. Live Coding 에디터 patch_0~2 성공, 최종 patch_2 링크 오류 없음. `DevicesV020/Before`에 복구본과 검사 원본/JSON을 보존한다. **사용자 Lint/Build/Bake를 요청했으며 실제 조립 Chaos·정식2개 검사와 변경 훅 PIE는 대기다.** 최신 기준은 `Phase09_HollowDevicesV020.md`다.
- 첫 V020 사용자 생성 lint0/베이크 완료(12Support/24Layer/2154508샘플/185568Nav셀/25훅/40검토점/165086tris). 데이터 검사 성공1.092초. 실제 Chaos652관성낙하에서 WestMid5방향이 협곡 바닥으로 떨어져 HollowContent 실패3.303초/오류5/경고0. 원본 근사 접근 판정의 경로 제외가 실제 Chaos보다 많아 놓친 사례였다.
- WestMid를(-2500,−2850,H3800)으로 낮추고 안쪽으로 옮겼다. Pocket 출발은(-4200,−2800,H−2500), Mid→High는 약4m 오른쪽에서 당긴다. 지형을 유지한 임시 마커 후보 Chaos25접근/22착지/643관성낙하/1044패널 sweep 성공3.248초/오류0/경고0. 임시 transform은 검사 후 복구했고 임시 적용 코드를 완전히 제거했다. 에디터 Live Coding patch_5 성공. 수정본 Build/Bake와 저장 조립 정식2검사 대기, PIE 보류다. 실패·후보 검사 JSON은 DevicesV020에 보존했다.
- WestMid 수정본 사용자 재생성·베이크 완료. 생성 lint0/25훅/2패널/8Pod/40검토점/8반입 오류0, 베이크12Support/24Layer/2154508샘플/185568Nav셀/165086tris. 임시 위치 적용을 제거한 저장 조립 정식2검사 성공(HollowContent3.233초,ProductionSurfaceDataDefinitions0.160초,총3.393초/오류0/경고0). 훅25접근/22착지/643관성낙하, 패널1044 sweep, 외피6455바닥/12730왕복5트랙 sweep·접합2592바닥 및 기존 Nav/Pod 확인. 승인 V017 내부8개 SHA256·패널 BP 일치. 최종 JSON·생성/베이크 로그를 DevicesV020에 보존했고 변경 훅 마지막 PIE 한 묶음을 요청한다.
- 2026-10-11 사용자 마지막 변경 구간 PIE 응답: “변경 구간 검토 완료, 문제 없음.” 요청한 잔도 체인/Panel 분기의 관성 착지, 추가 정상 훅2개, 외곽 복귀4역할(동쪽 보고2곳 통합)의 검토 묶음을 통과했다. V017 전체 외피 채택·V019 호스트/게스트 패널 채택·V020 저장 조립 정식2검사와 이번 PIE를 근거로 Hollow 전체 단순화 재제작·장치·훅 조정을 완료 처리한다. NPC LoS·풀 통합·부하 측정을 완료한 것으로 확대하지 않는다.

### 2026-10-11 — Hollow 단순화 방식의 Bastion 적용 인계

- 사용자는 Hollow 전체 폴리곤 단순화에 높은 만족도를 밝히고 Bastion에도 같은 방식을 적용할 수 있도록 새 세션용 문서 갱신을 요청했다. 이번 세션은 문서만 수정했고 Bastion 코드/메시/LVI/SurfaceData는 변경하지 않았다.
- `phases/Phase09_BastionLowPolyRebuild.md`를 작성했다. 큰 닫힌 몸체·내부 면 제거·구역 소유권·구면 분할/법선·Floor/Coarse/Shell 분리와 Hollow 비용/실패 사례를 정리했다. Bastion의 정상/나선·성벽/성채·빈 안뜰/지붕·공동/섬·Pod/훅의 보존 기준, 기준선/복구부터 마지막 PIE까지의 실행/통과 조건을 명시했다. 기존 BastionEncounters는 편성 검사이며 형상/착지 검사를 대신하지 않는다고 기록했다.
- 두 공통 가이드에 재사용 기준과 다음 적용 링크를 반영하고, 남아 있던 Hollow 미완료/다음 국소 재제작 안내를 완료 이력으로 바로잡았다. Current·README 라우팅·Phase9 단위2의 후속 체크·Bastion 채택 기록과 Hollow 완료 문서를 연결했다. 다음 세션 첫 작업은 Bastion 현 원본/수동 마커/에셋 확인·백업과 자기 비용 기준선/구역 교체 목록 확정이다. 승인 Hollow 보존, 베이크/자동 검사 선행·마지막 PIE 묶음, NPC/풀 조합/패키지/부하 Gate 구분은 유지한다.

### 2026-10-12 — Bastion 광원1차 비용 실측·감축 후보 평가 V004

- 최신 Current에서 BastionV001~V003·HollowV020의 제작/장치/사용자 PIE 완료를 확인하고 광원 평가 한 단위를 진행했다. 사용자 요청대로 MCP 연결을 요청했으며 연결 후 TestMap03에서 작업했다. 기존 DefaultGame.ini·ActorMix 편성 에셋·LNPInputHandlerComponent.cpp의 사용자 변경은 건드리지 않았다.
- Bastion81개×3slot=243개 포인트 광원을 확인하고 기존/전체 소등/기존 반복을 공동·성채 북문·고층 건물에서 측정했다. 실제 플레이어를 기존 검토 명령으로 리스폰하고 안정화·스크린샷 처리를 CSV6초 구간과 분리했다. GPUTime 중앙값의 소등 차이는 공동·성채 북문 약0.4~0.6ms, 고층 건물 약0.12~0.13ms다. 임시 ViewTarget을 따르지 않은 최초 카메라 시험은 폐기하고 같은 CSV를 유효한 플레이어 이동 시험으로 덮어썼다.
- 성채/외성벽 문8곳을 모두3→1개로 합치는65개안은 성채 문 바닥 계산상 최대38% 저하로 제외했다. 외성벽 문4곳만 합치는73개안은 반경4917.5cm·unitless 세기0.274311647배로 바닥 조도 비율0.9449~1.0288을 맞춰 PIE에서 비교했다. 외성벽 문 `GPU/Lights`는0.6575/0.6604→0.5694ms로 줄었지만 GPUTime은6.5214/6.3887→6.5300ms였고 상부 벽 조명이 약해졌다. 다른3시점도 전체 GPU 이득이 작거나 기존 반복 편차와 겹쳐 후보를 미채택했다. 모든 수치·방법·한계는 `phases/Phase09_BastionLightingV004.md`가 소유한다.
- 실제 PIE viewport1039×543·RTX4060 Laptop·WindowsEditor 조건이며 스크린샷만1280×720이다. 약60fps 프레임 제한 때문에 frame Gate로 쓰지 않으며 최종 해상도 패키지·멀티플레이·전원 활성 측정을 대체하지 않는다. 광원 소등은 플레이어 랜턴·발광 구슬·환경광을 끄지 않았다. 세기0 후보는 Actor/구슬 제거의 CPU 비용을 측정하지 않았다.
- 최종 광원243개 개수/세기/반경 복원 불일치0, LVI 복구본 SHA256 일치. GPU CSV 진단 비활성화·PIE 종료·TestMap03 유지. 코드·에셋 변경이 없어 빌드/재베이크/형상 회귀·동일 사용자 PIE 재검토는 생략했다. 승인 광원81개 유지, 추가 감축 필요성은 단위5 패키지 GPU 측정에서 재판단한다. 복구·원본CSV21개·캡처·요약·복원 증거는 `Saved/SliceOctant/Bastion_00/LightsV004`와 `Saved/Profiling/CSV/LightsV004_*.csv`에 둔다.
- Current·Phase9 실행 문서의 오래된 Bastion 미착수 안내/체크를 실제V003 완료 상태로 정리했다. 다음은 사용자 진행 확인 뒤 단위4 풀 통합/여러 seed의8-slot 조합·이음매·게시·결정론·stale 차단 검증이다. 별도 세션 소유인 벽보행 LoS 수정과 NPC 교전, 단위5 패키지/부하 Gate는 유지한다.

### 2026-10-12 — 풀 검증 구현·production 검증 V005

- 사용자 진행 승인 뒤 production 풀의 Meadow·Bastion·Hollow와 전체 slot 허용을 확인했다. 기존 slot 순서 greedy 선택은 유지했다. `Runtime.ProductionSurfaceDataDefinitions`를1→32seed로 확대해0/−1/int32양끝을 포함한 선택 재현·허용 slot·Surface/Nav generation·12seam·이전 generation node 거부를 검사한다. 각 seam의1/4·1/2·3/4에서 양쪽 실제 내부 node 사이 양방향 A*를 실행한다. 불필요했던 내부 node의6방향 열린 edge 조건을 제거한 최종 검사에서32배치·definition×slot24·방향 조합9·2304경로·최대 높이차0cm를 통과했다.
- `Bake.ProductionPoolCurrentAndDeterministic`를 추가해 세 source를 각각 두 번 구워 source hash와5payload stream의 결정론·저장본 최신성을 검사했다. 기준선124검사에서는118통과/6실패와 Meadow stale source/Spawn 오류가 나왔다. 사용자 ActorMix 설정은 유지하고 Meadow SurfaceData만 재베이크했다. Regression/Conditional fixture의 미등록Bench01_2·비행형 기대3/현재9·Crust fixture 저장본 최신성 문제는 별도로 남겼다.
- Live Coding에서는 source의 실제 Support11/100/12개가 있는데 extractor 수집0 오류가 나왔다. 진단 삽입 위치 오류로 컴파일도1회 실패해 수정했다. 사용자 요청대로 에디터 종료를 즉시 요청했고 종료 후 `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 전체 빌드가89.67초/오류0으로 성공했다. 임시 진단은 제거했다. 새 에디터에서는 수집0 오류가 사라졌으며 정확한 원인은 미확정이다.
- 새 빌드 production2검사 성공293.984초/오류·경고0. 세 source 두 차례 베이크·저장본의 source hash/5stream 일치,32seed runtime 검사 성공5.854초다. 같은 빌드의 기존124회귀 재실행은118통과/같은6실패195.108초이며 Meadow stale 오류는 해소됐다. 두 배치의 고유125검사는119통과/6실패로 전체 Gate는 미통과다. 결과는 `production-results.json`·`regression-results.json`·`combined-results.json`이다.
- TestMap03 PIE 고정seed−1/135792468/2147483647 모두 phaseComplete·8LVI·Support8/8·seam4281·Surface/Nav/registrySurface generation1 일치로 PASS. bindings269/269·357/357·357/357, Nav component/group589/589·709/709·709/709이다. 기존 listen host 한 world의 게시 검사이며 guest/late join/패키지2P Gate를 대신하지 않는다. 실제 slot 배열과 로그는 `Saved/SliceOctant/PoolV005`에 보존했다.
- PIE 종료·TestMap03 유지·원래CDOseed0/EnemyDensity1 복원 확인. 자동화가 저장한 테스트3에셋은 unload 후 HEAD 바이트로 복구했고 dirty가 된 테스트BP/CDO 전파 GameStateBP도 디스크에서 다시 읽었다. 최종 dirty content/map0, 사용자 기존 DefaultGame.ini·ActorMix·InputHandler 변경은 보존했다. Current·Phase9 실행 문서·V005 결과를 갱신했다. 풀 검증 구현 단위를 마치고 사용자 확인 뒤 fixture6실패 정리를 다음 단위로 제안한다. Phase9 전체·패키지/부하 Gate는 미완료다.

### 2026-10-12 — 혼합 풀 SourceKeys·정식 편성 배치 부족 수정 V009

- 사용자 진행 지시로 V008 잔여 2건을 다뤘다. `ProbeSourceKeys`를 slot별 게시 snapshot의 저장 source 표와 대조하도록 바꿨다(혼합 풀에서 slot 간 key 동일 가정 제거).
- 새 `Runtime.ProductionSpawnComposition`이 패키지 배치(WorldSeed1)에서 2130/2235를 Editor로 재현했다. 원인은 반경 안 같은 Layer Enemy 후보가 편성27보다 적은 좁은 Layer에 랜덤 Pod를 둔 것과 일부 후보 경합이다.
- `BuildPlan`을 수동 앵커 선택→수동 Pod 적 배치→편성 전량 배치 가능한 랜덤 Pod 순으로 바꿨다. 수정 후 패키지 배치2235/2235, 33배치77,629/77,629, Pod 부족0.
- 전체127검사 통과(경고 포함4·실패0/513.5초). 입력 에셋7개 해시 유지, 테스트 생성 에셋3개 HEAD 복구. 게이트 통과 BuildCookRun exit0/104.6초·1229packages.
- 동일 패키지1P exit0/42.7초: 적2235/2235·Pod120/120, SourceKeys 358/358·패널·FaceIndex·SurfaceData PASS, UnknownHits/EnvelopeEscapes/LayerJumps0. exact P95 4.536ms FAIL은 성능 단위에 남긴다. 상세 [V009](../phases/Phase09_SpawnCompositionV009.md).

### 2026-10-12 — 패키지 리슨2P·late join V010

- V009 커밋(ca60758) 뒤 착수. 패널 자세 계산을 `EvaluatePose`로 분리하고 `ProbePanelPoses`·`ProbeWorldDevices`를 추가해 host/guest 로그를 스크립트로 대조했다.
- unity 묶음 변경으로 드러난 `LNPSurfaceCrawlTest.cpp`의 `FParams` 모호성을 한정자로 고쳤다. 전체127검사 실패0(경고 포함5/489.6초), DynamicTerrain 검사 통과, 입력 에셋 해시 유지·테스트 생성 에셋 복구.
- 첫 Normal 대조는 장치 개수 207 vs 50으로 FAIL이었다. 장치 net cull distance 300m에 의한 relevancy로 확인하고 규칙을 host 대응·guest 폰 250m 내 완전성으로 바꿨다(Attempt1 보존). 폰 위치 로그를 더해 재패키징했다.
- Normal·LateJoin(+60초) 모두 통과: snapshot 일치, 패널11 자세 차0, 장치 미대응0·근처 누락0, probe5종 양측 PASS, 적2434/2434. host exact P95 3.42/4.09ms FAIL은 성능 단위에 남긴다. 상세 [V010](../phases/Phase09_NetworkV010.md).

### 2026-10-12 — 성능·관통·메모리 Gate V011

- Final7과 같은 `RunPhase13Gate.ps1 -Cases Production`으로 V010 패키지를 측정했다. 성능2회 frame P95 9.17/7.76ms·경로 PASS, AlwaysActive2회 19.88/20.20ms(참고). exact >2ms FAIL은 기준선 Final7도 같은 FAIL(4.13/4.30ms)이라 Phase3 목표값으로 분류했다.
- 관통4회에서 벽보행 약340틱이 나와 사용자에게 보고했다. 사용자는 Meadow가 원인인지 물었고, 옥탄트별 실패가 아님을 설명한 뒤 Meadow를 풀에 유지하기로 했다(재제작은 양산 단계).
- 첫 진단(패널에 붙은 휴면 개체의 운반 누락)으로 지상·벽보행 휴면 운반 query를 넣었다. 빌드·전체127검사 실패0·재패키징 후에도 같은 개체(939)·위치·깊이20.77cm로 관통해 진단이 틀렸음을 확인하고 되돌렸다. 운반 접촉이 있으면 이미 강제 활성이다.
- 정정 원인: Hollow slot2 Shuttle 패널 접안 벽에 붙은 휴면 벽보행을 패널이 덮는다(HitDotSurface 1.000, 운반 접촉 없음). 휴면은 정지 겹침 검사를 하지 않는다. 사용자 결정으로 수용(D-078).
- harness에 `dormantPenetratingEntityFrames`를 추가했다. 에디터 빌드·전체127검사 통과(경고 포함4/491.2초)·입력 해시 유지·테스트 에셋 복구·BuildCookRun exit0/118.3초. V011B 관통4회 휴면290/338/343/339·활성3/5/3/2틱(≤0.174cm)·비행0, 성능2회 7.65/9.13ms PASS.
- 메모리 기준선(옥탄트별 payload·Nav·삼각형, 런타임 resident)을 기록했다. 상세 [V011](../phases/Phase09_PerfMemoryV011.md).

### 2026-10-12 — 벽보행 LoS 수정 반영 문서 동기화

- 사용자 확인으로 Phase9 문서의 "별도 세션 LoS 수정 대기"가 오래된 상태임을 정리했다. 수정은 `0ab5e4f`(2026-10-10)이며 설계·Roadmap·Phase14b 계획 문서는 그 커밋이 갱신했다. Current·Phase9 실행 문서·V011의 대기 문구를 완료와 남은 재검증으로 바꿨다.
- 수정 뒤 Hollow를 V017~V020으로 재제작했으므로 D-017 Shuttle/서쪽 절벽 양방향 교전은 현재 지형에서 다시 확인한다. 사거리 밖 추격 우회는 Phase 14b 범위다.
- 사용자 확인(2026-10-12): Hollow 재제작으로 Shuttle 섬↔서쪽 절벽 거리가 멀어져 이 교전 사례는 더 이상 시험할 수 없다. 대신 LoS 수정 당시 유사 상황을 재현한 다른 지형에서 벽보행 교전을 사용자 PIE로 모두 확인했고 동작 문제는 없었다. 세부 튜닝은 남았으며 벽보행 교전 시작 거리(AttackRange)를 늘리는 것이 좋겠다는 의견이다. 수치를 바꾸면 성능 측정 2회로 재확인한다(V011 측정 중 교전 벽보행은 평균 약 24기).
- 교전 거리 상한 측정(V011C): 사용자 요청으로 벽보행 `AttackRange`를 인지 거리와 같은 5,000으로 임시 변경(MCP)해 전체 cook한 패키지로 성능2회를 측정했다. frame P95 9.11/9.28ms·경로 PASS·exact P95 3.28/3.15ms, 투사체 평균 약405(800일 때 280). 에셋은 800으로 되돌려 원본 SHA256 일치. 상세 [V011](../phases/Phase09_PerfMemoryV011.md) §2.

## 2026-10-12 — Phase 9 완료

- 완료 조건: 실규모 블록아웃 옥탄트 2개(Bastion_00·Hollow_00)를 제작·베이크하고, 3종 혼합 풀에서 회귀·cook·패키지1P·리슨2P/late join·성능·관통·메모리 Gate를 통과했다. 근거 표는 [Phase 9 실행 문서](../phases/Phase09_VerticalSlice.md) §5다.
- 테스트 증거: 전체127검사 실패0(경고 포함4, 491.2초, `Saved/SliceOctant/GateV011B`), 게이트 통과 BuildCookRun exit0, 리슨2P Normal·LateJoin 양측 exit0·대조 통과(V010), 성능4회 frame P95 7.65~9.28ms·경로 PASS, 관통4회 활성2~5틱·비행0(V011), 교전 거리 5,000 성능2회 통과(V011C).
- D-017: 벽보행 LoS 수정 `0ab5e4f` 당시 유사 지형 사용자 PIE로 확인. Shuttle 사례는 재제작으로 시험 불가.
- 결정: D-078(휴면 접안 겹침 수용). 사용자: Meadow 유지·재제작은 양산 단계, 벽보행 교전 거리는 플레이 테스트로 결정.
- 다음 Phase는 사용자 확인 뒤 착수한다(본선 10, 분기 14b).
