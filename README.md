# Week7 Team5 기술문서

## 1. 개요

본 문서는 KTL 엔진에 7주차(2025.10.16 ~ 2025.10.19)에 구현된 주요 렌더링 시스템의 기술적 사양과 아키텍처를 설명합니다.

7주차에는 **Clustered Forward Shading (Forward+)** 렌더링 파이프라인 구축, **완전한 조명 시스템** (4종 라이트 타입), **Uber Shader 아키텍처**, **Normal Map 시스템**, **Render Pass 구조 재정립** 등 대규모 렌더링 인프라 개선 작업이 진행되었습니다.

총 **97개의 커밋**으로 구성되며, 대규모 조명 처리와 유지보수성을 크게 향상시켰습니다.

---

## 2. Clustered Forward Shading (Forward+) 구현

### 2.1. 개요

Forward+ 렌더링은 화면을 타일로 나누어 각 타일에 영향을 미치는 라이트만 처리함으로써, 수백~수천 개의 동적 광원을 효율적으로 렌더링할 수 있는 기법입니다. 기존 Forward Rendering의 단순함을 유지하면서도 Deferred Rendering 수준의 조명 성능을 달성합니다.

### 2.2. 핵심 아키텍처

#### 2.2.1. Light Tiles Compute Shader (`LightTilesComputeShader.hlsl`)

화면을 16x16 픽셀 타일로 분할하고, 각 타일의 AABB를 View Space에서 계산한 뒤, Sphere-AABB 충돌 검사로 해당 타일에 영향을 주는 라이트 인덱스를 기록합니다.

```hlsl
// 타일별 라이트 인덱스 저장 (최대 1024개 라이트)
RWStructuredBuffer<uint> LightIndexList;
RWStructuredBuffer<uint2> LightGrid; // x: offset, y: count
```

**주요 기능:**

- Frustum의 near/far plane을 고려한 타일 AABB 계산
- Left-Handed 좌표계에 맞춘 Y축 반전 처리
- Sphere-AABB intersection 검사 (`BoundingSphere::Intersects(AABB)`)
- Thread Group 내 동기화 (`GroupMemoryBarrierWithGroupSync`)

#### 2.2.2. 타일 기반 라이팅 (`TexturePS.hlsl`)

픽셀 셰이더에서 현재 픽셀이 속한 타일의 라이트 리스트만 순회하여 조명을 계산합니다.

```hlsl
// 픽셀이 속한 타일 계산
uint2 tileIdx = uint2(Input.Position.xy) / TILE_SIZE;
uint lightOffset = LightGrid[tileIdx].x;
uint lightCount = LightGrid[tileIdx].y;

// 해당 타일의 라이트만 순회
for (uint i = 0; i < lightCount; ++i) {
    uint lightIndex = LightIndexList[lightOffset + i];
    // 조명 계산...
}
```

#### 2.2.3. Heat Map 시각화 (`ClusterHeatShader.hlsl`)

디버깅을 위해 타일당 라이트 개수를 색상으로 표시하는 Heat Map View Mode를 구현했습니다.

- **파란색**: 라이트 적음 (0~5개)
- **초록색**: 중간 (5~15개)
- **노란색**: 많음 (15~30개)
- **빨간색**: 매우 많음 (30개 이상)

### 2.3. 성능 최적화

- **Compute Shader 기반 Culling**: CPU 부하 없이 GPU에서 타일-라이트 매핑
- **메모리 효율**: 타일당 가변 길이 인덱스 리스트 사용
- **조기 종료**: 라이트 개수가 0인 타일은 조명 계산 스킵

---

## 3. 조명 시스템 구현

### 3.1. Light Component 계층 구조

```
ULightComponentBase (Abstract)
├── UAmbientLightComponent      // 환경광
├── UDirectionalLightComponent  // 방향광
├── UPointLightComponent        // 점광원
└── USpotLightComponent         // 스포트라이트
```

모든 라이트 컴포넌트는 `GetLightData()` 가상 함수를 통해 셰이더에 필요한 데이터를 제공합니다.

