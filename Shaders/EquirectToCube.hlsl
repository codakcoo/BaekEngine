#include "IBLCommon.hlsli"

Texture2D gEquirect : register(t0);

float2 DirToEquirect(float3 d)
{
    float u = 0.5f - atan2(d.z, d.x) / (2.0f * PI);
    float v = acos(clamp(d.y, -1.0f, 1.0f)) / PI;
    return float2(u, v);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gSize || id.y >= gSize)
        return;
    
    float3 dir = CubeDir(id);
    gOut[id] = float4(gEquirect.SampleLevel(gSampler, DirToEquirect(dir), 0).rgb, 1.0f);
}