# Phase 9 풀 통합 검증 V005

> 상태: 당시 테스트 편성의 풀 검증 기록 — 정식 편성의 전체 회귀 Gate는 후속 V006에서 통과
> 날짜: 2026-10-12
> 범위: 단위4의 Meadow·Bastion·Hollow 풀 조합, 결정론, 이음매, 저장 데이터 최신성

> 후속 정정: 사용자가 ActorMix는 종류별 한 마리 테스트 편성이며 정식 편성은 `DA_MassSpawnConfig`라고 확인하고 기본 설정을 복원했다. 아래 결과는 당시 테스트 편성에서 얻은 기록이다. 정식 편성 재검증과 테스트 에셋 이름 변경은 [V006](Phase09_OfficialSpawnConfigV006.md)을 따른다.

## 변경

- 기존 풀의 세 definition·SurfaceData 연결과 전체 slot 허용을 유지한다. 선택기는 기존 slot 순서 greedy이며 제한 mask를 도입하는 D-043 구현은 이번 범위에 없다.
- `Runtime.ProductionSurfaceDataDefinitions`를 고정 seed1개에서32개로 확대했다. 0·−1·int32 양끝·기존135792468 및 고정 난수 stream의 seed를 사용한다. 같은 seed의 선택 재현, 허용 slot, Surface/Nav generation, 12개 world seam, stale node generation 차단을 검사한다.
- 각 seam의1/4·1/2·3/4 표본 양쪽에서 실제 격자 내부 이웃을 찾아 양방향 A*를 실행한다. 내부 이웃에6방향 열린 edge를 요구했던 최초 검사 전제는 불필요하므로 제거했다. 표본이나 경로를 건너뛰지 않는다.
- `Bake.ProductionPoolCurrentAndDeterministic`를 추가했다. 실제 production source를 두 번 구워 source hash와5개 payload stream의 결정론, 저장 에셋과의 일치를 검사한다. CI 자동화에서 production stale을 실패로 드러낸다. cook 자체의 차단 확인은 단위5에 남긴다.
- 현재 사용자가 설정한 `DA_MassSpawnConfig_ActorMix`를 유지한다. 기준선에서 Meadow의 저장 source hash·Spawn payload만 현재 재베이크와 달라 Meadow SurfaceData를 재베이크했다. 지형/LVI와 사용자 설정은 수정하지 않았다.

## 검증 결과

