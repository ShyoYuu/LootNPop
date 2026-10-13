# Acropolis_00 V011 — 최종 접합 틈 수정

사용자가 이 틈 수정의 성공을 확인했다. 이후 내부 지면에서 경계로 걸어 복귀할 수 없는 문제를 보고했으며, 현재 변경은 **[V015 경계 복귀로](Phase09_AcropolisV015.md)**가 소유한다. 아래는 V011 당시 기록이다.

2026-10-13. 사용자가 V009 전체 구성을 PIE로 검토하고 승인했으며, Pocket_05 지붕 끝의 검은 틈 하나를 마지막 수정으로 요청했다. 현재 그레이박스 기준본은 **V011**이다. 구성·섬·동굴·훅 제작을 마쳤으며 이번 마감은 엔진 외형·광선과 베이크 검사로 확인했다. V011 마감 구간의 사용자 PIE 재확인을 이미 받았다고 기록하지 않는다.

## 신고 위치와 원인

`WhereAmI`: slot 6, `LVI_Octant_Acropolis_00`, local X=6567 Y=-12668 H=-3402, floor=`SM_Acropolis_00_Pocket_05_Floor`. H는 캐릭터 중심이다.

Pocket_05 뒤쪽 모서리의 실제 지각 절단 교선은 H 약 -3353~-3384였다. 기존 동굴 지붕/외벽 상단 H=-3500과 약 1.2~1.5m 차이가 났다. 경계 보존 띠에서 내부 지면으로 높이가 전이하는 구간인데, 절단 뒤 그 측면을 마감하지 않아 외부가 검게 보였다. 구면 이음매 자체의 반지름 불일치는 아니었다.

## 수정 범위

- `BuildAcropolisOctant.py`가 Pocket_05 양측·뒤쪽의 실제 지각 절단 교선 385개를 공유하는 마감면을 생성한다. 열린 진입구는 제외한다.
- 마감은 같은 교선에서 H=-3505까지 수직으로 내린 뒤 기존 100cm 외벽 안쪽 50cm, H=-3580으로 매입한다. 모서리의 지붕 높이 위에서 안쪽으로 바로 기울이지 않아 사선으로 빠져나가는 틈을 막는다.
- UE에서는 지각 `SM_Octant_Acropolis_00_R30000` 하나만 재반입했다. 나머지 Authored 62개는 저장 파일 SHA256이 수정 전과 모두 같다. 마커 61개의 ID·위치/회전과 훅 28개의 상태를 보존했다.
- 현재 원본은 `Art/Octants/Acropolis_00/Acropolis_00_V011.blend`. V009와 중간 V010 후보, 복구본은 `Saved/SliceOctant/Acropolis_00/`에 보존한다.
- 기존 LVI·SurfaceData·실제 OctantPoolData 경로는 유지한다. 공통 C++·설정·다른 옥탄트를 변경하지 않았다.

V010의 첫 마감은 바로 안쪽으로 기울어 모서리 사선 두 곳이 남았다. V011은 수직 마감 후 지붕 아래에서 매입해 이를 닫았다. `Scripts/Blender/VerifyAcropolisGap.py`가 같은 시점의 전후 이미지와 검토 광선을 기록한다.

## 검증과 인계

- 이전 원본에서 빈 공간을 통과하던 광선 201개가 수정 원본과 최종 UE 메시에서 모두 막힌다. 엔진의 단면 충돌에 맞춰 **지붕 안쪽에서 지각 바깥쪽으로** 검사한다. 지각 아래에서 지붕을 향한 역방향 backface 광선과 구분한다.
- 같은 시점의 Unreal Unlit 뷰에서 검은 외부 노출이 사라진 것을 확인했다. `NativeGapAfter.png`와 Blender `GapBefore.png`·`GapAfter.png`를 보존했다.
- native seam 정점 13989개 복원, 베이크 seam 표본 2208개 Meadow와 일치.
- Pod 바닥 광선 297개, 동굴 바닥·진입 광선 330개, 천장 5개, 훅 시야 91개와 저장 마커 자세 검사가 통과했다.
- 자동 검사 `ProductionSurfaceDataDefinitions`, `ProductionSpawnComposition`, `ProductionPoolCurrentAndDeterministic` 3개가 오류 0·경고 0으로 통과했다.
- 지각은 224807 tris, 전체 원본 579279 tris다. V009 대비 지각의 마감면 1532 tris만 늘었다. Support/Nav/Traversal/Spawn/Patch의 표본 수·payload 크기는 V009와 같다.
- 실제 충돌·베이크·외형 확인 후 `TestMap03`을 열고 PIE가 꺼진 상태로 인계한다. 이번에는 전체 플레이를 다시 자동 실행하지 않았다.

검사 보고서와 이미지는 원본 `.blend` 옆에 복사한다. 세부 로그는 `Saved/SliceOctant/Acropolis_00/V011/`에 있다. 이 기록의 완료 범위는 채택한 그레이박스 형상·동선·장치 배치이며 정식 아트·NPC 밸런스·패키지/부하 Gate를 완료했다는 뜻은 아니다.
