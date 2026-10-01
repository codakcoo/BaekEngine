cbuffer PerDraw : register(b0)
{
    float4x4 gMVP;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 color : COLOR;
};
struct VSOut
{
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0f), gMVP);
    o.color = i.color;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    return float4(i.color, 1.0f);
}
