#include "../Material/TextureVS.hlsl"

// VSM 토글 TODO:UI 연동
#ifndef USE_VSM
#define USE_VSM 0
#endif

//--------------------------------------------------------------------------------------
// [FORWARD PLUS RENDERING] Light Tile Clustering Data Structures
//--------------------------------------------------------------------------------------

// Camera and tiling parameters
cbuffer FP_CameraCB : register(b2)
{
	row_major float4x4 FP_View;
	row_major float4x4 FP_Projection;
	row_major float4x4 FP_InvProj;
	uint2   FP_ScreenSize;     // pixels (width, height)
	uint2   FP_ViewportOrigin; // pixels (top-left x,y)
	uint    FP_NumTilesX;      // dispatch dim X
	uint    FP_NumTilesY;      // dispatch dim Y
	uint    FP_NumZSlices;     // dispatch dim Z
	float   FP_NearZ;          // view-space near (>= 0)
	float   FP_FarZ;           // view-space far  (>  NearZ)
}

// Forward+ control parameters
cbuffer FP_ForwardPlusCB : register(b3)
{
	uint FP_NumLights;                // number of entries in DynamicLights
	uint FP_MaxLightsPerCluster;      // capacity per cluster
	uint FP_TotalClusters;            // NumTilesX*NumTilesY*NumZSlices
	uint FP_Pad0;
};

// Cluster lists produced by the compute shader
StructuredBuffer<uint> FP_ClusterCount : register(t10);  // count per cluster
StructuredBuffer<uint> FP_ClusterIndex : register(t11);  // flat indices array (clusterID*Max + i)

Texture2D DiffuseTexture : register(t0);	// map_Kd
Texture2D AmbientTexture : register(t1);	// map_Ka
Texture2D SpecularTexture : register(t2);   // map_Ks
Texture2D ShininessTexture : register(t3);   // map_Ns
Texture2D AlphaTexture : register(t4);		// map_d
Texture2D BumpTexture : register(t5);		// map_bump
SamplerState SamplerWrap : register(s0);

// Shadow
cbuffer DirectionalLightConstants : register(b4)
{
	FLightViewProj DirectionalShadowMatrix;
};

StructuredBuffer<FLightViewProj> SpotLightShadowMatrices : register(t12);
Texture2DArray<float> SpotShadowAtlas : register(t13);
Texture2DArray<float2> SpotMomentsAtlas : register(t14);
TextureCubeArray<float> PointShadowAtlas : register(t15);
TextureCubeArray<float2> PointMomentsAtlas : register(t16);
Texture2D<float> DirectionalTexture : register(t17);
Texture2D<float2> DirectionalMoment : register(t18);

SamplerComparisonState ShadowSampler : register(s1);
SamplerState ShadowLinearSampler : register(s2);


