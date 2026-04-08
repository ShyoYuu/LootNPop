# 월드 생성 시스템 기술 설계 (World Generation Technical Design)

## 1. 개요 (Overview)
**LootNPop**의 월드는 거대한 **구체(Sphere)** 형태의 메인 공간(내부 표면)과 그 내부를 떠다니는 작은 구형 부유 행성들로 구성됩니다. **PCG(Procedural Content Generation)**를 활용하여 수학적 공식 기반의 모듈형 타일을 배치함으로써 거대 지형을 구축하며, 실시간 노이즈 변조를 통해 자연스러운 언덕과 골짜기를 형성합니다.

## 2. 핵심 설계 원칙
- **Geometric Gravity:** 중력은 언덕/절벽 같은 세부 지형의 법선이 아닌, **구체의 기하학적 중심점(Center)**을 기준으로 계산합니다.
    - **Main Inverted Sphere:** `Direction = Normalize(PlayerPos - SphereCenter)` (바깥쪽 중력)
- **Fibonacci Tile Assembly:** 단순 위도/경도 분할 방식의 밀도 불균형을 해결하기 위해, **피보나치 구체(Fibonacci Sphere)** 알고리즘을 사용하여 전 표면에 균일한 밀도로 타일을 배치합니다.
- **Slope-Aware Alignment:** 타일은 단순히 중심을 바라보는 것이 아니라, 지형의 **경사면(Slope)**에 맞춰 기울어지도록 정렬하여 타일 간의 들뜸 현상을 최소화합니다.

## 3. 지형 생성 단계 (Generation Steps)

### 3.1 단계 1: 피보나치 구체 기반 타일 배치 (Uniform Assembly)
- **알고리즘:** Golden Spiral 방식을 활용한 피보나치 분포 적용.
    - **포인트 개수($N$):** 구체 표면적($4\pi R^2$)을 타일 면적($S^2$)으로 나눈 값으로 산출.
    - **분포 공식:** $z$축을 $[-1, 1]$ 범위로 균등 분할하고, 각 층마다 황금각(Golden Angle, $\approx 2.399$ rad)을 적용하여 나선형으로 배치.
- **장점:** 위도에 상관없이 타일 간격이 일정하며, UV Sphere 방식 대비 약 30~40% 적은 포인트로 동일한 피복력을 확보.

### 3.2 단계 2: 3D 노이즈 기반 지형 변조 (Surface Displacement)
- **구현:** 3D Perlin Noise를 사용하여 각 타일의 반지름($R$) 값을 변조.
    - `Height = PerlinNoise3D((Direction + SeedOffset) * R * Frequency) * Amplitude`
- **경사면 정렬(Slope Alignment):** 
    - 현재 위치 주변의 높이값을 추가 샘플링하여 외적(Cross Product)을 통해 표면 법선(Slope Normal) 산출.
    - 타일의 $Z$축을 해당 법선에 일치시켜 부드러운 곡면 지형 형성.

### 3.3 단계 3: 결정론적 시드 및 동기화
- **SeedOffset:** `FVector` 형태의 시드 오프셋을 노이즈 입력값에 더해 지형의 형태를 결정.
- **동기화:** 서버의 `GameState`에서 생성된 시드값을 공유하여 모든 클라이언트가 동일한 지형을 생성하도록 보장.

## 4. 중력 시스템 연동 (Gravity & Movement)
- **Geometric Logic:** 캐릭터 좌표와 구체 중심 좌표의 차이를 이용하여 즉각적인 중력 벡터 산출.
- **Visual Alignment:** 캐릭터의 $Up$ 벡터를 중력 반대 방향과 일치시키되, 발밑의 타일 각도에 따라 애니메이션 보정 적용.

## 5. 기술적 고려사항 및 최적화
- **HISM & Nanite:** 모든 타일은 HISM 인스턴스로 생성하며, 원본 메쉬에 **나나이트(Nanite)**를 적용하여 고밀도 타일 환경에서도 렌더링 부하를 최소화합니다.
- **PCG Partitioning:** 월드 파티션 그리드 단위로 PCG 데이터를 분할 생성하여 플레이어 주변의 지형만 실시간으로 로드/언로드합니다.
- **OverlapFactor:** 구체 곡률로 인해 발생하는 타일 간 미세한 틈새를 메우기 위해 타일 스케일에 보정값(예: 1.05x)을 적용합니다.
- **Cull Distance:** 특정 거리 이상의 인스턴스는 렌더링에서 제외하여 에디터 및 게임 내 CPU 부하를 제어합니다.
- **Navigation (Future Work):** 표준 NavMesh 대신, PCG 포인트 데이터를 노드로 활용한 그래프 기반 이동 시스템(Point-to-Point Pathfinding) 도입 검토.
