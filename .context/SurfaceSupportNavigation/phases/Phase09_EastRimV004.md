# Hollow 동쪽 Dock/Rim 외피 V004

2026-10-10. 서쪽V015의 사용자 보고 틈 해소 확인 후 남은 재제작을 이어간다. 동쪽은 [V001 크래시 복구 기록](Phase09_EastRimV001.md)을 거쳐V004를 반입·생성·베이크했다. 정식 검사에서 동쪽 능선 왕복 연결이 실패해 [V005](Phase09_EastRimV005.md)로 경사 윗끝을 수정했다. 아래 반입 수치는V004 이력이다.

## 변경 범위

- local X−14000~−7000/Y7800~12000의Ridge04·OuterEastRockJoin·Rim과OuterEastDock/Bridge/Landing·RimLanding/Approach/CliffExit의 구역 안 부분을 교체한다. 구역 밖 Generated 부분은 보존한다.
- 접안 굽이 중심(−12800,10000)에 반폭1000cm의 지면까지 찬 팔각 채움을 합쳤다. 원래 길 중심선·Rim H3000/2400·상승 연결과서쪽V015는 유지한다.
- 기존 C++ RimLanding/Approach/CliffExit는 밑면 한 장과 폭 분할 끝막음이 구면에서 맞지 않았다. 이 세 길만 밑면을 끝막음과 같은 폭으로 나눴다. 수정 수식의 절단면은 보조 연결 없이 닫힌 윤곽을 이룬다.
- Blender local Exact Boolean→구면 투영→방사 절단→기존 C++ 단면 보정 순서다. 곡률을 더 다듬되 1m 접합 띠와 가파른 벽의 기존 현은 유지했다. 모든 외벽을 0.5cm 이내 구면 근사로 바꿨다고 주장하지 않는다.

## 형상·단면·반입 검사

| 항목 | 결과 | 한계 |
|:---|:---|:---|
| 닫힌 통합 몸체 | 12540 verts/25080 tris/덩어리1·토폴로지 오류0 | 방사 절단 전 local 몸체 |
| 최종 출력 | 12558 verts/24951 tris, Floor4852/Shell20099 | V001보다887 tris 증가 |
| 열린 변 | 처방 경계169개, 예상 밖/내부 비정상 변0 | 실제 전체 조립과 별도 |
| 최종 형상 | 방향·중복·퇴화0, 비인접 BVH 교차0 | UE 캡슐 검사 아님 |
| 경계 보정 | 최대 정점 이동5.697cm, C++ 꺾임23곳 삽입 | 서쪽V015와 독립 |
| X−7000 단면 | C++65선분↔후보142선분, 각21점 양방향 최대0.353784cm | 독립 double 수식 재구성, UE 정점 추출 아님 |
| Y12000 단면 | C++23선분↔후보27선분, 최대0.002037cm | 매립 원통의 내부 판은 외곽선 비교 대상 아님 |
| 보행면 구면 근사 | 접합 띠 밖 최대0.468cm, 띠 포함 최대1.063cm | 기존 C++ 현을 유지한 띠를 구분 |
| Shell 구면 근사 | 완만한 면/접합 띠 밖 최대0.494cm | 가파른 외벽은 판으로 유지, 전체 최대28.603cm |
| UE staging | 두 역할 triangle 일치, bounds 최대차0.003907cm, LOD1개/충돌LOD0/별도complex없음 | 최종 LVI 조립 전 |
| 충돌 | complex-as-simple·양면false 설정/재조회/저장 | 생산 후보 복사 후에도 재조회 |
| Live Coding | Result: Success, patch_2 | 생성·베이크는 별도 |

V002는 기존의 넓은 C++ 밑면 때문에 지각 아래 단면이 맞지 않아 채택하지 않았다. V003은 밑면 분할 수정과 경계 보정을 확인한 중간본이다. V004의 전체 외벽을 이상 구면으로 세분화한 중간 시도는 면 교차 검사에서 탈락했다. 최종은 완만한 면만 보정하며 교차0을 다시 확인한 출력이다.

산출물은 `Saved/SliceOctant/Hollow_00/EastRimV004`의`EastRim_Sources.blend`/`EastRim.blend`/Floor·Shell FBX/manifest/seam-verification/chord-verification/unreal-verification/Preview.png다. 생성기는 `Scripts/Blender/BuildHollowEastRim.py`, 독립 단면 검사기는 `VerifyHollowEastRimSeam.py`다. 단면 목표 파일은V001 폴더에 있으며 수정한 C++ 밑면 수식으로 재생성했다. 새 후보 생성에는 이 목표 파일을 명시한다.

staging은 `/Game/Maps/OctantWorkflow/HollowEastRimV004`, 생산 후보는 `/Game/Maps/Hollow_00/Authored/SM_Hollow_00_EastRimV004_Floor|Shell`이다. Authored 바로 아래는서쪽V015 두 역할과동쪽V004 두 역할이다. 기존 서쪽Archive들은 보존했다. 수정 전 복구본은 `EastRimV001/Before`다.

## 다음 검증

사용자 생성·베이크 완료와 저장 로그를 확인했다. `Bake.HollowContent`의 동쪽Ridge04→안부 왕복2건이 실패했고 `Runtime.ProductionSurfaceDataDefinitions`는 별도 통과했다. 실제 Nav 좌표에서 급한 경사→평지 꺾임의 하단 구 침투를 재현했다. 수치·수정·다음 검증은V005 기록이 소유한다. 자동 캡슐과 Hollow 전체 완료는 미확인이다.

동쪽 통과 후 외곽 구덩이 복귀와 Shuttle 패널 점프 우회를 다룬다. 벽보행 LoS 수정은 별도 세션 범위다.