### 3.2. 라이트 타입별 구현

#### 3.2.1. Ambient Light (환경광)

- 전역 조명으로 모든 물체에 균일하게 적용
- Intensity와 Color 지원
- 씬당 하나의 Ambient Light 권장

#### 3.2.2. Directional Light (평행광)

- 태양광 시뮬레이션에 적합
- Direction, Intensity, Color 속성
- 거리 감쇠 없음

#### 3.2.3. Point Light (점광원)

- 모든 방향으로 균일하게 빛을 발산
- 역제곱 감쇠 (Inverse Square Law) 적용
- Radius 파라미터로 영향 범위 제어

```hlsl
float attenuation = 1.0 / (distance * distance + 1.0);
```

#### 3.2.4. Spot Light (스포트라이트)

- 원뿔 형태로 빛을 투사
- Inner/Outer Cone Angle 지원
- 부드러운 가장자리 (Smooth Falloff) 구현

```hlsl
float theta = dot(lightDir, -spotDirection);
float epsilon = innerCos - outerCos;
float intensity = saturate((theta - outerCos) / epsilon);
```

### 3.3. Light Actor 및 에디터 통합

- **Billboard Component**: 씬 뷰에서 라이트 위치 시각화
- **Color-coded Icon**: 라이트 색상에 따라 아이콘 색상 변경
- **Abstract Actor 필터링**: Light Component Base는 생성 메뉴에서 제외
- **Detail Panel**: 라이트별 속성 편집 UI 제공

---

## 4. Uber Shader 시스템

### 4.1. 개요

Uber Shader는 여러 셰이더의 기능을 하나로 통합하여, 런타임에 조건부 분기로 원하는 렌더링 모드를 선택하는 아키텍처입니다. 코드 중복을 제거하고 유지보수성을 향상시킵니다.

### 4.2. 핵심 구조

#### 4.2.1. LightingFunctions.hlsl

공통 조명 계산 함수들을 분리한 라이브러리:

```hlsl
// PS_INPUT 구조체 (모든 셰이더 공통)
struct PS_INPUT {
    float4 Position : SV_POSITION;
    float4 WorldPos : POSITION;
    float3 Normal : NORMAL;
    float2 Tex : TEXCOORD0;
    float3 Tangent : TANGENT;
};

// Blinn-Phong 조명 계산
float3 CalculateBlinnPhong(float3 normal, float3 viewDir,
                           float3 lightDir, float3 lightColor,
                           float3 diffuseColor, float specularPower);

// Normal Map 처리
float3 ApplyNormalMap(float3 sampledNormal, float3 normal,
                      float3 tangent);
```

#### 4.2.2. 통합된 Vertex Shader (`TextureVS.hlsl`)

- World/WorldInverseTranspose 행렬 적용
- View Space Normal 계산
- Tangent 공간 벡터 전달

#### 4.2.3. 통합된 Pixel Shader (`TexturePS.hlsl`)

View Mode에 따라 출력 변경:

```hlsl
if (ViewMode == VIEW_MODE_LIT) {
    // Forward+ 조명 계산
} else if (ViewMode == VIEW_MODE_UNLIT) {
    return BaseColor;
} else if (ViewMode == VIEW_MODE_NORMAL) {
    return float4(Normal * 0.5 + 0.5, 1.0);
} else if (ViewMode == VIEW_MODE_HEAT_MAP) {
    // Cluster 시각화
}
```

### 4.3. 장점

- **코드 재사용**: 조명 함수 공유로 중복 제거
- **일관성**: 모든 렌더 패스에서 동일한 조명 결과
- **유지보수**: 한 곳만 수정하면 모든 패스에 반영
- **디버깅**: View Mode 전환으로 실시간 검증

---

## 5. Normal Map 시스템

### 5.1. 구현 과정

#### 5.1.1. Tangent Space 계산

OBJ 파일 로딩 시 CPU에서 Tangent 벡터를 계산합니다 (`ObjManager.cpp`):

