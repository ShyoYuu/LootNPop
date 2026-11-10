# Grove 정식 풀 통합·최종 검증

**후속 풀 변경:** 사용자 지시로 Meadow를 제외해 현재는5종이다. 아래6종 통합 당시 검증 기록은 보존하며 최신 상태는 [Meadow 제외](MeadowRemoval.md)를 따른다.

2026-11-10. V040 지형과 V042 앵커/최종 조명 사용자 승인 뒤 정식 풀 통합을 요청받았다. 확정 지형·앵커30개·조명47개·Pod8개와 검토점83개를 보존했다. C++/Live Coding/전체 빌드와 전역 그래픽스 설정 변경은 하지 않았다.

## 정식 경로와 보존

- LVI: `/Game/Maps/Grove_00/LVI_Octant_Grove_00`
- SurfaceData: `/Game/Maps/Grove_00/DA_OctantSurface_Grove_00`
- 지각: `/Game/Maps/Grove_00/SM_Octant_Grove_00_V040_R30000`
- 외피: `/Game/Maps/Grove_00/Authored/SM_Grove_00_V040_Forest_Floor` 및 `SM_Grove_00_V040_Forest_Shell`
- 재질: `/Game/Maps/Grove_00/M_Grove_Ground`, `M_Grove_Bark`

기존 WIP 사본을 이동하지 않고 정식 사본을 만들었다. 171개 액터의 변환·태그·역할별 속성은 경로 치환 외에 동일하다. 복제 월드 참조를 해제한 뒤 맵을 열며, 복구 가능한 실패 사본과 실행 로그도 보관했다. 의존성 그래프10개 패키지에 WIP 참조가 없다. 기존327개 파일 중 실제 풀 하나만 바뀌었고326개·DefaultGame.ini는 불변이다. 풀의 기존5개 정의와 legacy 목록을 보존하고 Grove 정의1개를 추가했다. 회전255·signature `DysonSphere_R300m_V1`, 전체6종이다. 실제 풀 저장은 라이브 에디터에서 수행했다.

## 베이크와 자동 확인

| Stream | 요소 | 바이트 |
|:---|---:|---:|
| Support | 1,921,443 | 13,620,423 |
| Navigation | 128,907 | 1,047,200 |
| Traversal | 1,530 | 23,531 |
| Spawn | 11,879 | 475,396 |
| ConditionalPatch | 0 | 4 |

기준 Meadow seam2,208개가 일치한다. 정식 사본에서 기존 검사 경로로 앵커 착지368·여유322·Pod 머리 공간72·앵커 접근46 광선을 확인했고 최대 바닥 오차0.001913cm, 마커30/스폰8 ID가 고유하다. 고정 지형 전체 검사는 기존 V040/V042 근거와 사본 보존 비교를 사용하며, 이번 native 범위는 앵커 중심이다. 원본 SHA256를 확인한 별도 Blender FBX 재출력에서 역할별 triangles/bounds가 일치하고 총433,250tris다.

실제6종 풀로 `LootNPop.SurfaceNavigation` 전체135개 실행 결과 최초133개 통과·2개 실패, Grove 저장해시 재베이크 후 해당 현재성 검사를 재통과해134개 검증·PodOverlay1개 잔여 실패다. `Nav.ProductionSpawnProjection`은 앵커100개·후보99,114개가 모두 투영되어 통과했다. 원본 패키지 재저장 뒤 오래된 Grove 저장 원본 해시를 재베이크로 갱신했고, 다섯 런타임 스트림의 전후 SHA256가 같다. Grove 단독 정식 DA로 현재성·두 번 재베이크 재검사를 통과했다. 기존5종의 현재성·재현성은 최초 전체 검사에서 이미 통과했다. 최초 실패와 재검사 보고서를 모두 보존한다. 경고4건은 외부 HTTP timeout3건과 기존 B-option authoring world 현재성 안내1건이다. 시험편3개는 Git 원본으로 복원했다.

## 혼합 PIE와 사용자 확인

TestMap03·정식6종 풀·적 밀도1로 실행했다. slot0 Grove,1 Bastion,2 Hollow,3 Pyramid,4 Meadow,5 Acropolis,6 Bastion,7 Hollow다. 8slot·283bindings·Nav755layers·seamLinks4,379·seam 높이 차0cm·Pod120/120·적2,087/2,087을 확인했다. 차단 seam node49/edge2는 초기화 관측값이며 높이 불일치나 전체 경계 통행 실패를 뜻한다고 단정하지 않는다.

사용자에게 GroveEntryWest의 이웃 경계 왕복, GroveClearingA·GroveUndergroundHub의 Pod 적 실제 접근·공격을 요청했고 사용자는 문제없었다고 답했다. 요청한 통행·교전 검토는 통과로 기록한다. 적 종류별 개별 관찰 횟수나 공격 횟수는 별도 보고되지 않았으며 자동 배치 수치를 종류별 실제 교전 증거로 확대하지 않는다. 이전 지형/훅/조명 전체 승인도 유지한다.

저장 해시 갱신 뒤 정식 혼합 풀 PIE를 다시 시작했다. 이번 Grove는 slot3이며8slot·214bindings·seam 높이 차0cm·Pod120/120·적2,286/2,286이다. 다섯 런타임 스트림이 동일하므로 이전 사용자 통행/교전 확인은 유지한다. 재시작 근거는 `pie-after-rebake.json`이다.

근거 소유 경로는 `Saved/SliceOctant/Grove_00/Production`의 `integration.json`, `actor-preservation.json`, `bake-summary.json`, `native-verification.json`, `asset-audit.json`, `pie-startup.json`, `SourceExport.log`, `SurfaceNavigationTests/`다. 최종 파일 해시와 실행 원본 사본도 이 위치에 보존한다. 패키지/네트워크 부하·정식 아트·보스·보상 밸런스 완료로 확대하지 않는다.


### 남은 자동 검사1건

`LootNPop.SurfaceNavigation.Nav.PodOverlay`는 고정 seed135792468로 선택된 slot0 Pyramid의 첫 Nav 층에서 “Pod가 직선을 막고 양 끝점은 열린” 샘플을 찾지 못해 실패했다. Grove가 없는 기존 Pyramid 단독 풀에서도 같은 assertion 실패가 재현됐다. 기존 Pyramid 에셋 해시는 불변이다. Grove 지형을 수정하거나 풀 순서를 바꿔 우회하지 않았다. 샘플 선택이 slot0/첫 층에 고정된 검사 입력을 별도 검토해야 하며, 이를 실제 Pod 우회 기능의 성공/실패로 단정하지 않는다. C++ 테스트/런타임 코드·Live Coding·전체 빌드는 이번 공유 에디터 작업에서 변경하지 않았다. 전체 자동 검사를 모두 통과한 상태로 표기하지 않는다.

후속 보정(2026-11-10): Pyramid 지각에는 바로 옆 이웃 간격이 Pod 차단 반경178cm를 넘는 node가 기하학적으로 없어서(외곽 밖 최대176.5cm) 검사가 끝점을 처음 열린 node까지 연장하도록 고쳤다. 정식5종 풀·Pyramid 단독 풀에서 모두 통과했다. 근거는 [현재 상태](../../../.context/SurfaceSupportNavigation/Current.md)에 있다.
