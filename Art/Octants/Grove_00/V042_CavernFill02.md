# Grove V042 공동 국소 조명 보충

**사용자 최종 승인(2026-11-10).** 현재47개 조명에서 보충을 마친다. 사용자는 전체적으로 조금 어둡고 이동 중 시야가 더 어두워지는 순간이 있음을 확인했으나 이를 지하 공간의 분위기로 수용했다. 눈 적응/자동 노출이 원인일 가능성을 언급했으며 실제 원인 진단은 하지 않았다. 조명 추가와 노출 설정 변경은 하지 않는다. 지형V040·승인 앵커30개·현재 조명47개가 확정 기준이다.

2026-11-10. CavernFill01 PIE에서 사용자가 두 어두운 지점을 WhereAmI로 보고했다. slot7 local(-4985,1038,-9901), (-10771,5020,-9902)다. 기존 조명까지 가장 가까운 거리는 각각16.37m와25.48m였다.

각 지점 근처에 포인트 조명1개씩 추가했다. RoomEdgeEast는 local(-4300,1050,-9000), RoomEdgeNorth는 local(-10600,5400,-9000)이다. 둘 다 바닥 위10m·반경24m·40,000lm·Movable·그림자 꺼짐·기존 색을 사용한다. 새 조명과 가장 가까운 기존 조명 사이는16.23m/21.73m로, 주변 조명을 중복시키지 않으면서 낮은 영역을 보충한다. 보고 지점에서 새 조명까지는12.59m/10.36m다. 거리는 구면 투영 후 월드 좌표로 측정했다.

전체 조명45→47개, 공동13→15개다. 기존45개 조명 속성·모든 기존 액터 변환·승인 앵커30개·Pod8개를 보존하고 신규 위치의 세 축±150cm가 비어 있음을 확인했다. V042 LVI만 저장했다. 지형3개와 기존327개 파일·실제 풀·DefaultGame.ini 해시를 비교했다. 그래픽스 전역 설정/C++/Live Coding/전체 빌드는 사용하지 않았다.

조명만 변경해 재베이크/전체 검사를 반복하지 않았다. 기존 SurfaceData로 TestMap03 PIE8slot·16bindings·blocked seam nodes/edges0·seam delta0·Pod120/120·적0 초기화를 확인했다. 이후 사용자가 실제 카메라 밝기를 검토하고 위의 잔여 어두움을 수용했다.

추가 위치와 간격은 Saved/SliceOctant/Grove_00/V042/CavernFill02의 lights.json·spacing.json, 적용/보존/시작 근거는 application.json·preservation.json·pie-startup.json이 소유한다. 변경 전 맵은 LVI_BeforeCavernFill02.umap, 이번 코드 사본은 SourceAtReview에 보존한다. 기존 CavernFill01 및 V042/SourceAtReview는 변경하지 않는다. 지형은 V040, 앵커는 승인된 V042 그대로다. 정식 풀 통합·전체 SurfaceNavigation·혼합 PIE·NPC 실제 교전 검증은 후속 단계다.
