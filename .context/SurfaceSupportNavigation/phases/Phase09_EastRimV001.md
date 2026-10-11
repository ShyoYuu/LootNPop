# Hollow 동쪽 Dock/Rim 외피 후보 V001

2026-10-10. 사용자가 BackPocketV015의 보고 틈 해소를 확인하고 남은 재제작을 요청했다. 이 문서는 동쪽V001의 크래시와 복구 경위다. 최신 후보는 [V004](Phase09_EastRimV004.md)이며 기존 C++ 단면 보정·반입·Live Coding까지 완료했다. V001 자체를 UE에 반입하지 않았다.

## 구역과 보존 기준

- 교체 후보: local X−14000~−7000/Y7800~12000. OuterEastDock 굽이·Ridge04·OuterEastRockJoin·Rim과 구역을 가로지르는 연결 길이다.
- 접안 굽이에 중심(−12800,10000)/반폭1000cm의 지면까지 찬 팔각 암반을 합쳐 급한 좁은 굽이를 넓힌다. Rim 상단H3000/하단H2400과 기존 상승 연결을 보존한다.
- `Scripts/Blender/BuildHollowEastRim.py`가 후보 원본을 소유한다. 코드와 전체 Hollow 에셋의 수정 전 복구본은 `Saved/SliceOctant/Hollow_00/EastRimV001/Before`다.
- C++ 교체 함수/사전 검사 초안은 아직 Live Coding하지 않았다. 동쪽 에셋 확보와 경계 검사 전에는 새 생성 명령을 요청하지 않는다.

## Blender 크래시와 복구

첫 실행은 원본들을 구면 투영한 뒤 Exact Boolean modifier 스택을 평가하는 중 크래시했다. 사용자 제공 `blender.crash.txt`에서 Blender5.2.0의 `meshintersect::trimesh_nary_intersect`→`boolean_trimesh`→`boolean_mesh`와 write-null `EXCEPTION_ACCESS_VIOLATION`을 확인했다. Python backtrace는 비어 있다. 정확한 operand와 입력 조건은 아직 특정하지 않았다.

크래시 로그는 후보 폴더의 `blender-first-union.crash.txt`에 복사했다. 첫 실행에서는 `.blend`/FBX/manifest가 생성되지 않았다. 별도 백그라운드 프로세스에서 원본만 재생성해 확인한 결과, 팔각 암반의 반복 세분화가 중복 면을 대량 생성했다(Ridge04 중복20474개, DockBendApron 중복237514개). 해당 prism 경로에 Boolean 전 토폴로지 검사가 빠져 있었다. 이 입력이 크래시를 유발했을 가능성이 크지만 네이티브 backtrace만으로 정확한 operand를 확정하지는 않는다. 실패 원본은 `EastRim_FirstInvalidSources.blend`와 `first-source-report.json`에 보존했다.

분할 변마다 중점을 하나만 공유하고 삼각형을 직접 나누는 방식으로 바꿨다. 모든 원본에 비정상 연결·방향·중복·퇴화 검사를 추가했으며 원본10개 모두 오류0을 확인했다. 합치기 전 원본 `.blend`/source 보고서와 각 operand 시작·완료 기록도 남긴다. 수정 원본의 순차 Exact Boolean 9회는 크래시 없이 완료했다.

구면 투영 후 공면 합치기는 얇은 오류 면을 남겨 최종 검사에서 기각됐다. Manifold solver도 비교했으나 채택하지 않았다. 최종 경로는 local Exact Boolean→구면 투영이며, 같은 정점을 공유하고 법선이 반대인 내부 면 두 쌍(4면)만 쌍으로 상쇄했다. 닫힌 통합 몸체의 토폴로지와 출력의 교차/열린 경계 검사를 통과한 뒤 FBX를 내보냈다. 구면 단면을 C++와 맞췄다는 뜻은 아니며 다음 검사로 남긴다.

## 복구 결과

- 닫힌 몸체: 12540 verts/25080 tris/덩어리1, 비정상 연결·방향·중복·퇴화0.
- 출력: 12103 verts/24064 tris(Floor4542/Shell19522), 네 방사 절단면의 열린 변146개/예상 밖0, 방향·중복·퇴화0/비인접BVH교차0.
- 산출물: `EastRim_Sources.blend`, `EastRim.blend`, Floor/Shell FBX, manifest/build-result. 원본과 각 단계의 기록은 후보 폴더에 있다.
- 백그라운드 최종 생성 약9초, 크래시 없음. 썸네일 폴더 쓰기 경고는 있었으나 `.blend`/FBX 저장 성공을 확인했다.
- Blender 재시작·MCP 재연결을 사용자에게 요청했다. 라이브 장면 외형과 기존 C++ 경계의 단면·투영 오차·최종 조립은 미검증이다.

## 다음 검증

1. 원본 생성 시간/크기·토폴로지 확인 후 operand별 Boolean 재시험.
2. 닫힌 통합 몸체·예상 방사 경계만 열린 출력·중복/퇴화/비인접 교차 검사, 기존 C++ 실제 단면과 접합 비교.
3. staging 반입의 triangle/bounds·complex-as-simple·LOD0·양면false 확인과 production 후보 배치, Live Coding.
4. 제작 가이드 §6에 따라 사용자 생성/베이크 요청 후 정식 검사와 Dock 굽이/Rim 하부 탈출의 PIE 검토.

이후 외곽 구덩이의 자력 복귀와 Shuttle 패널의 점프 우회를 다룬다. 이 문서는 동쪽 후보의 완료를 뜻하지 않는다.
