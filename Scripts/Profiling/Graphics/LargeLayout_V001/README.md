# Ridge U4 큰 배치 저작본 V001

`RidgeLayout.json`은 보존할 V037/V010과 새 구역 JSON을 연결한다. `EDTerrainLayout_Z2.json`은 U4-1의 손배치 결과와 몸체 단계 인계를 소유한다. 기존 `EDTerrainLayout_Z1.json`은 수정하지 않는다.

- 현지 X/Y/H 단위는 cm이고 H는 구 중심 방향으로 증가한다. 회전은 현지 Up 좌표계 기준 pitch/yaw/roll, 스케일은 3축이다.
- 기존 15개 Level Instance는 월드 좌표 Cull 사본이다. 원점·단위 스케일 그대로 유지한다. 새 Z2 조립체는 원본 좌표의 사본 레벨을 현지 자세로 놓는다.
- 새 조립체 사본은 원본 `PL_LargeCliff_02d`를 복제한 것으로 원래 내부 배치·원래 크기를 유지한다. 공유 메시/재질/BPP는 수정하지 않는다. 입력 레벨과 사본의 패키지 해시를 JSON에 기록한다.
- 현재 충돌과 마커는 V010의 보존본이다. 새 시각 배치에는 충돌·SurfaceData를 만들지 않았다. 이 맵을 검증 완료된 플레이용 풀에 넣지 않는다.

에디터 Cmd의 `RidgeLargeLayout.py export V001`은 현재 새 작업본의 실제 자세를 읽는다. Z1 지문/입력 해시·마커·새 시각 NoCollision·협곡 상자 검사에서 문제가 있으면 JSON을 갱신하지 않고 Saved 보고서에 남긴다. 상자 검사는 보수적인 배치 점검이며 실제 충돌/보행 검증을 대신하지 않는다.

하위 레벨이 로드되지 않았으면 `LoadRequested` 보고서만 쓰고 해당 Level Instance에 로드를 요청한다. 이때 export는 하지 않는다. 로드 뒤 같은 Cmd를 다시 실행한다. `SceneTools.create_level_instance`는 내부 로드를 요청하지 않으므로 이 도구로 액터를 만든 직후의 화면을 배치 확인 근거로 삼지 않는다. 저장된 새 맵을 다시 열어도 로드할 수 있다.

`RidgeLargeLayout.py build V002`는 저장된 V010을 새로운 최상위 맵으로 복제하고 현재 구역 JSON의 소유자만 추가한다. 기존 맵 이름이 있으면 덮어쓰지 않고 멈춘다. Level Instance가 로드된 다음 별도 Cmd에서 `verify V002`를 실행한다. 재현 원본이나 생성물을 준비 목적으로 재실행하는 명령이 아니다.

기존 JSON/레벨을 되돌려 고치거나 원본에 Cull을 다시 적용하지 않는다. 이후 단위도 새 버전에서 진행하며, 새 큰 배치의 구도가 확인된 뒤 몸체·충돌·세부 마감 단계로 넘긴다.