```cpp
// 삼각형의 두 엣지와 UV 델타 계산
XMVECTOR edge1 = pos[1] - pos[0];
XMVECTOR edge2 = pos[2] - pos[0];
XMVECTOR deltaUV1 = uv[1] - uv[0];
XMVECTOR deltaUV2 = uv[2] - uv[0];

// Tangent 계산
float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
```

#### 5.1.2. Normal Map 로딩

- 자동 탐지: `filename_normal.jpg/png` 패턴 검색
- 없을 경우: 평면 Normal Map (RGB=128,128,255) 생성
- 텍스처 파일 경로를 Scene 파일에 직렬화

#### 5.1.3. Shader에서의 적용

```hlsl
// Normal Map 샘플링 (Tangent Space)
float3 normalMapSample = NormalTexture.Sample(Sampler, Input.Tex).rgb;
normalMapSample = normalMapSample * 2.0 - 1.0; // [0,1] -> [-1,1]

// Tangent Space -> World Space 변환
float3 N = normalize(Input.Normal);
float3 T = normalize(Input.Tangent - dot(Input.Tangent, N) * N);
float3 B = cross(N, T);
float3x3 TBN = float3x3(T, B, N);
float3 worldNormal = mul(normalMapSample, TBN);
```

### 5.2. Normal View Mode

Normal Map이 올바르게 적용되었는지 확인하기 위한 시각화 모드:

- RGB 값으로 Normal 방향 표시
- 배경은 검은색으로 유지 (깊이 테스트 실패 시 discard)

---

## 6. Render Pass 구조 재정립

### 6.1. Ping-Pong 버퍼 시스템

두 개의 Render Target을 번갈아 사용하여 여러 Post-Process 효과를 순차 적용:

```
Scene -> RT0 (StaticMeshPass)
      -> RT1 (FogPass, input: RT0)
      -> RT0 (FXAAPass, input: RT1)
      -> BackBuffer (BlitPass, input: RT0)
```

### 6.2. Render Pass 순서

1. **SceneDepthPass**: Depth Pre-pass
2. **StaticMeshPass**: Forward+ 조명 계산
3. **DefaultViewPass**: 특수 View Mode 처리
4. **BillboardPass**: 라이트 아이콘 렌더링
5. **FogPass**: Height Fog 적용
6. **FXAAPass**: Anti-Aliasing
7. **BlitPass**: 최종 출력

### 6.3. Rendering Context

모든 Pass가 공유하는 렌더링 상태:

```cpp
struct FRenderingContext {
    ID3D11RenderTargetView* CurrentRTV;
    ID3D11DepthStencilView* DSV;
    D3D11_VIEWPORT Viewport;
    EEngineShowFlags ShowFlags;
    EViewMode ViewMode;
};
```

### 6.4. 개선 사항

- **모듈화**: 각 Pass가 독립적으로 동작
- **확장성**: 새로운 Pass 추가 용이
- **디버깅**: Pass별 출력 확인 가능
- **성능**: Depth Pre-pass로 Overdraw 감소

---

## 7. View Mode 확장

### 7.1. 지원하는 View Mode

- **Lit**: 기본 조명 렌더링 (Forward+)
- **Unlit**: 조명 없이 Base Color만 표시
- **Normal**: Normal Map 적용 결과 시각화
- **Depth**: Depth Buffer 시각화
- **Wireframe**: 와이어프레임 모드
- **Heat Map**: Cluster별 라이트 밀도 표시

### 7.2. 런타임 전환

UI 버튼 클릭으로 실시간 View Mode 전환:

```cpp
void OnViewModeChanged(EViewMode NewMode) {
    Renderer->SetViewMode(NewMode);
    // 셰이더 상수 버퍼 업데이트
    ViewModeBuffer.ViewMode = static_cast<int>(NewMode);
}
```

---

## 8. WorldInverseTranspose 적용

### 8.1. 문제점

Non-uniform Scale 변환 시 World 행렬로 Normal을 변환하면 방향이 왜곡됩니다.

### 8.2. 해결책

Normal 벡터는 World 행렬의 역전치(Inverse Transpose)로 변환:

