# Phase 4a 로그 — 지각 Support Atlas와 옥탄트 이음매

## 2026-09-27

### 착수와 범위 결정

- 실행 문서 `phases/Phase04a_CrustAtlasAndSeams.md`를 작성했다.
- 사용자 결정 세 가지:
  - 4a에는 Phase 4 공통 전제 중 지각 관련만 넣는다. 섬·동굴 fixture 재배치, 동굴 키트, Conditional Patch 개념 설계는 4b 착수 전으로 미룬다.
  - 지각 Layer는 세 이음매 변 모두에 닿는 유일한 Support 컴포넌트로 식별한다(D-055).
  - 4a는 지각만 굽는다.

### 구현 단위 0 — 수집기 보강

- `LNPOctantSourceCollector`에 다음 검증을 추가했다.
  - 무태그 충돌 컴포넌트를 오류로 보고한다. 해당 컴포넌트를 모두 모아 한 번에 보고한다.
  - LVI 안의 비-Static 수명주기를 차단한다.
  - tag와 profile의 `LNPSurfaceSupport`·`LNPWorldExact` 응답 일치를 검증한다.
  - Decoration 컴포넌트의 profile과 태그 배타성을 검증한다.
- 예외 두 가지:
  - 모든 레벨의 기본 builder brush(`Brush_0`)는 충돌이 켜져 있어 제외했다. BSP·볼륨 brush는 그대로 오류다.
  - 에디터로 레벨을 열면 transient `SmartObjectSubsystemRenderingActor`가 붙는다. transient 액터는 저장되지 않으므로 source에서 제외했다.
- 기존 `SourceDependencyCollection` 테스트는 `BlockAll` profile을 쓰고 있어서 LNP profile로 바꿨다.
- 음성 사례 11건 `Schema.SourceContractValidation`과 실제 LVI 검증 `Schema.ProductionOctantSourceValidation`을 추가했다.

### `Meadow_00` 태그 마이그레이션

- 새 검증에서 `BP_Octant_Meadow_00`의 무태그 충돌 컴포넌트 4개가 드러났다. 지각 `StaticMesh`와 PCG HISM 3개다. 섬 컴포넌트는 이미 태그가 있었다.
- 지각 SCS 템플릿에 Support+Blocker+Static을 넣었다(profile `LNPStaticTerrain`).
- `PCG_Octant_BaseProps` Static Mesh Spawner의 `TemplateDescriptor.componentTags`에 Blocker+Static을 넣었다(profile `LNPStaticBlocker`). PCG Seed를 42→43→42로 바꿔 재생성하고 LVI 외부 액터를 저장했다.
- 런타임 코드는 이 태그를 읽지 않는다.

### 지각 fixture LVI

- 에디터 명령 `LNP.SurfaceNav.BuildCrustFixture`를 추가했다. 이 명령이 `/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Crust`를 생성한다.
  - 지각은 완전 구면 옥탄트 패치다(N=96, 삼각형 9,209개). 구멍 1.5°로 삼각형 7개가 빠졌다.
  - 분리 sheet, 양면 판, 음수·비균일 scale 슬래브, `Probe_*` 7개를 둔다.
- 새 FAutoConsoleCommand를 새 파일에 넣으면 Live Coding은 링크만 하고 컴파일하지 않는다. 에디터를 닫고 UBT 빌드가 필요했다.
- 헤드리스 `-ExecCmds`는 쉼표로 명령을 나눈다. 세미콜론을 쓰면 전체가 한 명령으로 처리돼 아무것도 실행되지 않는다. 또 에디터 바이너리는 `Quit`로 종료되지 않아 프로세스를 직접 끝냈다. `Automation RunTests ...; Quit`는 자동화 명령이 세미콜론을 스스로 해석하고 종료한다.

### 검증

- `LootNPopEditor Win64 Development` 빌드 성공
- 자동화 `LootNPop.SurfaceNavigation` 26/26 통과. 에디터 MCP와 헤드리스 둘 다에서 돌렸다.
