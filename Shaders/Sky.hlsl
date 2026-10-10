cbuffer SkyCB : register(b0)
{
    float4x4    gInvViewProj;
    float       gSkyIntensity; // inverse (view without translation * proj)
    float3      _pad;
};

Texture2D       gEnvMap : register(t0);
SamplerState    gSampler : register(s0);

static const float PI = 3.14159265f;

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD;
};

// Fullscreen triangle placed exactly const the far plane (depth = 1)
VSOut VSMain(uint id : SV_VertexID)
{
    float2 uv = float2((id << 1) & 2, id & 2);
    VSOut o;
    o.ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    o.pos = float4(o.ndc, 1.0f, 1.0f);
    
    return o;
}

// World direction -> equirectangular UV
float2 DirToEquirect(float3 d)
{
    float u = 0.5f - atan2(d.z, d.x) / (2.0f * PI);
    float v = acos(clamp(d.y, -1.0f, 1.0f)) /  PI;      // 0 = straight up, 1= straight down
    
    return float2(u, v);
}

float4 PSMain(VSOut i) : SV_Target
{
    float4 world = mul(float4(i.ndc, 1.0, 1.0f), gInvViewProj);
    float3 dir = normalize(world.xyz / world.w);
    
    float3 c = gEnvMap.SampleLevel(gSampler, DirToEquirect(dir), 0).rgb * gSkyIntensity;
    
    return float4(c, 1.0f);
}