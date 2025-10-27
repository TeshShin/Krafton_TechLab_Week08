#include "../Common/CommonConstants.hlsli"

// We write VSM moments into an RG32F render target using LIGHT VIEW-SPACE depth (linear z).
// This improves stability vs. non-linear clip-space depth and must be matched at sampling time.

cbuffer LightViewConstants : register(b0)
{
	row_major float4x4 LightView;
	row_major float4x4 LightProjection; 
};

// VS Output
struct PS_INPUT
{
	float4 Position : SV_POSITION; // SV_Position.z is depth in [0,1]
	float4 LightPos : TEXCOORD0;   // not used for moments, kept for compatibility
}; 

float2 ComputeMomentsVSM(float depth01)
{
	float2 m;
	m.x = depth01;
    
	// Add small variance based on depth derivatives to reduce light leaking
	float dx = ddx(depth01);
	float dy = ddy(depth01);
	float d2 = dx * dx + dy * dy;
	// Clamp derivative energy to avoid exploding variance
	d2 = min(d2, 0.25);
	m.y = depth01 * depth01 + 0.25 * d2;

	return m; 
}

// Writes VSM moments: M1 = z, M2 = z^2 (z in light view space)
float4 mainPS(PS_INPUT Input) : SV_Target0
{
	// Use light view-space depth directly for VSM moments
	float depthVS = Input.LightPos.z;
	float2 m = ComputeMomentsVSM(depthVS);
	return float4(m.x, m.y, depthVS, 0);
	//return float4(depthVS, depthVS * depthVS , depthVS, 0);
}