```hlsl
// Vertex Shader
Output.Normal = normalize(mul(float4(Input.Normal, 0.0f), WorldInverseTranspose).xyz);
```

```cpp
// CPU에서 계산
XMMATRIX world = actor->GetWorldMatrix();
XMMATRIX worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
```

### 8.3. Sphere 특수 처리

Sphere 모델은 정점 위치를 정규화한 것이 Normal이므로, 별도 로직 적용:

```cpp
if (mesh->IsSphere()) {
    normal = XMVector3Normalize(position);
}
```

---

## 9. 기타 개선 사항

### 9.1. 버그 수정

- RenderDoc 크래시 이슈 해결
- Fog Pass에서 Fog가 안 나오던 문제 수정
- Normal View에서 배경 색상 변경 방지
- Ambient Light 삭제 시 크래시 수정
- 여러 병합 충돌 해결

### 9.2. 에디터 개선

- vcxproj.filters 파일 구조 정리
- .editorconfig 추가
- 다수의 테스트 씬 추가 (L3~L8, LightTest 등)
- 통계 오버레이 개선

### 9.3. 성능 최적화

- Depth Pre-pass로 Fragment 연산 감소
- Compute Shader 기반 Culling
- Structured Buffer로 라이트 데이터 전송

---

## 10. 결론 및 향후 계획

### 10.1. 주요 성과

- ✅ **Forward+ 렌더링 파이프라인 완성**: 수백 개 라이트 실시간 처리
- ✅ **완전한 조명 시스템**: 4종 라이트 타입 지원
- ✅ **Uber Shader 아키텍처**: 유지보수성 대폭 향상
- ✅ **Normal Map 시스템**: 디테일 표현력 증가
- ✅ **Render Pass 재구성**: 확장 가능한 파이프라인

### 10.2. 통계

- **총 커밋**: 97개
- **주요 기여자**: lorevoon (26), nayechan (18), dack-c (17), Donghee (14)
- **변경 파일**: 수백 개 (셰이더, 소스 코드, 씬 등)
- **주요 PR**: #33 (Forward+), #26 (Uber Shader), #25 (Spot Light), #28 (Unlit)

### 10.3. 향후 개선 방향

- Shadow Mapping 구현
- Screen Space Reflections (SSR)
- Physically Based Rendering (PBR) 머티리얼
- Deferred Rendering 파이프라인 추가
- Volumetric Lighting

---

## 11. UPROPERTY 시스템

### 11.1. 개요

UPROPERTY 시스템은 UObject 기반 클래스의 멤버 변수를 런타임 리플렉션 시스템에 등록하여, 자동 직렬화, 복제, UI 생성 등을 지원하는 메타데이터 시스템입니다.

**주요 기능:**
- 자동 직렬화/역직렬화 (JSON)
- 자동 객체 복제
- 에디터 UI 자동 생성
- 타입 안전성 보장
- 메타데이터 기반 제약 조건

### 11.2. 기본 사용법

#### 11.2.1. UPROPERTY 매크로 종류

```cpp
// 1. 기본 선언 (기본값 없음, 메타데이터 없음)
UPROPERTY(Type, Name)

// 2. 기본값 포함
UPROPERTY_INIT(Type, Name, DefaultValue)

// 3. 플래그만 지정
UPROPERTY_INIT_WITHMETA(Type, Name, DefaultValue, UPROPERTY_FLAGS(Flags))

// 4. 완전한 메타데이터 지정
UPROPERTY_INIT_WITHMETA(Type, Name, DefaultValue, FPropertyMetadata({
    .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
    .Min = 0.0,
    .Max = 100.0,
    .Step = 1.0,
    .DisplayName = "Property Name",
    .Tooltip = "Description here",
    .bSlider = true
}))
```

#### 11.2.2. 사용 예제

