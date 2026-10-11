#include "IBLSampling.hlsli"

cbuffer Params : register(b0)
{
    uint gSize;
    float gUnused;
    uint2 _pad;
};

RWTexture2D<float2> gLut : register(u0);

float G1(float NdotX, float roughness)
{
    float k = (roughness * roughness) / 2.0f;               // IBL uses a diffrent k than direct light
    return NdotX / (NdotX * (1.0f - k) + k);
}

float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 V = float3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);
    float3 N = float3(0, 0, 1);
    const uint SAMPLES = 1024;
    
    float A = 0.0, B = 0.0f;
    [loop]
    for (uint i = 0; i < SAMPLES; ++i)
    {
        float3 H = ImportanceSampleGGX(Hammersley(i, SAMPLES), N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);
        
        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));
        
        if(NdotL > 0.0f)
        {
            float G = G1(NdotV, roughness) * G1(NdotL, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 1e-4f);
            float Fc = pow(1.0f - VdotH, 5.0f);
            A += (1.0f - Fc) * G_Vis;           // scale on F0
            B += Fc * G_Vis;                    // bias
        }
    }
    return float2(A, B) / SAMPLES;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gSize || id.y >= gSize)
        return;
    
    float2 uv = (float2(id.xy) + 0.5f) / gSize;         // x = NdotV, y = roughness
    gLut[id.xy] = IntegrateBRDF(max(uv.x, 1e-3f), uv.y);
}