- 변경 전 전체 SurfaceNavigation:124개 중118통과/6실패. 회귀 fixture의 미등록 `Bench01_2`, 현재 ActorMix의 비행형9개와 fixture 기대3개의 차이, Crust/Meadow 저장 source·Spawn 최신성 차이 때문이다. 원본 결과는 `Saved/SliceOctant/PoolV005/baseline-results.json`이다.
- Live Coding 재검증: runtime production 검사 성공5.922초/오류·경고0. seed32·배치32·definition×slot24·방향 있는 definition쌍9·왕복 경로2304·최대 seam 높이차0cm.
- 재베이크 검사는 Live Coding 후 세 source의 Support 수집이0으로 실패했다. 별도 진단에서는 유효 actor30/446/110, transient actor각2, 실제 Support component11/100/12를 확인했다. **전체 빌드·새 에디터에서는 수집 실패가 사라졌다.** Live Coding 실행 상태와 관련된 것으로 보지만 정확한 원인은 미확정이다. 저장 에셋 오류로 기록하지 않으며 임시 actor 진단 코드는 제거했다.
- 사용자가 저장하지 않고 에디터를 종료했다. 자동화가 저장한 테스트 에셋3개는 생성본을 `AutomationGenerated`에 보존한 뒤 HEAD 바이트로 복구했다. 사용자 기존 변경인 DefaultGame.ini·ActorMix·InputHandler는 보존한다.
- 전체 빌드 `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 성공(89.67초/컴파일 오류0). 임시 진단을 제거한 최종 소스가 빌드됐다. 로그는 `full-build.log`다.
- 새 에디터의 production2검사 성공/오류·경고0/293.984초. 세 옥탄트의 두 차례 전체 베이크와 저장본이 source hash·Support·Navigation·Traversal·Spawn·ConditionalPatch에서 일치한다. runtime32seed 재검사는5.854초, 전체 베이크 최신성/결정론 검사는288.130초다. `production-results.json`에 기록했다.
- 같은 전체 빌드에서 기존124회귀를 다시 실행했다(195.108초).118통과/6실패로 기준선과 같은 검사들이 실패하며 Meadow stale 오류는 사라졌다. production 배치와 회귀 배치의 고유 검사125개를 합치면119통과/6실패다. `regression-results.json`과 `combined-results.json`은 두 배치 결과임을 명시한다. 새로운 실패 검사는 없다.

| 남은 fixture 검사 | 실패 이유 |
|:---|:---|
| CrustAtlasExactError·LayerAtlasExactError·ConditionalPatch.Bake·ConditionalPatch.RegressionFixture | Regression/Conditional fixture가 현재 ActorMix에 없는 `Bench01_2`를 참조 |
| OctantBakeDeterministic | Crust fixture의 저장 source/Spawn이 현재 설정과 다르고 Regression fixture에 같은 미등록 ID가 있음 |
| VolumetricFixture | 현재 ActorMix의 비행형9개와 fixture 기대3개가 다름 |

stale 차단은 실제 전후 기록으로 확인했다. 변경 전 `OctantBakeDeterministic`이 Meadow의 저장 source hash·Spawn 불일치를 실패로 검출했고, 재베이크 후 같은 오류가 사라졌으며 새 production 최신성 검사도 통과했다. 이전 generation의 Nav node 참조는32seed 모두 거부됐다. cook 자체의 freshness 차단 확인은 단위5에 남긴다.

## 실제 PIE 게시

TestMap03에서 CDO seed만 임시 지정하고 적 밀도0으로 게시를 검사했다. seed0은 순수 선택 검사에 포함되며 GameMode의0=랜덤 의미를 바꾸지 않는다.

| 고정 seed | bindings | Nav component/group | 결과 |
|---:|---:|---:|:---|
| −1 |269/269|589/589|PASS|
|135792468|357/357|709/709|PASS|
|2147483647|357/357|709/709|PASS|

각 세션은 phase Complete·8개 LVI·Support 조회8/8·seam link4281, Surface/Nav/registrySurface generation1 일치다. 세 seed의 slot 배치가 다르며 각 세션에 세 지형이 모두 포함된다. 증거는 `pie-<seed>.json`과 `pie-publication.log`다. 기존 PIE 설정의 listen host(NetMode2) 한 world에서 검사했으며 guest·late join·패키지2P Gate를 대신하지 않는다.

PIE 종료·TestMap03 유지, 원래 CDO seed0/EnemyDensity1 복원 확인. 자동화가 저장한 테스트 에셋3개는 해당 패키지를 unload한 뒤 HEAD 바이트로 다시 복구했다. 자동화가 dirty로 만든 BP_Octant_COption과 임시 기본값 전파로 dirty가 된 BP_LNPGameState만 디스크에서 다시 읽었으며 dirty content/map package0이다. 사용자 기존3파일은 보존했다.

## 다음 단위

당시에는 fixture 입력 분리를 다음 단위로 제안했으나, 사용자의 정식 편성 확인으로 [V006](Phase09_OfficialSpawnConfigV006.md)의 `DA_MassSpawnConfig` 재검증으로 변경했다. 전체 회귀 Gate가 통과한 뒤 단위5의 cook·패키지·2P·성능·메모리 검증을 진행한다.

로컬 산출물은 `Saved/SliceOctant/PoolV005`이며 Git에 포함하지 않는다. 단위5의 패키지/2P/성능 Gate와 NPC LoS·D-017 검증은 별도다.
