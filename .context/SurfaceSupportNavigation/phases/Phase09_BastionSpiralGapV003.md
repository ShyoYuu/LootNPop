# Bastion 나선–나팔 틈 V003 — 재베이크·자동7검사·사용자 PIE 통과

2026-10-12 사용자는 V002 패널·훅을 채택했고, 나선 경사로와 나팔 벽 사이에 캡슐이 끼이는1곳을 보고했다. 캡슐이 아래로 떨어질 만큼 틈을 조금 넓힌다. 승인한 나머지 지형·장치·광원·Pod는 유지한다.

## 위치와 원인

`WhereAmI` 기록: slot7/Bastion_00/local X4406 Y2527 H17323, r5079/29.8°, 발밑 `SM_Bastion_00_P01_Dish_Shell`. 위치는 캡슐 중심이며 `floor`는 아래5m 광선의 첫 메시다. 보행 가능한 바닥 위에 서 있음을 뜻하지 않는다. 사용자 PIE 종료를 확인했다.

원본 `BuildBastionSpire`는 나선 바깥 반지름2100cm와 나팔 안쪽 경계2200cm 사이를100cm로 뒀다. 캡슐 지름84cm만 비교하면 충분해 보여도 현지 중력에 기운 전체192cm 캡슐, 다각형 벽 단면과 낙하 중의 구면 방향을 포함하면 통과하지 못한다. 저장 메시를 복제한 실제 Chaos에서 H15500~17500의 나선41지점, 캡슐42/96, 현지 중력 낙하를 검사해 **41곳 모두 벽에 막힘**을 재현했다. 첫 실패는 H15500의 `P01_Dish_Shell` 초기 겹침이다. 기존 중앙/가장자리 보행 검사로 이 낙하 틈까지 통과한 것으로 판단한 것이 누락이었다.

## 수정과 후보 검증

나선 폭900cm·위치와 나팔 외곽4000cm·정상 높이는 유지한다. 나팔 안쪽 경계만2200→2300cm로1m 물려 명목 틈100→200cm로 넓힌다. 해당 경계의 상부 개구부도 동일 반지름을 사용한다. 기존 나팔 몸체 자체를 고치며 덧댐/차단 메시를 추가하지 않는다. 변경 원본은 `LNPBastionLowPoly.inl`의 `BuildBastionSpire`다.

`BastionLowPolyContent`에 나선 위41곳의 전체 캡슐 낙하 회귀 검사를 추가했다. 경사판 위150cm에서 판 아래로 캡슐 전체가 빠지는 위치까지 현지 중력 방향으로 sweep한다. **지하까지 자유 낙하·실제 Mover 조작을 검증한 결과로 확대하지 않는다.**

`BastionSpireProposal`은 저장 조립 중 나팔2역할만 새 원본의 임시 메시로 바꿔 기존 Chaos 검사와 낙하 검사를 재사용한다. production 에셋은 쓰지 않는다. Live Coding patch_11(재현 검사), patch_12(후보) 성공. 원본+후보 **2검사 성공·오류0·경고0·1.291초**, 나선 낙하41/41통과·기존 보행14658바닥/28716캡슐·훅46착지/1747관성 낙하·패널774sweep 통과다. 후보 훅 접근1089/착지 여유9912점도 통과했다.

## 보존·다음 단계

복구본: `Saved/SliceOctant/Bastion_00/SpiralGapV003/Before/`의 수정 전 LVI/SurfaceData/나팔2메시/원본·검사 코드. 기준은 `before-asset-hashes.json`, 실패 재현 `before-tests.json`, 후보 `proposal-tests.json`, 로그 `diagnosis-proposal-log.txt`다. 코드의 변경 부품은 `P01_Dish`뿐이며 재생성은 기존 `BuildSliceOctant Bastion_00` 명령을 사용한다.

사용자 재생성 후 **저장본·원본·지각3검사 성공·오류0·경고0·2.221초**. 저장본의 낙하41/41·보행14658바닥/28716캡슐·훅46착지/1747관성 낙하·패널774sweep을 통과했다. 근거는 `generated-tests.json`이다. 기존 전체 재생성 명령은 같은 형상의 메시도 재저장하므로 파일 SHA가 바뀌며, V002의199파일 해시 보존 결과를 V003에 재사용하지 않는다. 변경 전후 파일 목록은 `post-generation-assets.json`에 기록했다. 원본의 의도된 형상 변경은 나팔2역할뿐이다.

**재베이크·정식7개 자동 검사 성공·오류0·경고0·6.928초.** `BastionCrustIntegrity`, `BastionLowPolySource`, `BastionLowPolyContent`, `BastionLowPolyNav`, `BastionEncounters`, `HollowContent`, `ProductionSurfaceDataDefinitions`를 통과했다. 나선 낙하41/41, 기존 보행·장치와 Nav 왕복1408경로·Pod96투영을 통과했다. 저장 지각227636tris·구조물56100tris, 훅46·패널1·Pod12·광원81, 생성lint0/authoredErrors0이다.

DataVersion7/manifest203개, Support1813344샘플/12858982bytes, Nav151117cells·345components·2553portals·1107seams/1232304bytes, Traversal3660/68227bytes, Spawn12authored+13107candidates/528020bytes다. 국소 형상 변경으로 Nav셀44개/랜덤Spawn후보3개가 줄었으며 기존 필수 연결·Pod 회귀는 통과했다. 근거는 `baked-tests.json`, `baked-header.json`, `generated-baked-log.txt`다.

**2026-10-12 사용자 PIE 재확인 통과.** 사용자는 나선 경사로 양측을 모두 시험했고, 양쪽 모두 끼임 없이 잘 떨어지는 것을 확인했다. 보고된 나선–나팔 틈의 캡슐 끼임 수정은 완료다. 외측 틈의 자동41곳 검사와 사용자 양측 실제 낙하 확인을 구분해 근거로 남긴다. 사용자 답변을 별도 나선 전 구간 보행 왕복·지하까지 자유 낙하 확인으로 확대하지 않는다. V001 메시·외피와 V002 패널·훅 채택을 유지하며 이번 제작·장치 개편과 발견된 끼임 수정 작업을 마무리한다. 광원 성능 실측·감축, 기존 전체 Pod 교전·NPC LoS·풀 조합·패키지/부하 Gate는 별도 후속 범위다.
