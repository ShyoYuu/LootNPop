# Phase 9 정식 Spawn 편성 재검증 V006

> 상태: 완료 — 정식 편성 기준 고유125검사 통과(전체 실행 + 영향 범위 재검사)
> 날짜: 2026-10-12
> 범위: 사용자 정식 편성 확인, 테스트 에셋 이름 변경, fixture 회귀 실패 정리

## 입력 정정

사용자가 `DA_MassSpawnConfig_ActorMix`는 종류별 한 마리 테스트 편성이며 정식 편성은 `DA_MassSpawnConfig`라고 확인하고 기본 설정을 복원했다. V005의 ActorMix 사용 기록은 당시 테스트 입력의 결과로 보존한다. fixture 입력을 별도 설정으로 분리하는 변경은 하지 않았다.

에디터 Settings의 `MassSpawnConfig`와 DefaultGame.ini 모두 정식 에셋을 가리키고, `LNP.Spawn.ConfigOverride`는 빈 문자열이다. 정식 편성 에셋은 수정하지 않았다.

## 변경

- 테스트 에셋을 `/Game/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies`로 이름 변경·저장했다. 변경 전 에셋 참조0, 변경 후 옛 경로/redirector 없음. 모든 편성·거리·성능 속성이 변경 전과 동일하다. Bench01의 근접 PureEntity·Actor 승격 근접·원거리·벽보행·비행은 각각1, Pod 세트 수120이다.
- 정식 편성에서 stale이 확인된 `DA_OctantSurface_Fixture_Crust`·`DA_OctantSurface_Fixture_Regression`·`DA_OctantSurface_Fixture_Conditional`을 재베이크·저장했다. Conditional은 전체 실행에서 별도 에셋임을 확인해 추가했다. Meadow는 정식 편성의 최초 재검사에서도 최신성 통과라 추가 재베이크하지 않았다.
- `Bake.VolumetricFixture`의 실제 공동 Pod 검사에서 고정 비행3마리 기대값을 정식 config의 요청 수량으로 바꿨다. 비행 편성이 양수인지와8slot 모두 authored Pod를 받는 검사는 유지한다. 독립 합성 fixture의3마리 기대값은 유지한다.
- 활성 검증 편성 안내의 이름과 용도를 수정했다.

## 검증

- 정식 편성 최초6개 재검사:3통과/3실패,126.378초. `Bench01_2` 미등록 오류는 사라졌다. 남은 실패는 fixture2개의 저장 source/Spawn stale 및 VolumetricFixture의 기대3/실제9였다. `initial-six-results.json`에 기록했다.
- 에디터 이름 변경·명시적 저장 성공, 변경 전후5개 속성 묶음 완전 일치.
- Crust/Regression/Conditional `BakeOctant` 모두 성공. 변경 전 세 에셋과 테스트 편성을 복구본으로 보존했다.
- 수정 후 dirty content/map package0, `git diff --check` 통과.
- 사용자 에디터 종료 후 `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 전체 빌드 성공(36.60초/컴파일 오류0). 수정한 VolumetricFixture를 컴파일·링크했다. 로그는 `full-build.log`다.
- 새 에디터의 전체125검사:124통과/1실패,454.154초. 남은 ConditionalPatch.RegressionFixture는 별도 Conditional 에셋의 저장 source/payload stale이었다. 최초에 재베이크 대상에서 빠뜨린 에셋이며, fixture 자체나 런타임 결함은 아니었다. `full-results-before-conditional.json`에 기록했다. 실행 MCP 요청은300초에 timeout됐지만 자동화는 계속 실행됐고 완료 후 GetTestResults로 최종 결과를 확보했다.
- Conditional 재베이크 후 영향 범위7개 전부 통과,32.866초. `conditional-results.json`에 기록했다. 전체 실행과 재검사의 최신 결과를 합친 고유125검사 전부 통과/실패0이다. `combined-results.json`에 두 배치를 명시하며 단일 실행125통과로 표시하지 않는다.
- production 전체 베이크 최신성/결정론248.666초·runtime32seed5.010초·VolumetricFixture0.353초 모두 오류·경고0. 세 production source의 두 차례 베이크와 저장본 source hash/5stream이 일치한다.
- 전체 배치에는 기존 RHI 가상 리소스 예산 경고, HTTP generate_204 timeout, BOption authoring world가 현재 맵이 아니라 저장 추출본을 검사한다는 경고가 있었다. 경고0으로 기록하지 않는다.
- 자동화가 저장한 테스트 에셋3개는 unload 후 HEAD 바이트로 복구하고 SHA256 일치를 확인했다. dirty BP_Octant_COption을 다시 읽고 임시 월드 GC·TestMap03 복귀로 자동화가 만든 외부 actor dirty도 해소했다. 최종 dirty content/map0·PIE 종료 상태, 테스트 편성 속성 재확인·diff check 통과. 진행 중 별도로 변경된 `.gitignore`와 기존 InputHandler 변경은 보존했다.

## 다음 행동

정식 편성 fixture 회귀 정리 단위를 완료했다. 사용자에게 다음 단위 진행 여부를 확인한다. 다음은 단위5의 cook freshness 차단·Development 패키지 검증이며, 이후 패키지1P/리슨2P·late join·성능·메모리 Gate를 수행한다. 이 문서의 자동화 통과를 Phase9 전체 완료로 해석하지 않는다.

로컬 근거·복구본은 `Saved/SliceOctant/SpawnConfigV006`이며 Git에 포함하지 않는다.