// Material flags
#define HAS_DIFFUSE_MAP	 (1 << 0)
#define HAS_AMBIENT_MAP	 (1 << 1)
#define HAS_SPECULAR_MAP (1 << 2)
#define HAS_SHININESS_MAP (1 << 3)
#define HAS_ALPHA_MAP	 (1 << 4)
#define HAS_BUMP_MAP	 (1 << 5)

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
uint FP_ComputeClusterID(float4 svpos /* SV_POSITION */, float3 worldPos)
{
    // Tile X/Y from pixel coords relative to viewport origin
    float2 pix = svpos.xy - float2(FP_ViewportOrigin);
    int tileX = clamp(int(floor(pix.x * (float)FP_NumTilesX / max(FP_ScreenSize.x, 1))), 0, int(FP_NumTilesX - 1));
    int tileY = clamp(int(floor(pix.y * (float)FP_NumTilesY / max(FP_ScreenSize.y, 1))), 0, int(FP_NumTilesY - 1));

    // LH view space depth (camera looks +Z)
    float3 posVS = mul(float4(worldPos, 1.0f), FP_View).xyz;
    float depthVS = posVS.z;

    // Detect orthographic vs perspective to match the compute shader partitioning
    // D3D-style: perspective has FP_Projection[2][3] ~= 1 and FP_Projection[3][3] ~= 0;
    //            orthographic has FP_Projection[2][3] ~= 0 and FP_Projection[3][3] ~= 1
    bool isOrtho = (abs(FP_Projection[2][3]) < 1e-6f) && (abs(FP_Projection[3][3] - 1.0f) < 1e-6f);

    // Match CS z-slicing: logarithmic for perspective, linear for orthographic
    int zSlice;
    if (!isOrtho)
    {
        // Clamp depth to [NearZ, FarZ]
        float depthClamped = clamp(depthVS, FP_NearZ + 1e-6f, FP_FarZ - 1e-6f);
        float logDen = log(FP_FarZ / FP_NearZ);
        float sliceF = (log(depthClamped / FP_NearZ) / max(logDen, 1e-6f)) * FP_NumZSlices;
        zSlice = clamp(int(floor(sliceF)), 0, int(FP_NumZSlices - 1));
    }
    else
    {
        // Linear partitioning across [NearZ, FarZ]
        float depthClamped = clamp(depthVS, FP_NearZ, FP_FarZ);
        float t = (depthClamped - FP_NearZ) / max(FP_FarZ - FP_NearZ, 1e-6f);
        float sliceF = t * FP_NumZSlices;
        zSlice = clamp(int(floor(sliceF)), 0, int(FP_NumZSlices - 1));
    }

	return (zSlice * FP_NumTilesY + tileY) * FP_NumTilesX + tileX;
}

