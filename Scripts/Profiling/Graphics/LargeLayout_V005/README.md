# Ridge U4-5 V005 북쪽 단 뒤~안부 초벌

V004 사용자 채택 뒤 만든 V005다. 사용자가 2026-11-25 채택했다. 이후 동결한다.

- V004 맵과 저작 파일 해시·채택7소유자의 자세/참조/결과 지문을 고정한다. 기존 Z2/Z4는 이 폴더의 독립 사본이다.
- `ASM_Z3_NorthRearShoulder`: 01c 전용 사본·로컬 `(-5600,6200,H200)`·yaw−30°·크기1, 830인스턴스. 원본 상자 상단 H약3782다.
- `ASM_Z3_SaddleSouthEast`: 01b 사본·`(-3100,8000,H0)`·yaw50°·크기1, 505인스턴스. 원본 상자 상단 H약4024다.
- 북쪽 단 H약1950→안부 H약3740 관계와 기존 산괴 윤곽을 유지한다. 같은 조립체를 긴 벽으로 반복하지 않는다.
- 실제 NorthSaddle Pod `(-5628,9248.1,H3739.3)`·9m와 북서 접근/동쪽 출구·부유섬/동굴/잔도 등14개 보호 공간을 대조한다. 보호 폭/메시 상자50cm 여유는 유지한다.
- Crown/WestStair 동선·제외2Pod는 보류를 유지한다. 서쪽 접근은 U4-6으로 남긴다.
- 원본/공유 에셋·V001~V004·기존 지각/충돌/SurfaceData·마커·풀을 보존한다. 몸체·연속 외피·세부 식생/잔돌 마감은 후속이다.
- 새 맵 지정 저장·재로드 후 UE Cmd: `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeLargeLayout.py" export V005`.
- 근거: `Saved/Profiling/Graphics/Phase01/U4_5_V005/`. `Views_Before/`는 V004의 신규4시점이며 기존8시점도 비교한다.

실제 `Export_20261124_233749_888432.json`: PASS·issues없음·exported=true·dirty없음. 기존 Z1·채택7소유자·51파일 불변, 24개 LI/로드, 신규830+505인스턴스 NoCollision·14개 보호 공간 표본 겹침0이다. 경고1건은 기존 북벽 AttachTo다. 사용자가 구도를 채택하고 다음 U4-6 진행을 승인했다.
