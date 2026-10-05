cbuffer PerFrame : register(b0)
{
    float4x4 gViewProj;
    float3 gCameraPos;      float _pad0;
    float3 gLightDir;       float _pad1; // direction the light travels
    float3 gLightColor;     float gAmbient;
};

cbuffer PerObject : register(b1)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    float4 gBaseColor;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    float4 wp = mul(float4(i.pos, 1.0f), gWorld);
    o.worldPos = wp.xyz;
    o.pos = mul(wp, gViewProj);
    o.normal = mul(i.normal, (float3x3) gWorldInvTranspose);
    o.color = pow(i.color * gBaseColor.rgb, 2.2f);          // sRGB -> linear

    
    return o;
}

float4 PSUnlit(VSOut i) : SV_Target
{
    return float4(i.color, 1.0f);
}

float4 PSLit(VSOut i) : SV_Target
{
    float3 N = normalize(i.normal);
    float3 L = normalize(-gLightDir);
    float3 V = normalize(gCameraPos - i.worldPos);
    float3 H = normalize(L + V);
    
    float ndl = saturate(dot(N, L));                                                        // NDL 법선 구조
    float spec = (ndl > 0.0f) ? pow(saturate(dot(N, H)), 64.0f) * 0.25f : 0.0f;             // 반대방향은 표현 x
    
    float3 c = i.color * (gAmbient + ndl * gLightColor) + spec * gLightColor;
    
    return float4(c, 1.0f);
}