```cpp
UCLASS()
class USpotLightComponent : public UPointLightComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(USpotLightComponent, UPointLightComponent)

public:
    // 편집 가능 + 저장되는 float 프로퍼티 (슬라이더 UI)
    UPROPERTY_INIT_WITHMETA(float, OuterConeAngle, 45.0f, FPropertyMetadata({
        .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
        .Min = 0.0f,
        .Max = 90.0f,
        .DisplayName = "Outer Cone Angle",
        .Tooltip = "Outer angle of the spotlight cone",
        .bSlider = true
    }));

    // 단순 플래그만 지정
    UPROPERTY_INIT_WITHMETA(FString, Description, "Default",
        UPROPERTY_FLAGS(EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame));

    // 복제 시 리셋되는 프로퍼티
    UPROPERTY_INIT_WITHMETA(int32, RuntimeCounter, 0,
        UPROPERTY_FLAGS(EPropertyFlags::EditAnywhere | EPropertyFlags::DuplicateTransient));
};
```

### 11.3. 프로퍼티 플래그

```cpp
enum class EPropertyFlags : uint64
{
    None = 0,

    // 에디터 표시
    EditAnywhere = 1ULL << 0,      // 에디터에서 편집 가능
    VisibleAnywhere = 1ULL << 1,   // 에디터에서 읽기 전용 표시

    // 직렬화/복제
    SaveGame = 1ULL << 2,           // JSON 저장 파일에 포함
    DuplicateTransient = 1ULL << 3  // 복제 시 기본값으로 리셋
};
```

**플래그 조합 예시:**
```cpp
// 편집 가능 + 저장됨
EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame

// 읽기 전용으로 표시만
EPropertyFlags::VisibleAnywhere

// 편집 가능하지만 복제 시 리셋
EPropertyFlags::EditAnywhere | EPropertyFlags::DuplicateTransient
```

### 11.4. 메타데이터 옵션

```cpp
struct FPropertyMetadata
{
    EPropertyFlags Flags;           // 프로퍼티 플래그
    double Min;                     // 최소값 (숫자 타입)
    double Max;                     // 최대값 (숫자 타입)
    double Step;                    // 증감 단위 (기본: 1.0)
    const char* Category;           // 카테고리 (미사용)
    const char* Tooltip;            // 툴팁 텍스트
    const char* DisplayName;        // UI 표시 이름
    bool bSlider;                   // Slider UI 사용 여부
    bool bReadOnly;                 // 읽기 전용 여부
};
```

### 11.5. 지원하는 타입

#### 11.5.1. 기본 타입
- `int8`, `int16`, `int32`, `int64`
- `uint8`, `uint16`, `uint32`, `uint64`
- `float`, `double`
- `bool`
- `FString`, `FName`

#### 11.5.2. 벡터 및 색상 타입
```cpp
// 벡터 타입
UPROPERTY_INIT_WITHMETA(FVector2, Position2D, FVector2(0, 0), ...);
UPROPERTY_INIT_WITHMETA(FVector, Position, FVector(0, 0, 0), ...);
UPROPERTY_INIT_WITHMETA(FVector4, Vector4, FVector4(0, 0, 0, 1), ...);

// 색상 타입 (컬러 피커 UI 자동 생성)
UPROPERTY_INIT_WITHMETA(FLinearColor3, Color, FLinearColor3(1, 1, 1), ...);
UPROPERTY_INIT_WITHMETA(FLinearColor, ColorWithAlpha, FLinearColor(1, 1, 1, 1), ...);
```

#### 11.5.3. JSON 직렬화 형식

벡터 및 색상 타입은 JSON 배열로 직렬화됩니다:
```json
{
    "Position": [1.000000, 2.000000, 3.000000],
    "Color": [1.000000, 0.500000, 0.250000],
    "ColorWithAlpha": [1.000000, 0.500000, 0.250000, 0.800000]
}
```

### 11.6. 자동 직렬화 및 복제

#### 11.6.1. 자동 직렬화 (Serialize)

`SaveGame` 플래그가 있는 프로퍼티는 자동으로 JSON에 저장/로드됩니다:

```cpp
void UObject::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    // UPROPERTY 시스템이 자동으로 처리
    // SaveGame 플래그가 있는 모든 프로퍼티를 자동 직렬화
}
```

