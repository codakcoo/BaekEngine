Texture2D       gScene : register(t0);
SamplerState    gSampler : register(s0);

cbuffer Params : register(b0)
{
    float gExposure;
    float3 _pad;
};

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

// Fullscreen triangle: noise vertex buffer, 3 vertices from SV_VertexID
VSOut VSMain(uint id : SV_VertexID)
{
    VSOut o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return o;
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSMain(VSOut i) : SV_Target
{
    float3 hdr = gScene.Sample(gSampler, i.uv).rgb * gExposure;
    float3 ldr = ACESFilm(hdr);
    return float4(pow(ldr, 1.0f / 2.2f), 1.0f);         // linear -> displat gamma
}