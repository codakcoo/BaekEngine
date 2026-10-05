static const float PI = 3.14159265f;

Texture2D       gAlbedoMap : register(t0);
SamplerState    gSampler : register(s0);

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
    float4 gMaterial;           // x = metalllic, y = roughness
};

struct VSIn
{
    float3 pos      : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
    float3 color    : COLOR;
};

struct VSOut
{
    float4 pos      : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
    float3 color    : COLOR;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    float4 wp = mul(float4(i.pos, 1.0f), gWorld);
    o.worldPos = wp.xyz;
    o.pos = mul(wp, gViewProj);
    o.normal = mul(i.normal, (float3x3) gWorldInvTranspose);
    o.color = pow(i.color * gBaseColor.rgb, 2.2f);          // sRGB -> linear
    o.uv = i.uv;
    
    return o;
}

float4 PSUnlit(VSOut i) : SV_Target
{
    return float4(i.color, 1.0f);
}

// D: how many microfacets face the half vector (GGX / Trowbridge-Reitz)
float D_GGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d);
}

// G: microfacet self-shadowing (Smith, Schlick-GGX)
float G_SchlickGGX(float NdotX, float k)
{
    return NdotX / (NdotX * (1.0f - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}

// F: reflectance grows toward grzing angles (Schlick)
float3 F_Schlick(float VdotH, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - VdotH), 5.0f);
}

float4 PSLit(VSOut i) : SV_Target
{
    float3 albedo = i.color * gAlbedoMap.Sample(gSampler, i.uv).rgb;
    float metallic = saturate(gMaterial.x);
    float roughness = clamp(gMaterial.y, 0.045f, 1.0f);
    
    float3 N = normalize(i.normal);
    float3 L = normalize(-gLightDir);
    float3 V = normalize(gCameraPos - i.worldPos);
    float3 H = normalize(L + V);
    
    float NdotL = saturate(dot(N, L));
    float NdotV = max(dot(N, V), 1e-4f);
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));
    
    // Dielectrics reflect ~4%, metals reflect their albedo color
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    
    float D = D_GGX(NdotH, roughness);
    float G = G_Smith(NdotV, NdotL, roughness);
    float F = F_Schlick(VdotH, F0);
    
    float3 specualr = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-4f);
    float3 kD = (1.0f - F) * (1.0f - metallic);         // metal have no diffuse
    float3 diffuse = kD * albedo / PI;
    
    float3 direct = (diffuse + specualr) * gLightColor * NdotL;
    float3 ambient = gAmbient * albedo;                 // placeholder until IBL
    
    return float4(direct + ambient, 1.0f);
}