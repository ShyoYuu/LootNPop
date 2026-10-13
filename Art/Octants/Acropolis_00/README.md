# Acropolis_00 제작 원본

현재 지형 원본은 `Acropolis_00_V015.blend`, 장치 배치 버전은 **V016**이다. 사용자는 9개 경계 복귀로 모두에서 점프·훅 없이 경계에 도달하는 것을 확인했다. 이후 신고한 외곽 구덩이 한 곳에 탈출 훅을 추가했다. 지형은 그대로이며 기존 훅 28개에 `PitRecovery_01` 하나가 더해졌다. 최신 변경과 검토 방법은 `.context/SurfaceSupportNavigation/phases/Phase09_AcropolisV016.md`에 기록했다.

2026-10-13 사용자가 구덩이에 빠진 뒤 새 훅으로 원활하게 탈출하는 동작까지 PIE에서 확인했다. **Acropolis_00의 지형·이동 동선 프로토타입은 확정 상태**다.

- `manifest.json`: 지형 치수·역할별 FBX·토폴로지·배치 의도.
- `placement.json`: Pod·훅·대표 출발 지점의 옥탄트 로컬 좌표.
- `geometry-verification.json`: 이음매 정점 표본·원본 바닥·연결 시야 검사.
- `ready-verification.json`: 현재 V016 저장본의 마커 자세·Pod 바닥·훅 시야·베이크 경계 검사.
- `pit-recovery.json`, `pit-recovery-verification.json`: V016 탈출 훅과 검토점의 배치 원본, 하부 시야·이동 공간·상단 착지면 검사.
- `bake-summary.json`: 생성된 DA의 stream 통계와 실제 풀 파일 해시.
- `native-seam-fix.json`: 원본 UE 정점으로 보존 띠를 복원한 결과.
- `formal-validation.json`: 혼합 풀·스폰·저장 베이크 현재성의 정식 검사 결과.
- `exit-verification.json`: 9개 복귀로의 경사·폭·바닥 검사와 하단 검토점 좌표.
- `gap-verification.json`: 신고된 틈의 최종 광선 검사.
- `asset-audit.json`: 커밋 전 최종 에셋 참조·중간본 참조 배제·풀 변경 범위 검사와 파일 해시.
- `OctantDefinition.txt`: UE가 내보낸 풀 정의 구조체. 실제 풀의 새 항목에 붙여넣는 용도.
- `OctantDefinition.json`: 같은 정의의 필드별 입력값.

최종 콘텐츠 에셋 68개는 LVI·DA 2개, 메시 63개, 재질 3개이며 모두 최종 LVI/DA의 참조 그래프에 포함된다. `_WorkInProgress` 참조와 리다이렉터는 0개다. 실제 풀의 변경은 기존 정의 3개를 보존하고 Acropolis 정의 하나를 추가한 것뿐이다.

UE 중간 에셋과 검사용 풀은 `/Game/Maps/_WorkInProgress/Acropolis_00/`에 둔다. 이전 진단 이미지·PIESmoke 로그·최초 조립/반입 보고서 등 11개 파일은 `Art/Octants/_WorkInProgress/Acropolis_00/Diagnostics/`로 옮겨 Git에서 제외했다. 과거 단계 문서의 해당 파일 경로는 당시 위치이며, 현재 최종 원본과 검증 데이터만 이 폴더에 남긴다. FBX·구버전 원본·실행 로그·복구본은 기존 Git 제외 작업 공간 `Saved/SliceOctant/Acropolis_00/`에 보존한다.

현재 맵은 `/Game/Maps/Acropolis_00/LVI_Octant_Acropolis_00`, 베이크된 DA는 `/Game/Maps/Acropolis_00/DA_OctantSurface_Acropolis_00`이다. 실제 `OctantPoolData.OctantDefinitions`에 이미 등록되어 있다. 전체 8개 회전 허용값은 255, SeamSignature는 `DysonSphere_R300m_V1`이다. 사용자는 V005의 전체 구성과 훅 배치를 확인했다. V009 변경 구간은 `TestMap03`의 PIE에서 `AcropolisUpper_02`(ㄱ자), `AcropolisUpper_03`(2단), `AcropolisPocket_02`(굽은 동굴)를 `LNP.Debug.TeleportToReview`로 검토한다.

재현은 `Scripts/ExportAcropolisSeam.py`로 기존 Meadow 지각을 내보낸 뒤 `Scripts/Blender/BuildAcropolisOctant.py`, `VerifyAcropolisOctant.py` 순서로 진행한다. 생성 스크립트는 기존 버전 덮어쓰기를 막으므로 새 버전 번호를 선택하고 검사·조립 스크립트의 버전 경로도 함께 맞춘다.

최초 에디터 조립은 `Scripts/AssembleAcropolisOctant.py`를 Unreal MCP `ProgrammaticToolset.execute_tool_script`로 실행한다. `ACTION`은 `import` → `geometry` → `markers` → `verify` 순서다. 현재 V015의 `Scripts/ReviseAcropolisOctant.py`는 기존 Geometry Actor와 모든 게임플레이 마커를 보존하면서 지각과 검토점만 갱신한다. 복귀로 검사에는 `Scripts/Blender/VerifyAcropolisExits.py`를 사용한다. 첫 제작에서 복제한 fixture 원본은 수정하지 않았다.

공용 C++ `BuildSliceOctant`에는 아직 등록하지 않았다. 지형 반입 뒤에는 `Scripts/FinalizeAcropolisSeam.py`로 경계 띠를 원본 Meadow의 native 정점에 맞춘다. V016의 `Scripts/RepairAcropolisPit.py`를 실행해 탈출 훅과 검토점을 반영하고 `Scripts/BakeAcropolisOctant.py`로 DA를 굽는다. 훅만 바꿀 때는 지형 재반입·경계 보정이 필요 없다. `Scripts/RunAcropolisValidation.ps1`은 실제 풀 대신 검사용 사본으로 네이티브 자동 검사 3개를 실행한다. `Scripts/VerifyAcropolisReady.py`는 라이브 에디터에서 최종 저장본을 검사하고 정의 텍스트를 내보낸다. 물리 장면이 없는 Python commandlet 모드로 최종 광선 검사를 실행하지 않는다.

조립 재실행은 마커를 새로 발급하므로 이후 만들어진 SurfaceData를 다시 베이크해야 한다. 지각의 native 경계 보정 후에도 재베이크가 필요하다. 이전 자동 생성 원본은 `Saved/SliceOctant/Acropolis_00/V001~V004`에 보관했다.
