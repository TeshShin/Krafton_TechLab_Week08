## 내가 맡은 부분 — 신동민

**기간** 2025.10.23 – 10.30 · **내 커밋** 26건

| 구현 | 내용 |
| --- | --- |
| **PSM (Perspective Shadow Map)** | 스포트라이트용 PSM 구현. **W가 음수로 나올 때 양수로 바꾸는 것이 아니라, 음수가 나오는 상황 자체를 회피**하도록 처리하고 `W<0`인 경우 역투영 경로로 분기. 섀도우 아크네는 **컬링 모드를 front로** 바꿔 해결하고 래스터라이저 상태를 함께 수정 |
| **라이트** | 라이트 종류 선택 UI, 라이트 매트릭스 인수 변경, 라이트별 빌보드 정상화 |
| **회전** | 위젯 회전을 쿼터니언 기반으로 전환(LH 표기), `Cross` 정상화 |
| **컬링** | 프러스텀 컬링 수정 |

**주요 파일** `Engine/Source/Scene/Private/Component/SpotLightComponent.cpp` · `Engine/Source/Renderer/Private/RenderPass/ShadowPass.cpp` · `Engine/Source/Core/Private/Math/Quaternion.cpp`

→ **[내 커밋 26건 보기](https://github.com/TeshShin/Krafton_TechLab_Week08/commits?author=TeshShin)** · [14주 전체 정리](https://github.com/TeshShin/Krafton-TechLab-Roles)

---

## 8주차(2025.10.23 – 10.30)에 추가된 것 — 그림자 시스템

아래 기술문서는 7주차 내용까지를 담고 있어, 8주차에 들어간 그림자 시스템을 여기에 정리합니다.

- **섀도우 맵 종류** — CSM(Cascaded Shadow Map), VSM(Variance Shadow Map), PSM(Perspective Shadow Map)
- **광원별 그림자** — Directional Light Shadow, Point Light Shadow, Spotlight Shadow
- **필터링** — PCF, Box Filter, Gaussian Filter
- **CSM 세부** — SubFrustum별 Depth Map, Moment RTV를 Array로 전환
- **에디터 연동** — Shadow ShowFlag, ShadowSettings UI, Shadow Stat

---

# KTL Engine - Technical Documentation

## 프로젝트 개요

**KTL Engine**은 DirectX 11 기반의 실시간 3D 렌더링 엔진으로, Forward+ (Clustered Forward Shading) 렌더링 파이프라인, 완전한 조명 시스템, 그리고 현대적인 셰이더 관리 시스템을 갖추고 있습니다.

**주요 특징**:
- ✅ **Forward+ Rendering**: 수백 개의 동적 광원 실시간 처리
- ✅ **완전한 조명 시스템**: Directional, Point, Spot, Ambient Light 지원
- ✅ **Shader Hot-Reload & Binary Caching**: 빠른 개발 및 최적화된 런타임 성능
- ✅ **UPROPERTY 시스템**: 자동 직렬화, 복제, UI 생성
- ✅ **계층적 Visibility 시스템**: SceneComponent 기반 Visibility 관리
- ✅ **Uber Shader 아키텍처**: 유지보수 용이한 셰이더 구조

---

## Week7-8 작업 기간 (2025.10.16 ~ 2025.10.23)

본 문서는 KTL 엔진에 Week7-8 기간(2025.10.16 ~ 2025.10.23)에 구현된 주요 시스템의 기술적 사양과 아키텍처를 설명합니다.

이 기간 동안 **Clustered Forward Shading (Forward+)** 렌더링 파이프라인 완성, **Shader Hot-Reload & Binary Caching 시스템**, **UPROPERTY 리플렉션 시스템**, **계층적 Visibility 시스템** 등 대규모 인프라 개선 작업이 진행되었습니다.

총 **120+ 개의 커밋**으로 구성되며, 렌더링 성능, 개발 효율성, 유지보수성을 크게 향상시켰습니다.

---

## 2. Clustered Forward Shading (Forward+) 구현

### 2.1. 개요

Forward+ 렌더링은 화면을 3D 클러스터(타일 + 깊이 슬라이스)로 나누어 각 클러스터에 영향을 미치는 라이트만 처리함으로써, 수백~수천 개의 동적 광원을 효율적으로 렌더링할 수 있는 기법입니다. 기존 Forward Rendering의 단순함을 유지하면서도 Deferred Rendering 수준의 조명 성능을 달성합니다.

**구현 완료 날짜**: 2025.10.22 (Main 브랜치 머지)

**주요 개선사항** (10.20-10.22):
- ✅ Spot Light Cone-Frustum Intersection 정밀 구현
- ✅ Orthographic & Perspective Projection 모두 지원
- ✅ Compute Shader numthreads 최적화 (8×8×1)
- ✅ 타일 크기: 16×16 픽셀, Z-슬라이스: 24개 (로그 분할)

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

- **총 커밋**: 120+개
- **작업 기간**: 2025.10.16 ~ 2025.10.23 (8일)
- **주요 기여자**: lorevoon, nayechan, dack-c, Donghee
- **변경 파일**: 수백 개 (셰이더, 소스 코드, 씬, 시스템 등)
- **주요 PR**:
  - #33 (Forward+)
  - #44 (Shader Hot-Reload & Binary Caching)
  - #41 (UPROPERTY System)
  - #45 (Hierarchical Visibility)
  - #47 (Shader Folder-based Recompilation)

### 10.3. 향후 개선 방향

- Shadow Mapping 구현
- Screen Space Reflections (SSR)
- Physically Based Rendering (PBR) 머티리얼
- Deferred Rendering 파이프라인 추가
- Volumetric Lighting

---

## 11. Hierarchical Visibility 시스템

### 11.1. 개요

계층적 Visibility 시스템은 `USceneComponent`의 부모-자식 관계를 활용하여, **부모 컴포넌트가 숨겨지면 자식도 자동으로 숨겨지는** 기능을 제공합니다.

**구현 날짜**: 2025.10.22 (PR #45)

**주요 기능**:
- ✅ `SetVisibility(bool bNewVisibility)`: 현재 컴포넌트와 모든 자식 Visibility 설정
- ✅ `IsVisibleInHierarchy()`: 부모 체인 전체 Visibility 확인
- ✅ Abstract Component 생성 방지: `MeshComponent`, `PrimitiveComponent` 등은 생성 메뉴에서 제외

### 11.2. 구현 상세

#### 11.2.1. SetVisibility (재귀적 Visibility 설정)

```cpp
void USceneComponent::SetVisibility(bool bNewVisibility)
{
    bVisible = bNewVisibility;

    // 재귀적으로 모든 자식 컴포넌트에 적용
    for (USceneComponent* Child : Children)
    {
        if (Child)
        {
            Child->SetVisibility(bNewVisibility);
        }
    }
}
```

#### 11.2.2. IsVisibleInHierarchy (부모 체인 확인)

```cpp
bool USceneComponent::IsVisibleInHierarchy() const
{
    if (!bVisible)
        return false;

    // 부모가 있으면 부모의 Visibility도 확인
    if (AttachParent)
        return AttachParent->IsVisibleInHierarchy();

    return true;
}
```

#### 11.2.3. 렌더링 통합

모든 렌더 패스에서 `IsVisibleInHierarchy()` 사용:

```cpp
// StaticMeshPass.cpp
for (UStaticMeshComponent* MeshComp : Context.StaticMeshes)
{
    if (!MeshComp->IsVisibleInHierarchy()) continue;
    // 렌더링...
}

// BillboardPass.cpp
for (ULightComponentBase* Light : Context.Lights)
{
    if (!Light->IsVisibleInHierarchy()) continue;
    // Billboard 렌더링...
}
```

### 11.3. Abstract Component 필터링

특정 컴포넌트는 직접 생성할 수 없도록 Abstract로 표시:

```cpp
// MeshComponent.cpp
UClass* UMeshComponent::StaticClass()
{
    static UClass* Class = new UClass(
        "MeshComponent", "USceneComponent",
        sizeof(UMeshComponent),
        true  // bIsAbstract = true
    );
    return Class;
}
```

**Abstract로 표시된 컴포넌트**:
- `UMeshComponent`
- `UPrimitiveComponent`
- `ULightComponentBase`

**에디터 UI**: Abstract 컴포넌트는 "Add Component" 메뉴에서 자동 제외됩니다.

### 11.4. 사용 예시

```cpp
// Actor에 라이트 추가 (Billboard 포함)
UPointLightComponent* Light = Actor->AddComponent<UPointLightComponent>();
UBillboardComponent* Billboard = Actor->AddComponent<UBillboardComponent>();
Billboard->AttachToComponent(Light);  // Light의 자식으로 설정

// 라이트를 숨기면 Billboard도 자동으로 숨겨짐
Light->SetVisibility(false);

// 확인
assert(Light->IsVisibleInHierarchy() == false);
assert(Billboard->IsVisibleInHierarchy() == false);  // 부모가 숨겨져서 false
```

### 13.5. 장점

- ✅ **일관성**: 부모-자식 관계가 Visibility에 자동 반영
- ✅ **편의성**: 한 줄로 전체 계층 숨기기/보이기
- ✅ **안전성**: Abstract 컴포넌트 직접 생성 방지
- ✅ **성능**: 숨겨진 컴포넌트는 렌더링 파이프라인에서 조기 제외

---

## 12. UPROPERTY 시스템

### 12.1. 개요

UPROPERTY 시스템은 UObject 기반 클래스의 멤버 변수를 런타임 리플렉션 시스템에 등록하여, 자동 직렬화, 복제, UI 생성 등을 지원하는 메타데이터 시스템입니다.

**주요 기능:**
- 자동 직렬화/역직렬화 (JSON)
- 자동 객체 복제
- 에디터 UI 자동 생성
- 타입 안전성 보장
- 메타데이터 기반 제약 조건

### 12.2. 기본 사용법

#### 12.2.1. UPROPERTY 매크로 종류

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

#### 12.2.2. 사용 예제

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

### 12.3. 프로퍼티 플래그

```cpp
enum class EPropertyFlags : uint64
{
    None = 0,

    // 에디터 표시
    EditAnywhere = 1ULL << 0,      // 에디터에서 편집 가능
    VisibleAnywhere = 1ULL << 1,   // 에디터에서 표시됨됨

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

### 12.4. 메타데이터 옵션

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

### 12.5. 지원하는 타입

#### 12.5.1. 기본 타입
- `int8`, `int16`, `int32`, `int64`
- `uint8`, `uint16`, `uint32`, `uint64`
- `float`, `double`
- `bool`
- `FString`, `FName`

#### 12.5.2. 벡터 및 색상 타입
```cpp
// 벡터 타입
UPROPERTY_INIT_WITHMETA(FVector2, Position2D, FVector2(0, 0), ...);
UPROPERTY_INIT_WITHMETA(FVector, Position, FVector(0, 0, 0), ...);
UPROPERTY_INIT_WITHMETA(FVector4, Vector4, FVector4(0, 0, 0, 1), ...);

// 색상 타입 (컬러 피커 UI 자동 생성)
UPROPERTY_INIT_WITHMETA(FLinearColor3, Color, FLinearColor3(1, 1, 1), ...);
UPROPERTY_INIT_WITHMETA(FLinearColor, ColorWithAlpha, FLinearColor(1, 1, 1, 1), ...);
```

#### 12.5.3. JSON 직렬화 형식

벡터 및 색상 타입은 JSON 배열로 직렬화됩니다:
```json
{
    "Position": [1.000000, 2.000000, 3.000000],
    "Color": [1.000000, 0.500000, 0.250000],
    "ColorWithAlpha": [1.000000, 0.500000, 0.250000, 0.800000]
}
```

### 12.6. 자동 직렬화 및 복제

#### 12.6.1. 자동 직렬화 (Serialize)

`SaveGame` 플래그가 있는 프로퍼티는 자동으로 JSON에 저장/로드됩니다:

```cpp
void UObject::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    // UPROPERTY 시스템이 자동으로 처리
    // SaveGame 플래그가 있는 모든 프로퍼티를 자동 직렬화
}
```

#### 12.6.2. 자동 복제 (Duplicate)

객체 복제 시 `DuplicateTransient` 플래그가 없는 프로퍼티만 복사됩니다:

```cpp
UObject* NewObject = OriginalObject->Duplicate();
// DuplicateTransient가 아닌 모든 프로퍼티 자동 복제
```

### 12.7. 자동 UI 생성

#### 12.7.1. ComponentWidget 통합

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

#### 12.7.2. 타입별 UI

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

### 12.8. 고급 사용 예제

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

### 12.9. 주의사항

1. **타입 일치**: 기본값의 타입과 선언된 타입이 일치해야 합니다
   ```cpp
   // ❌ 잘못된 예
   UPROPERTY_INIT_WITHMETA(int32, Value, 3.5f, ...)  // float를 int에 대입

   // ✅ 올바른 예
   UPROPERTY_INIT_WITHMETA(int32, Value, 42, ...)
   ```

2. **세미콜론**: UPROPERTY 매크로는 세미콜론으로 끝나지 않아도 됩니다.
   ```cpp
   UPROPERTY_INIT_WITHMETA(float, Value, 1.0f, ...)  // ✅ 세미콜론 없음
   UPROPERTY_INIT_WITHMETA(float, Value, 1.0f, ...); // ✅ 세미콜론 있음
   ```

3. **Min/Max 제약**: infinity 값은 자동으로 타입별 최대/최소값으로 변환됩니다
   - `float`: `±FLT_MAX`
   - `int32`: `INT_MIN` ~ `INT_MAX`

4. **상속**: 부모 클래스의 UPROPERTY도 자동으로 UI에 표시됩니다

---

## 13. Shader 시스템 (Hot-Reload & Binary Caching)

### 13.1. 개요

KTL 엔진의 Shader 시스템은 **Hot-Reload**와 **Binary Caching**을 지원하여 빠른 셰이더 개발 및 최적화된 런타임 성능을 제공합니다.

**주요 기능:**
- ✅ **Hot-Reload**: 셰이더 파일 수정 시 실시간 재컴파일 (F4 키 + 0.5초 자동 감지)
- ✅ **Flyweight 패턴**: 동일한 셰이더 variant는 하나의 객체로 공유
- ✅ **Binary Caching**: 컴파일된 셰이더를 `.cso` 파일로 저장하여 빠른 시작
- ✅ **MD5 + Timestamp 검증**: 소스 변경 감지 및 캐시 무효화
- ✅ **ComPtr 기반 관리**: 자동 참조 카운팅으로 메모리 안전성 보장

### 13.2. 아키텍처

#### 13.2.1. 계층 구조

```
┌─────────────────┐
│   RenderPass    │  Raw pointer 소유 (ID3D11VertexShader*)
└────────┬────────┘
         │ calls
         ▼
┌─────────────────────────────┐
│ RenderResourceFactory       │  [DEPRECATED] Legacy wrapper
│  - CreateVertexShader       │  (하위 호환성 유지)
│  - CreatePixelShader        │
└────────┬────────────────────┘
         │ delegates to
         ▼
┌─────────────────────────────┐
│ ShaderFactory (NEW)         │  Modern API
│  - CreateVertexShader()     │  ├─ 생성 전용 (SRP 준수)
│  - CreatePixelShader()      │  ├─ Helper 함수 제공
│  - CreateComputeShader()    │  └─ Pool 사용
└────────┬────────────────────┘
         │ uses
         ▼
┌─────────────────────────────┐
│ FShaderPool                 │  Flyweight pattern
│  - VSCache (ComPtr map)     │  ├─ 셰이더 객체 공유
│  - PSCache (ComPtr map)     │  ├─ 참조 카운팅
│  - CSCache (ComPtr map)     │  └─ Binary Cache 연동
└────────┬────────────────────┘
         │ uses
         ▼
┌─────────────────────────────┐
│ FShaderBinaryCache          │  .cso 파일 I/O
│  - LoadFromCache()          │  ├─ MD5 해시 검증
│  - SaveToCache()            │  ├─ Shader Folder Timestamp 검증
│  - IsCacheValid()           │  └─ Compile Flags 검증
│  - GetShaderFolderTimestamp │
└─────────────────────────────┘

         ┌─────────────────────┐
         │ FShaderManager      │  Hot-reload tracking
         │  - RegisterVS/PS/CS │  ├─ 포인터 주소 추적
         │  - ReloadShader()   │  ├─ 파일 변경 감지
         │  - RecompileVariant │  └─ Pool 재컴파일 요청
         └─────────────────────┘
```

### 13.3. Shader Key 시스템

동일한 셰이더 variant를 식별하기 위한 고유 키:

```cpp
struct FShaderKey {
    wstring SourcePath;              // e.g., L"Asset/Shader/TextureVS.hlsl"
    TArray<FShaderDefine> Defines;   // 전처리기 매크로 (정렬됨)
    EShaderType Type;                // VertexShader, PixelShader, ComputeShader

    size_t GetHash() const;          // Hash for TMap lookup
    wstring GetCacheFileName() const; // e.g., "TextureVS_PHONG_1A2B3C4D.cso"
};
```

**예시:**
- `TextureVS.hlsl + LIGHTING_MODEL=PHONG + VertexShader` → Key1
- `TextureVS.hlsl + LIGHTING_MODEL=GOURAUD + VertexShader` → Key2
- 다른 키 = 별도 컴파일 및 캐싱

### 13.4. 사용 방법

#### 13.4.1. 기존 코드 (RenderResourceFactory - Deprecated)

하위 호환성을 위해 기존 API는 그대로 동작합니다:

```cpp
// RenderPass에서 사용
ID3D11VertexShader* VS = nullptr;
ID3D11InputLayout* Layout = nullptr;

FRenderResourceFactory::CreateVertexShaderAndInputLayout(
    L"Asset/Shader/MyShader.hlsl",
    LayoutDescs,
    &VS,
    &Layout,
    nullptr,      // Defines
    true          // Enable hot-reload
);
```

#### 13.4.2. 새로운 코드 (ShaderFactory - Recommended)

새 코드는 두 단계 패턴을 사용합니다:

```cpp
class MyRenderPass {
    ID3D11VertexShader* VS = nullptr;
    ID3D11InputLayout* Layout = nullptr;
    ID3D11PixelShader* PS = nullptr;

    void Initialize() {
        // Define macros
        D3D_SHADER_MACRO Defines[] = {
            { "LIGHTING_MODEL", "PHONG" },
            { nullptr, nullptr }
        };

        // Step 1: Create shaders using ShaderFactory
        FShaderKey VSKey = ShaderFactory::CreateShaderKey(
            L"Asset/Shader/MyShader.hlsl",
            Defines,
            EShaderType::VertexShader
        );
        VS = ShaderFactory::CreateVertexShader(VSKey, &Layout, &LayoutDescs);

        FShaderKey PSKey = ShaderFactory::CreateShaderKey(
            L"Asset/Shader/MyShader.hlsl",
            Defines,
            EShaderType::PixelShader
        );
        PS = ShaderFactory::CreatePixelShader(PSKey);

        // Step 2: Register for hot-reload (optional)
        FShaderManager::Get().RegisterVertexShader(
            L"Asset/Shader/MyShader.hlsl",
            LayoutDescs,
            &VS,        // Pointer address for hot-reload
            &Layout,
            Defines
        );
        FShaderManager::Get().RegisterPixelShader(
            L"Asset/Shader/MyShader.hlsl",
            &PS,
            Defines
        );
    }

    void Release() {
        SafeRelease(VS);
        SafeRelease(Layout);
        SafeRelease(PS);
    }
};
```

**장점:**
- ✅ **명확한 책임 분리**: 생성(ShaderFactory) vs 등록(ShaderManager)
- ✅ **선택적 Hot-Reload**: Step 2 생략 가능 (임시 셰이더)
- ✅ **SOLID 원칙 준수**: 각 컴포넌트가 단일 책임만 가짐

### 13.5. Hot-Reload 동작 방식

#### 13.5.1. 자동 감지 (0.5초 간격)

```cpp
// Engine 메인 루프
static float TimeSinceLastCheck = 0.0f;
TimeSinceLastCheck += DeltaTime;

if (TimeSinceLastCheck >= 0.5f) {
    FShaderManager::Get().CheckAndReloadModifiedShaders();
    TimeSinceLastCheck = 0.0f;
}
```

#### 13.5.2. Shader Folder 기반 변경 감지

셰이더 시스템은 **폴더 전체의 타임스탬프**를 추적하여 `#include`로 인한 종속성 변경을 자동으로 감지합니다:

```cpp
// 셰이더 폴더의 모든 .hlsl 파일 중 최신 타임스탬프 획득
FILETIME GetShaderFolderTimestamp(const wstring& ShaderFolderPath);

// 폴더 타임스탬프가 변경되면 모든 셰이더 재컴파일
if (IsFileTimeNewer(CurrentFolderTimestamp, LastShaderFolderTimestamp)) {
    ReloadAllShaders();
}
```

**장점:**
- ✅ `#include "LightingFunctions.hlsl"` 등 공통 파일 수정 시 자동 재컴파일
- ✅ 개별 파일 추적보다 간단하고 안정적
- ✅ 누락 없이 모든 종속성 변경 감지

#### 13.5.3. 수동 리로드 (F4 키)

```cpp
// 모든 등록된 셰이더 리컴파일
FShaderManager::Get().ReloadAllShaders();

// 특정 파일만 리컴파일
FShaderManager::Get().ReloadShader(L"Asset/Shader/MyShader.hlsl");
```

#### 13.5.4. 안전한 리컴파일

- 컴파일 실패 시: Old shader 유지 (크래시 방지)
- 성공 시: RenderPass의 포인터를 새 셰이더로 자동 교체
- ComPtr로 관리: 메모리 누수 없음

### 13.6. Binary Cache 시스템

#### 12.6.1. Cache 파일 구조

```
.cso File Format (Version 3):
┌──────────────────────────┐
│ FShaderCacheHeader       │  Magic number + Version + MD5 hash
├──────────────────────────┤
│ FShaderCacheMetadata     │  Source path + Defines + ShaderFolderTimestamp + CompileFlags
├──────────────────────────┤
│ Bytecode (variable size) │  Compiled shader bytecode
└──────────────────────────┘
```

**Cache Version 3 변경사항:**
- ✅ `ShaderFolderTimestamp` 추가: 폴더 내 모든 `.hlsl` 파일의 최신 타임스탬프 저장
- ✅ `CompileFlags` 추가: 디버그/릴리스 빌드 또는 최적화 플래그 변경 시 캐시 무효화
- ❌ `CompileTimestamp` 제거: 개별 파일 타임스탬프 대신 폴더 타임스탬프 사용

#### 13.6.2. Cache Invalidation (캐시 무효화)

캐시가 유효한지 검증하는 3가지 기준:

```cpp
// 1. MD5 hash comparison (소스 코드 또는 Defines 변경 감지)
bool IsValid = (CachedMD5 == CurrentMD5);

// 2. Shader folder timestamp comparison (include 파일 변경 감지)
FILETIME ShaderFolderTimestamp = GetShaderFolderTimestamp("Asset/Shader");
bool IsFolderModified = IsFileTimeNewer(ShaderFolderTimestamp, CachedFolderTimestamp);

// 3. Compile flags comparison (빌드 설정 변경 감지)
bool FlagsChanged = (CachedCompileFlags != CurrentCompileFlags);

// Load from cache only if all checks pass
if (IsValid && !IsFolderModified && !FlagsChanged) {
    LoadFromCache();
} else {
    CompileFromSource();
    SaveToCache();
}
```

**무효화 트리거:**
- 📝 **소스 코드 변경**: `.hlsl` 파일 수정 → MD5 해시 변경
- 📝 **Defines 변경**: 전처리기 매크로 추가/수정 → MD5 해시 변경
- 📝 **Include 파일 변경**: `LightingFunctions.hlsl` 등 공통 파일 수정 → 폴더 타임스탬프 변경
- 🛠️ **빌드 설정 변경**: Debug ↔ Release, 최적화 플래그 변경 → CompileFlags 변경

### 13.7. 성능 최적화

#### 13.7.1. Flyweight Pattern

동일한 셰이더를 여러 곳에서 사용해도 메모리는 1개만 사용:

```
Before (No Flyweight):
- RenderPass1: VS_PHONG (Compiled #1)
- RenderPass2: VS_PHONG (Compiled #2)  ❌ 중복!
- RenderPass3: VS_PHONG (Compiled #3)  ❌ 중복!

After (With Flyweight):
- RenderPass1: VS_PHONG ─┐
- RenderPass2: VS_PHONG ─┼─> Pool[Key] = VS_PHONG (1개만 존재)
- RenderPass3: VS_PHONG ─┘
```

#### 13.7.2. 시작 속도 개선

| 상황 | 소요 시간 |
|------|----------|
| Cold start (No cache) | ~500ms (D3DCompileFromFile) |
| Warm start (Cache hit) | ~50ms (File I/O only) |
| **10배 빠름!** | |

### 13.8. SOLID 원칙 준수

| 원칙 | 구현 |
|------|------|
| **SRP** | ShaderFactory(생성), ShaderManager(등록), ShaderPool(캐싱) |
| **OCP** | 새 셰이더 타입 추가 시 기존 코드 수정 불필요 |
| **LSP** | ShaderFactory는 선택사항, 기존 API도 그대로 동작 |
| **ISP** | Hot-reload 불필요 시 Register 단계 생략 가능 |
| **DIP** | 구체적 구현이 아닌 추상화(FShaderKey)에 의존 |

### 13.9. 주의사항

1. **포인터 수명**: ShaderManager에 등록한 포인터는 객체 생명 주기 동안 유효해야 함
   ```cpp
   // ❌ 잘못된 예: 지역 변수
   void BadExample() {
       ID3D11VertexShader* VS = nullptr;
       ShaderFactory::CreateVertexShader(...);
       ShaderManager::Get().RegisterVertexShader(..., &VS, ...);
   } // VS가 스택에서 사라짐 → 무효한 포인터!

   // ✅ 올바른 예: 멤버 변수
   class MyRenderPass {
       ID3D11VertexShader* VS = nullptr;  // 멤버 변수
       void Initialize() {
           ShaderManager::Get().RegisterVertexShader(..., &VS, ...);
       }
   };
   ```

2. **SafeRelease 필수**: Pool에서 받은 셰이더는 반드시 Release 필요
   ```cpp
   VS = ShaderFactory::CreateVertexShader(...); // RefCount++
   SafeRelease(VS);  // RefCount-- (필수!)
   ```

3. **순환 참조 방지**: Pool의 ComPtr + RenderPass의 raw pointer = 안전
   - Pool: ComPtr로 소유 (자동 Release)
   - RenderPass: Raw pointer로 참조 (수동 Release)

### 13.10. 최근 개선 사항 (v3)

- ✅ **Shader Folder Timestamp 추적**: `#include` 종속성 자동 감지
- ✅ **Compile Flags 검증**: 빌드 설정 변경 시 자동 재컴파일
- ✅ **Cache Version 3**: 폴더 기반 타임스탬프로 간소화 및 안정성 향상

### 13.11. 향후 개선 방향

- [ ] Async Shader Compilation (백그라운드 스레드)
- [ ] Shader Variant Precompilation (에디터 시작 시)
- [ ] Shader Graph 시스템 (노드 기반 셰이더 에디터)
- [ ] Fine-grained Dependency Tracking (파일별 `#include` 그래프)
- [ ] Shader Permutation Reduction (Uber shader 최적화)
