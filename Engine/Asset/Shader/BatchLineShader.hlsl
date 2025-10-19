cbuffer PerFrame : register(b1)
{
	row_major float4x4 View; // View Matrix Calculation of MVP Matrix
	row_major float4x4 Projection; // Projection Matrix Calculation of MVP Matrix
};

struct VS_INPUT
{
	float4 position : POSITION; // Input position from vertex buffer
	float4 color : COLOR;
};

struct PS_INPUT
{
	float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
	float4 color : COLOR;
};

PS_INPUT mainVS(VS_INPUT input)
{
	PS_INPUT output;
	float4 tmp = input.position;
	tmp = mul(tmp, View);
	tmp = mul(tmp, Projection);
	
	output.position = tmp;
	output.color = input.color;

	return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
	return input.color;
}