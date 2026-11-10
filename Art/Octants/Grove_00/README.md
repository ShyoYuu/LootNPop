# Grove_00 로우폴리 프로토타입

**현재 정식 풀은 Meadow를 제외한5종이다(2026-11-10).** Bastion·Hollow·Acropolis·Pyramid·Grove를 사용하며 Meadow 원본/테스트 에셋은 보존한다. 정의·Spawn Nav 투영은 최초 통과했고, 스폰 편성은 고정 회귀 배치의 Meadow 의존 조건 보정·Editor 빌드 후 재검사를 통과해 관련3개 항목을 검증했다. PodOverlay는 이번5종 구성에서도 동일 샘플 선택 assertion으로 실패했다. 기존 Pyramid 단독에서도 이미 재현된 항목으로 남기며 해당 검사 입력 검토가 후속 과제다. [풀 변경 상세](MeadowRemoval.md). 아래6종 수치는 Grove 통합 당시의 기록이다.

2026-11-10. 사용자 승인된 V040 지형·V042 앵커30개·지하 조명47개를 `/Game/Maps/Grove_00`에 채택하고 실제 `/Game/Maps/OctantPoolData`에 등록했다. 기존5종을 보존한6종 풀이며 Meadow 제외나 Ridge 제작은 포함하지 않는다. 정식 아트 적용 전 동선 검증용이다.

거목7개(100~170m), 거대한 뿌리벽8개와 절삭 통로·경사로, 지상 공터 순환, 가지와 정상 보행 접합, 약133×138m의 깊은 지하 공동을 포함한다. 지형·훅은 사용자 왕복 검토를 통과했다. 지하의 남은 어두움과 이동 중 밝기 변화는 사용자 결정으로 수용하고 조명/노출을 더 조정하지 않는다.

최종 형상 원본은 `Grove_00_V040.blend`, 형상·역할·해시 기록은 `Grove_00_V042_Manifest.json`, 확정 배치는 `Placement.json`과 `UndergroundLights.json`이다. `Grove_00_V042_GeometryProof.json`은 원본 검사 근거이며 실제 엔진/플레이 검증과 구분한다. 총433,250 triangles, Floor40,213·Shell334,105·Crust58,932다.

`Scripts/Blender/ExportGroveOctant.py`를 별도 Blender background에서 실행하면 원본 SHA256·역할별 삼각형 수·bounds를 확인하고 FBX/조립 입력을 `Saved/SliceOctant/Grove_00/ProductionExport`에 복원한다. 확정 형상을 재설계하거나 기존 에셋을 덮어쓰지 않는다. `GroveRootNetworkSpec.py`·`BuildGroveRootNetwork.py`는 제작 경위와 새 버전 생성 경로다.

정식 콘텐츠는 LVI·DA·지각 메시·Authored Floor/Shell·전용 재질2개, 총7개다. 정식 에셋 의존 패키지10개에 `_WorkInProgress` 참조가 없다. 구버전 UE 콘텐츠는 `Content/Maps/_WorkInProgress/Grove_00`, 이전blend12개와 당시 README는 `Art/Octants/_WorkInProgress/Grove_00`에 복구 보관한다. V040 원본은 이 폴더에 유지한다.

등록은 준비(`Scripts/FinalizeGroveOctant.py`: 별도 UE Python commandlet)와 라이브 풀 저장(`Scripts/IntegrateGrovePool.py`: ProgrammaticToolset)을 나눈다. 열린 에디터가 실제 풀 파일을 잠그므로 별도 프로세스에서 풀을 저장하지 않는다. 등록 후 검사는 `Scripts/RunGroveValidation.ps1`로 실제 풀의 `LootNPop.SurfaceNavigation` 전체를 실행한다. 이 검사는 반복 베이크를 포함하며, 재저장하는 MeshTerrain 두 개와 Schema DA 한 개 시험편을 시작 시 깨끗했던 Git 원본으로 복원한다.

통합·검사 근거와 변경 전 복구본은 `Saved/SliceOctant/Grove_00/Production`에 있다. 정식 맵의 native 광선 검사(착지368·여유322·Pod 머리 공간72·앵커 접근46)와 혼합 풀 PIE8slot 게시·seam 높이 차0cm·Pod120/120·적2,087/2,087 배치를 확인했다. 혼합 PIE의 차단 seam node49/edge2는 관측값으로 기록하며 seam 높이 불일치와 구분한다. 사용자는 요청한 경계 왕복과 Pod 교전 확인에 문제가 없었다고 답했다. 전체135개 실행 결과 최초133개 통과·2개 실패, Grove 저장해시 재베이크 후 해당 현재성 검사를 재통과해134개 검증·PodOverlay1개 잔여 실패다. 전체 자동 검사 모두 통과를 뜻하지 않는다. 상세 기록은 [정식 통합](Production.md)을 따른다.
