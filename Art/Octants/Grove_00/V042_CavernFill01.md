# Grove V042 공동 보조 조명

2026-11-10. 사용자는 V042 앵커 전체를 둘러보고 적절하다고 승인했다. 지하 진입 통로는 문제없으나 큰 공동이 조금 어두워 조명을 소량 더 요청했다.

공동 네 사분면에 포인트 조명4개(RoomFill0~3)를 추가했다. 기존41개 조명 값·위치와 통로 배치는 그대로이며 전체45개, 공동 조명은9→13개다. 새 조명은 공동 바닥 위28m, 반경42m,45,000lm·기존과 같은 색·Movable·그림자 꺼짐이다. 상부와 넓은 공간을 보충하며 전역 노출/그래픽스 설정은 변경하지 않았다.

V042 LVI를 직접 저장했고 지형/재질·조명41개·승인 앵커30개·Pod8개를 보존했다. 신규 조명 위치에서 세 축±150cm 광선이 비어 있음을 확인하고 기존 액터의 변환/기존 조명 속성을 비교했다. 에디터 Lit 화면에서 천장/기둥/엄폐/연결로 가시성을 관찰했다. 실제 게임의 노출·카메라 밝기는 사용자 확인 범위다.

조명만 변경해 재베이크나 전체 자동 검사를 반복하지 않았다. 저장본으로 TestMap03 PIE8slot·16bindings·blocked seam nodes/edges0·seam delta0·Pod120/120·적0의 정상 초기화를 확인했다. 기존 SurfaceData가 유효함을 확인했으며 PIE 시작 로그를 보존한다. 서버 콘솔 `LNP.Debug.TeleportToReview GroveUndergroundHub 0`에서 공동 밝기만 검토한다.

적용 원본은 GroveRootNetworkSpec.py의 cave_lights(), 라이브 적용은 FillGroveCavernLights.py다. 고정 지형은 V040, 앵커는 승인된 V042를 유지한다. 보고서/추가 조명/시작 로그/코드 사본은 Saved/SliceOctant/Grove_00/V042/CavernFill01에 있고, 변경 전 맵은 LVI_BeforeCavernFill.umap으로 보존했다. 기존 V042/SourceAtReview는 앵커 승인 당시 사본이며 새 조명 코드는 CavernFill01/SourceAtReview가 소유한다.

기존327개 파일·실제 풀/DefaultGame.ini·다른 Graphics 작업을 보존했다. C++/Live Coding/전체 빌드 미사용이다. 정식 풀 통합·전체 SurfaceNavigation·혼합 PIE·NPC 실제 교전 검증은 후속 단계로 남아 있다.