#### 11.6.2. 자동 복제 (Duplicate)

객체 복제 시 `DuplicateTransient` 플래그가 없는 프로퍼티만 복사됩니다:

```cpp
UObject* NewObject = OriginalObject->Duplicate();
// DuplicateTransient가 아닌 모든 프로퍼티 자동 복제
```

### 11.7. 자동 UI 생성

#### 11.7.1. ComponentWidget 통합

모든 컴포넌트 위젯은 `UComponentWidget`을 상속받아 자동으로 UPROPERTY UI를 렌더링합니다:

```cpp
void MyCustomWidget::RenderWidget()
{
    // 먼저 UPROPERTY 자동 렌더링
    Super::RenderWidget();

    // 그 다음 커스텀 UI 추가
    ImGui::Text("Additional UI");
}
```

#### 11.7.2. 타입별 UI

| 타입 | UI 컨트롤 |
|------|----------|
| `int32` | `DragInt` 또는 `SliderInt` |
| `float` | `DragFloat` 또는 `SliderFloat` |
| `bool` | `Checkbox` |
| `FString` | `InputText` |
| `FVector2` | `DragFloat2` |
| `FVector` | `DragFloat3` |
| `FVector4` | `DragFloat4` |
| `FLinearColor3` | `ColorEdit3` (RGB 컬러 피커) |
| `FLinearColor` | `ColorEdit4` (RGBA 컬러 피커) |

### 11.8. 고급 사용 예제

```cpp
UCLASS()
class UMyComponent : public UActorComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UMyComponent, UActorComponent)

public:
    // 슬라이더로 편집 가능한 강도
    UPROPERTY_INIT_WITHMETA(float, Intensity, 5.0f, FPropertyMetadata({
        .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
        .Min = 0.0,
        .Max = 20.0,
        .Step = 0.1,
        .DisplayName = "Light Intensity",
        .Tooltip = "Brightness of the light source",
        .bSlider = true
    }));

    // 색상 피커
    UPROPERTY_INIT_WITHMETA(FLinearColor3, LightColor, FLinearColor3(1, 1, 1),
        FPropertyMetadata({
            .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
            .DisplayName = "Light Color"
        }));

    // 위치 벡터
    UPROPERTY_INIT_WITHMETA(FVector, Offset, FVector(0, 0, 0),
        FPropertyMetadata({
            .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
            .Step = 0.01,
            .DisplayName = "Position Offset"
        }));

    // 런타임 전용 (저장 안됨)
    UPROPERTY_INIT_WITHMETA(int32, FrameCount, 0,
        UPROPERTY_FLAGS(EPropertyFlags::VisibleAnywhere));

    // 복제 시 리셋
    UPROPERTY_INIT_WITHMETA(bool, bIsActive, false,
        UPROPERTY_FLAGS(EPropertyFlags::EditAnywhere | EPropertyFlags::DuplicateTransient));
};
```

### 11.9. 주의사항

1. **타입 일치**: 기본값의 타입과 선언된 타입이 일치해야 합니다
   ```cpp
   // ❌ 잘못된 예
   UPROPERTY_INIT_WITHMETA(int32, Value, 3.5f, ...)  // float를 int에 대입

   // ✅ 올바른 예
   UPROPERTY_INIT_WITHMETA(int32, Value, 42, ...)
   ```

2. **세미콜론**: UPROPERTY 매크로는 반드시 세미콜론으로 끝나야 합니다
   ```cpp
   UPROPERTY_INIT_WITHMETA(float, Value, 1.0f, ...)  // ❌ 세미콜론 없음
   UPROPERTY_INIT_WITHMETA(float, Value, 1.0f, ...); // ✅ 세미콜론 있음
   ```

3. **Min/Max 제약**: infinity 값은 자동으로 타입별 최대/최소값으로 변환됩니다
   - `float`: `±FLT_MAX`
   - `int32`: `INT_MIN` ~ `INT_MAX`

4. **상속**: 부모 클래스의 UPROPERTY도 자동으로 UI에 표시됩니다
