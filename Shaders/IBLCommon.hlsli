#ifndef IBL_COMMON_HLSLI
#define IBL_COMMON_HLSLI

cbuffer Params : register(b0)
{
    uint gSize;                                 // output face size in texels
    uint3 _pad;
};

SamplerState                gSampler : register(s0);
RWTexture2DArray<float4>    gOut : register(u0);           // 6 slices = À°¸éÃ¼

static const float PI = 3.14159265f;

// Cube face texel -> world direction (D3D cube mapconvention: +X, -X, +Y, -Y, +Z, -Z)
float3 CubeDir(uint3 id)
{
    float2 uv = ((float2(id.xy) + 0.5f) / gSize) * 2.0f - 1.0f;             // -1 .. 1, v grows downward
    float3 d;
    switch (id.z)
    {
    case 0:
        d = float3( 1.0f, -uv.y, -uv.x);
        break;
    case 1:
        d = float3(-1.0f, -uv.y,  uv.x);
        break;
    case 2:
        d = float3( uv.x,  1.0f,  uv.y);
        break;
    case 3:
        d = float3( uv.x, -1.0f,  -uv.y);
        break;
    case 4:
        d = float3( uv.x, -uv.y,  1.0f);
        break;
    default:
        d = float3(-uv.x, -uv.y, -1.0f);
        break;
    
    }
    return normalize(d);
}

#endif