PS_OUTPUT mainPS(PS_INPUT Input)
{
    PS_OUTPUT Output;

	float2 UV = Input.Tex;

    // Base diffuse color
	float4 DiffuseColor = Kd;
	if (MaterialFlags & HAS_DIFFUSE_MAP)
	{
		DiffuseColor *= DiffuseTexture.Sample(SamplerWrap, UV);
	}

    // Ambient color for material
	float4 AmbientColor = Ka;
	if (MaterialFlags & HAS_AMBIENT_MAP)
	{
		AmbientColor *= AmbientTexture.Sample(SamplerWrap, UV);
	}
	else if (MaterialFlags & HAS_DIFFUSE_MAP)
	{
		AmbientColor *= DiffuseTexture.Sample(SamplerWrap, UV);
	}

    // Specular color for material
	float4 SpecularColor = Ks;
	if (MaterialFlags & HAS_SPECULAR_MAP)
	{
		SpecularColor *= SpecularTexture.Sample(SamplerWrap, UV);
	}

	float4 FinalColor = float4(0, 0, 0, 1);

	// Accumulate separated diffuse and specular contributions
	float3 TotalAmbient = float3(0, 0, 0);
	float3 TotalDiffuse = float3(0, 0, 0);
	float3 TotalSpecular = float3(0, 0, 0);

	// #define LIGHTING_MODEL_GOURAUD
#if defined(LIGHTING_MODEL_GOURAUD)
	float3 wsNormal = Input.WorldNormal;

	TotalAmbient = Input.TotalAmbient;
	TotalDiffuse = Input.TotalDiffuse;
	TotalSpecular = Input.TotalSpecular;
#else
	// Normal mapping
    // -----------------------
	float3 wsNormal = Input.WorldNormal;
	if (MaterialFlags & HAS_BUMP_MAP)
	{
        // Sample and unpack tangent-space normal (assumes XYZ in texture)
		float3 nTS = BumpTexture.Sample(SamplerWrap, UV).xyz * 2.0f - 1.0f;
		nTS = normalize(nTS);

		float3 N = normalize(Input.WorldNormal);
		float3 T = normalize(Input.WorldTangent);
        // Recompute B using handedness (stored in TangentSign)
		float3 B = normalize(cross(N, T)) * Input.TangentSign;

		float3x3 TBN = float3x3(T, B, N);
		wsNormal = normalize(mul(nTS, TBN));
	}
	else
	{
		wsNormal = normalize(Input.WorldNormal);
	}

    float3 ViewDir = normalize(ViewWorldLocation - Input.WorldPosition);
    float SpecularPower = max(Ns, 1.0f); // Prevent division by zero

    uint cid   = FP_ComputeClusterID(Input.Position, Input.WorldPosition);
    uint count = FP_ClusterCount[cid];
    // Clamp to avoid reading past FP_ClusterIndex allocation when clusters overflow
    uint maxCount = FP_MaxLightsPerCluster;
    uint safeCount = (count < maxCount) ? count : maxCount;
    uint base  = cid * FP_MaxLightsPerCluster;

[loop]
    for (uint i = 0; i < safeCount; ++i)
    {
       uint li = FP_ClusterIndex[base + i];

       FLightingResult LightResult;

//--- 1. VSM ----------------------------------------------
#if USE_VSM
       // VSM은 <float2> 텍스처와 '일반' 샘플러(SamplerState)를 사용합니다.
       // (참고: CalculateDynamicLightWithVSM의 마지막 인자(ViewSpaceDepth)는
       // 내부에서 사용되지 않으므로 더미 값 0.0f를 전달합니다.)
       LightResult = CalculateDynamicLightWithVSM(
           DynamicLights[li], Input.WorldPosition, wsNormal, ViewDir, max(Ns, 1.0f),
           SpotMomentsAtlas,        // t14 <float2>
           SpotLightShadowMatrices, // t12
           PointMomentsAtlas,       // t16 <float2>
           DirectionalMoment,       // t18 <float2>
           DirectionalShadowMatrix, // (CBuffer에서 온다고 가정)
           ShadowLinearSampler,     // s2 (일반 샘플러)
           0.0f);                   // (Dummy 값)

//--- 2. PCF ---------------------------------------------
// #elif USE_PCF
//        // PCF는 <float> 텍스처와 '비교' 샘플러(SamplerComparisonState)를 사용합니다.
//        // (필터 크기는 예시로 11을 사용. CBuffer의 상수로 대체 가능)
//        LightResult = CalculateDynamicLightWithPCF(
//            DynamicLights[li], Input.WorldPosition, wsNormal, ViewDir, max(Ns, 1.0f),
//            SpotShadowAtlas,         // t13 <float>
//            SpotLightShadowMatrices, // t12
//            PointShadowAtlas,        // t15 <float>
//            DirectionalTexture,      // t17 <float>
//            DirectionalShadowMatrix, // (CBuffer)
//            ShadowSampler,           // s1 (비교 샘플러)
//            11);                     // (PCF 필터 크기)

//--- 3. Hard Shadow ---------------------------------------------
#else
       // 하드 섀도우는 PCF와 동일한 리소스를 사용하며, FilterSize=1인
       // CalculateDynamicLightWithShadows 함수를 호출합니다.
       LightResult = CalculateDynamicLightWithShadows(
          DynamicLights[li], Input.WorldPosition, wsNormal, ViewDir, max(Ns, 1.0f),
          SpotShadowAtlas,         // t13 <float>
          SpotLightShadowMatrices, // t12
          PointShadowAtlas,        // t15 <float>
          DirectionalTexture,      // t17 <float>
          DirectionalShadowMatrix, // (CBuffer)
          ShadowSampler);          // s1 (비교 샘플러)
#endif
//-------------------------------------------------------------------------

       TotalDiffuse  += LightResult.Diffuse;
       TotalSpecular += LightResult.Specular;
       TotalAmbient  += LightResult.Ambient;
    }
#endif

	// [PHYSICALLY CORRECT] Apply material properties separately
    // Ambient term: Ka * GlobalAmbient
	FinalColor.rgb = AmbientColor.rgb * TotalAmbient;

    // Diffuse term: Kd * Diffuse lighting
	FinalColor.rgb += DiffuseColor.rgb * TotalDiffuse;

    // Specular term: Ks * Specular lighting
	FinalColor.rgb += SpecularColor.rgb * TotalSpecular;

	Output.SceneColor = FinalColor;

	// 알파 값 처리 (기존 코드와 동일)
	FinalColor.a = D; // 기본 알파값
	if (MaterialFlags & HAS_ALPHA_MAP)
	{
		float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
		FinalColor.a = D * alpha;
	}

    float3 EncodedNormal = wsNormal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
}
