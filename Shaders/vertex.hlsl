cbuffer cbPassConstants : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gViewProj;
};

cbuffer cbPerObject : register(b1)
{
    float4x4 gWorld;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
	
    float4x4 worldViewProj = mul(gWorld, gViewProj);
    
	// Transform to homogeneous clip space.
    vout.PosH = mul(float4(vin.PosL, 1.0f), worldViewProj);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    
    return vout;
}