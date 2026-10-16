static const float PI = 3.14159265f;
static const uint kSamples = 16;

cbuffer SsaoCB : register(b0)
{
    float2 gTanHalfFov; // x = tan(fovY/2) * aspect, y = tan(fovY/2)
    float gRadius; // view-space units (m)
    float gBias;
    float gPower;
    float3 _pad;
};

Texture2D<float4> gNormalDepth : register(t0); // xyz = view normal, w = view depth (0 = sky)
Texture2D<float> gAoIn : register(t1); // blur input
RWTexture2D<float> gOut : register(u0);

float3 ViewPos(float2 uv, float z)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    return float3(ndc * gTanHalfFov * z, z);
}

float2 ToUV(float3 p)
{
    float2 ndc = p.xy / (p.z * gTanHalfFov);
    return float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
}

float Hash(uint n)
{
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / float(0x7fffffff);
}

// Cosine-weighted hemisphere direction (z = up), scaled so more samples sit near the center
float3 KernelSample(uint i)
{
    float u = Hash(i * 3u + 1u), v = Hash(i * 3u + 2u), r = Hash(i * 3u + 3u);
    float phi = 2.0f * PI * u;
    float sinT = sqrt(v), cosT = sqrt(1.0f - v);
    float3 d = float3(cos(phi) * sinT, sin(phi) * sinT, cosT);
    float t = (i + r) / kSamples;
    return d * lerp(0.1f, 1.0f, t * t);
}

// Orthonormal basis from a normal without branches (Duff et al. 2017)
void Basis(float3 n, out float3 b1, out float3 b2)
{
    float s = n.z >= 0.0f ? 1.0f : -1.0f;
    float a = -1.0f / (s + n.z);
    float b = n.x * n.y * a;
    b1 = float3(1.0f + s * n.x * n.x * a, s * b, -s * n.x);
    b2 = float3(b, s + n.y * n.y * a, -n.y);
}

[numthreads(8, 8, 1)]
void CSSsao(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    gOut.GetDimensions(w, h);
    if (id.x >= w || id.y >= h)
        return;

    float4 nd = gNormalDepth.Load(int3(id.xy, 0));
    if (nd.w <= 0.0f)
    {
        gOut[id.xy] = 1.0f;
        return;
    } // sky

    float2 size = float2(w, h);
    float3 P = ViewPos((id.xy + 0.5f) / size, nd.w);
    float3 N = normalize(nd.xyz);

    // Rotate the kernel per pixel with a 4x4 pattern (the 4x4 blur averages it out exactly)
    float angle = ((id.x & 3u) * 4u + (id.y & 3u)) * (2.0f * PI / 16.0f);
    float3 b1, b2;
    Basis(N, b1, b2);
    float3 T = cos(angle) * b1 + sin(angle) * b2;
    float3 B = cross(N, T);

    float occlusion = 0.0f;
    [unroll]
    for (uint i = 0; i < kSamples; ++i)
    {
        float3 k = KernelSample(i);
        float3 S = P + (T * k.x + B * k.y + N * k.z) * gRadius;
        if (S.z <= 0.01f)
            continue;

        float2 suv = ToUV(S);
        if (any(suv < 0.0f) || any(suv > 1.0f))
            continue;

        float sceneZ = gNormalDepth.Load(int3(int2(suv * size), 0)).w;
        if (sceneZ <= 0.0f)
            continue;

        // 그 픽셀에 실제로 보이는 표면의 뷰 공간 위치
        float3 Q = ViewPos(suv, sceneZ);
        float d = length(Q - P);

        // 반경 안: 그대로, 반경~2배: 서서히 감소, 그 밖: 무시 → 멀리 있는 앞 물체가 만드는 헤일로 제거
        float range = 1.0f - smoothstep(gRadius, 2.0f * gRadius, d);
        occlusion += (sceneZ <= S.z - gBias ? 1.0f : 0.0f) * range;
    }

    float ao = 1.0f - occlusion / kSamples;
    gOut[id.xy] = pow(saturate(ao), gPower);
}

[numthreads(8, 8, 1)]
void CSBlur(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    gOut.GetDimensions(w, h);
    if (id.x >= w || id.y >= h)
        return;

    float z0 = gNormalDepth.Load(int3(id.xy, 0)).w;
    if (z0 <= 0.0f)
    {
        gOut[id.xy] = 1.0f;
        return;
    }

    // 4x4 box (matches the noise tile) weighted by depth similarity -> no bleeding across edges
    float sum = 0.0f, wsum = 0.0f;
    [unroll]
    for (int y = -2; y < 2; ++y)
        [unroll]
        for (int x = -2; x < 2; ++x)
        {
            int2 p = clamp(int2(id.xy) + int2(x, y), int2(0, 0), int2(w, h) - 1);
            float z = gNormalDepth.Load(int3(p, 0)).w;
            float wgt = exp(-abs(z - z0) / (0.05f * z0 + 1e-3f));
            sum += gAoIn.Load(int3(p, 0)) * wgt;
            wsum += wgt;
        }
    gOut[id.xy] = sum / max(wsum, 1e-4f);
}