//--------------------------------------------------------------------------------------
// 광원 데이터 구조체 정의
//--------------------------------------------------------------------------------------
#define MAX_DIRECTIONAL_LIGHTS 16

// Ambient Light (씬 전체에 적용되는 단일 값)
struct FAmbientLight
{
    float3 Color;
    float Intensity;
};

// Directional Light
struct FDirectionalLight
{
    float3 Color; float Pad0;
    float3 Direction; float Pad1;
    float Intensity; float3 Pad2;
};

//--------------------------------------------------------------------------------------
// C++에서 채워줄 광원 데이터 상수 버퍼
//--------------------------------------------------------------------------------------
cbuffer LightConstants : register(b10) // 기존 레지스터와 겹치지 않게 b10으로 지정
{
    FAmbientLight GlobalAmbient;
    FDirectionalLight DirectionalLights[MAX_DIRECTIONAL_LIGHTS];
    int NumDirectionalLights; float3 Pad0;
};

float3 CaculateBlinnPhong(float3 ToLightDir, float3 ToEyeDir, float3 LightColor, float LightIntensity, float3 Normal, float4 DiffuseColor, float4 SpecularColor, float Shiniess)
{
	float3 halfVector = normalize(ToLightDir + ToEyeDir);

	float3 DiffuseTerm = DiffuseColor.rgb * saturate(dot(normalize(Normal), normalize(ToLightDir)));
	float3 SpecularTerm = SpecularColor.rgb * pow(saturate(dot(normalize(Normal), halfVector)), Shiniess);

	float3 FinalColor = (DiffuseTerm + SpecularTerm) * LightColor * LightIntensity;

	return FinalColor;
}

float3 CalculateLighting(float4 MaterialAmbient, float4 MaterialDiffuse, float4 MaterialSpecular, float MaterialShininess,
    float3 WorldPosition, float3 WorldNormal, float3 CameraPosition)
{
    // Ambient
	float3 FinalAmbient = GlobalAmbient.Color * GlobalAmbient.Intensity;
	float3 FinalColor = MaterialAmbient.rgb * FinalAmbient;

    // Directional Light
	for (int i = 0; i < NumDirectionalLights; ++i)
	{
		FDirectionalLight Light = DirectionalLights[i];
        
		float3 L = normalize(-Light.Direction); // 광원으로 향하는 벡터
		float3 V = normalize(CameraPosition - WorldPosition); // 카메라로 향하는 벡터

		FinalColor += CaculateBlinnPhong(L, V, Light.Color, Light.Intensity, WorldNormal, MaterialDiffuse, MaterialSpecular, MaterialShininess);
	}

	return FinalColor;
}

//--------------------------------------------------------------------------------------
// 조명 계산 메인 함수
//--------------------------------------------------------------------------------------
//float3 CalculateLighting(float4 MaterialAmbient, float4 MaterialDiffuse, float4 MaterialSpecular, float MaterialShininess,
//    float3 WorldPosition, float3 WorldNormal, float3 CameraPosition)
//{
//    // Ambient
//    float3 FinalAmbient = GlobalAmbient.Color * GlobalAmbient.Intensity;
//    float3 FinalColor = MaterialAmbient.rgb * FinalAmbient;

//    // Directional Light
//    for (int i = 0; i < NumDirectionalLights; ++i)
//    {
//        FDirectionalLight Light = DirectionalLights[i];
        
//        float3 L = normalize(-Light.Direction); // 광원으로 향하는 벡터
//        float3 V = normalize(CameraPosition - WorldPosition); // 카메라로 향하는 벡터
//        float3 H = normalize(L + V); // 블린-퐁(Blinn-Phong)을 위한 하프 벡터

//        // Diffuse
//        float NdotL = saturate(dot(WorldNormal, L));
//        float3 Diffuse = MaterialDiffuse.rgb * Light.Color.rgb * Light.Intensity * NdotL;

//        // Specular
//        float NdotH = saturate(dot(WorldNormal, H));
//        float SpecFactor = pow(NdotH, MaterialShininess);
//        float3 Specular = MaterialSpecular.rgb * Light.Color.rgb * Light.Intensity * SpecFactor;

//        // Ambient + Diffuse + Specular
//        FinalColor += Diffuse + Specular;
//    }

//    return FinalColor;
//}
