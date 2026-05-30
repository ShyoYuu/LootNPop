# Octant Level Instance 제작 가이드

Octant Level Instance 신규 제작 절차.  
완성된 레벨 인스턴스는 `OctantPoolData`에 등록되며, 런타임에 랜덤 선택·회전 배치되어 SphereWorld를 구성.

---

## 사전 지식

SphereWorld는 구체를 8개의 Octant(1/8 조각)로 분할하고, 풀에 등록된 레벨 인스턴스 중 8개를 시드 기반으로 선택·회전 배치해 완성.  
각 Octant는 **메시 + PCG 프랍 배치 + 레벨 인스턴스** 세 레이어로 구성.

> 런타임 스폰 흐름 상세: [TechDesign_WorldGeneration.md](TechDesign_WorldGeneration.md)

---

## 절차

### Step 1 — Octant 스태틱 메시 생성

`Maps/BP_OctantGenerator` 블루프린트를 열어 아래 변수를 설정.

| 변수 | 위치 | 설명 |
|:---|:---|:---|
| **Radius** | Blueprint 변수 | Octant 반지름 (SphereWorld 전체 크기 결정) |
| **Subdivisions** | Blueprint 변수 | 폴리곤 밀도 (높을수록 곡면이 부드러워지나 버텍스 수 증가) |
| **Magnitude** | Blueprint 변수 | 노이즈 변위 강도 (지형 굴곡의 높낮이 차) |
| **Frequency** | Blueprint 변수 | 노이즈 빈도 (낮을수록 완만한 구릉, 높을수록 날카로운 지형) |
| **Random Seed** | Blueprint 변수 | 노이즈 랜덤 시드 (동일 시드 = 동일 지형 재현 가능) |
| **Asset Path And Name** | Details 패널 → Save Asset | 결과 스태틱 메시 저장 경로 (예: `Maps/Meshes/SM_Octant_00`) |

1. Blueprint 변수를 입력한 뒤 **컴파일**하면 뷰포트 프리뷰가 갱신.
2. 결과가 마음에 들면 Details 패널 **Save Asset** 섹션에서 **Asset Path And Name**을 입력하고 **Save Mesh** 버튼을 클릭.
3. 콘텐츠 브라우저에서 해당 경로에 스태틱 메시가 생성된 것을 확인하고 **저장**.

> **주의:** Save Mesh 버튼을 눌러야만 새로운 에셋을 생성하거나 기존 에셋을 덮어씀.

> **예시:** `Maps/Meshes/SM_Octant_00`

---

### Step 2 — 테마 데이터 에셋 생성

콘텐츠 브라우저에서 우클릭 → **Miscellaneous → Data Asset** → `LNPOctantThemeData`를 부모 클래스로 선택해 데이터 에셋을 생성.

데이터 에셋을 열어 배치할 스태틱 메시 목록을 입력.

| 필드 | 설명 |
|:---|:---|
| **Static Mesh** | 배치할 배경 프랍 메시 |
| **Weight** | 이 메시가 선택될 상대적 확률 (값이 클수록 자주 배치됨) |
| **Random Scale Range** | 배치 시 적용할 스케일 무작위 범위 |

> **예시:** `Meadow_00/DA_OctantTheme_Meadow` — Cone(2.0), Cube(1.0), Cylinder(0.5)

---

### Step 3 — PCG 그래프 인스턴스 생성

콘텐츠 브라우저에서 `PCGGraph/PCG_Octant_BaseProps`를 우클릭 → **Create PCG Graph Instance**로 인스턴스를 생성.

생성된 인스턴스를 열어 **Octant Theme** 파라미터에 Step 2에서 만든 데이터 에셋을 연결.

> **예시:** `Meadow_00/PCGI_Octant_Meadow` → Octant Theme: `DA_OctantTheme_Meadow`

---

### Step 4 — Octant 액터 블루프린트 생성

콘텐츠 브라우저에서 우클릭 → **Blueprint Class → Actor**로 새 블루프린트를 생성.

블루프린트 안에 다음 두 컴포넌트를 추가.

| 컴포넌트 | 설정 |
|:---|:---|
| **Static Mesh Component** | Step 1에서 생성한 `SM_Octant_**` 메시 지정 |
| **PCG Component** | Step 3에서 생성한 `PCGI_Octant_**` 그래프 지정 |

이후 Static Mesh Component의 Details 패널에서 **Navigation → Can Ever Affect Navigation: `false`** 로, **Transform → Mobility : `Static`** 으로 설정.

> **예시:** `Maps/Meadow_00/BP_Octant_Meadow_00`

---

### Step 5 — 빈 레벨에 배치 및 HISM 확인

**File → New Level → Empty Level**로 완전히 빈 레벨을 오픈.

Step 4에서 만든 블루프린트를 레벨에 배치하고 Transform을 `(0, 0, 0)`으로 설정.

뷰포트에 PCG가 자동으로 실행되어 구면 위에 프랍이 배치된 모습이 보여야 한다.  
Outliner에서 해당 액터를 펼쳐 **Static Mesh Component 하위에 HISM(Hierarchical Instanced Static Mesh) 컴포넌트들이 생성되어 있는지 확인**한다.

> HISM이 보이지 않으면 PCG 컴포넌트를 선택 후 **Generate** 버튼을 눌러 수동 실행한다.

---

### Step 6 — 레벨 인스턴스 생성

Outliner에서 블루프린트 액터를 **우클릭 → Level → Create Level Instance**를 선택.

| 설정 | 값 |
|:---|:---|
| **Pivot Type** | Actor |
| **Pivot Actor** | Step 4에서 만든 블루프린트 액터 |
| **저장 경로** | `Maps/LevelInstances/` |

> **예시:** `Maps/Meadow_00/LVI_Octant_Meadow_00`

레벨 인스턴스 파일을 **저장**.

---

### Step 7 — OctantPoolData 등록

`Maps/OctantPoolData` 데이터 에셋을 열어 Step 6에서 생성한 레벨 인스턴스를 풀 목록에 추가하고 저장하면 완료.

다음 게임 실행 시 풀에 등록된 레벨 인스턴스 중 시드 기반으로 8개가 선택·회전 배치되어 완전한 SphereWorld가 구성.

---