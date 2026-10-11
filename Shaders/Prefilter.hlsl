#include "IBLCommon.hlsli"
#include "IBLSampling.hlsli"

TextureCube gEnvCube : register(t0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if(id.x >= gSize || id.y >= gSize)
        return;
    
    float3 N = CubeDir(id);
    
    if(gRoughness <= 0.0f)          // mirror: no blur needed
    {
        gOut[id] = float4(gEnvCube.SampleLevel(gSampler, N, 0).rgb, 1.0f);
        return;
    }
    
    // Assume View direction = reflection direction = N (split-sum approximation)
    float3 V = N;
    const uint SAMPLES = 1024;
    
    float3 sum = 0.0f;
    float weight = 0.0f;
    [loop]
    for (uint i = 0; i < SAMPLES; ++i)
    {
        float3 H = ImportanceSampleGGX(Hammersley(i, SAMPLES), N, gRoughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);
        
        float NdotL = dot(N, L);
        if(NdotL > 0.0f)
        {
            sum += gEnvCube.SampleLevel(gSampler, L, 0).rgb * NdotL;
            weight += NdotL;
        }
    }
    
    gOut[id] = float4(sum / max(weight, 1e-4f), 1.0f);
}