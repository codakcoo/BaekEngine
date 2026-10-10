#include "IBLCommon.hlsli"

TextureCube gEnvCube : register(t0);

[numthreads(8,8,1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gSize || id.y >= gSize)
        return;
    
    // Heisphere around N
    float3 N = CubeDir(id);
    float3 up = abs(N.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 right = normalize(cross(up, N));
    up = cross(N, right);

    
    float3 sum = 0.0f;
    float count = 0.0f;
    const float delta = 0.025f;
    
    [loop]
    for (float phi = 0.0f; phi < 2.0f * PI; phi += delta)
    {
        [loop]
        for (float theta = 0.0f; theta < 0.5f * PI; theta += delta)
        {
            float3 t = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));            // tangent space
            float3 s = t.x * right + t.y * up + t.z * N;                                            // world space
            
            sum += gEnvCube.SampleLevel(gSampler, s, 0).rgb * cos(theta) * sin(theta);
            count += 1.0f;
        }
    }
    gOut[id] = float4(PI * sum / count, 1.0f);
}