static const float PI = 3.14159265f;
static const float kPrefilterMaxLod = 4.0f;         // = prefilterMips - 1 (c++ 쪽과 맞출 것)

Texture2D               gAlbedoMap      : register(t0);
Texture2D               gNormalMap      : register(t1);
Texture2D               gMRMap          : register(t2);
Texture2D               gEmissiveMap    : register(t3);
TextureCube             gIrradianceMap  : register(t4);
TextureCube             gPrefilterMap   : register(t5);
Texture2D               gBrdfLut        : register(t6);
Texture2D               gShadowMap      : register(t7);
SamplerState            gSampler        : register(s0);
SamplerState            gClampSampler   : register(s1);
SamplerComparisonState  gShadowSampler : register(s2);

cbuffer PerFrame : register(b0)
{
    float4x4 gViewProj;
    float3 gCameraPos;      float _pad0;
    float3 gLightDir;       float _pad1; // direction the light travels
    float3 gLightColor;     float gAmbient;
    float4x4 gLightViewProj;
    float4 gShadowParams;       // x = bias, y = texel size, z = normal offset
};

cbuffer PerObject : register(b1)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    float4 gBaseColor;
    float4 gMaterial;           // x = metalllic, y = roughness
    float4 gEmissive;           // rgb = emissive factor
};

struct VSIn
{
    float3 pos      : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv       : TEXCOORD;
    float3 color    : COLOR;
};

struct VSOut
{
    float4 pos      : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
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
    o.tangent = float4(mul(i.tangent.xyz, (float3x3) gWorld), i.tangent.w);
    
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

// Fresnel for ambient light: rough surfaces reflect less at grazing angles
float3 F_SchilckRoughness(float NdotV, float3 F0, float roughness)
{
    return F0 + (max((1.0f - roughness).xxx, F0) - F0) * pow(saturate(1.0f - NdotV), 5.0f);
}

// 1 = lit, 0 = fully in shadow
float ShadowFactor(float3 worldPos, float3 Ng)
{
    // Push the lookup position along the surface normal to avoid self-shadowing
    float3 p = worldPos + Ng * gShadowParams.z;
    float4 lp = mul(float4(p, 1.0f), gLightViewProj);
    float3 ndc = lp.xyz / lp.w;
    
    float2 uv = ndc.xy * float2(0.5f, -0.5f) + 0.5f;            // NDC -> texture UV (Y flipped)
    if(any(uv < 0.0f) || any(uv > 1.0f) || ndc.z > 1.0f)
        return 1.0f;                                            // outside the shadow box
    
    float depth = ndc.z - gShadowParams.x;
    float texel = gShadowParams.y;
    
    // 3x3 PCF: average 9 hardware-filtered comparisions -> soft edges
    float sum = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            sum += gShadowMap.SampleCmpLevelZero(gShadowSampler, uv + float2(x, y) * texel, depth);
        }

    }
    return sum / 9.0f;
}

float4 PSLit(VSOut i) : SV_Target
{
    float3 albedo = i.color * gAlbedoMap.Sample(gSampler, i.uv).rgb;
    
    float3 mr = gMRMap.Sample(gSampler, i.uv).rgb;
    float metallic = saturate(gMaterial.x * mr.b);
    float roughness = clamp(gMaterial.y * mr.g, 0.045f, 1.0f);
    
    // Tangent space -> world space
    float3 N = normalize(i.normal);
    float3 T = normalize(i.tangent.xyz - N * dot(N, i.tangent.xyz));        // re-orthogonalize
    float3 B = cross(N, T) * i.tangent.w;
    
    float3 Ng = normalize(i.normal);                                        // 노멀 맵 적용 전, 지오메트리 노멀 (맨 앞에서 저장)
    
    float3 nTS = gNormalMap.Sample(gSampler, i.uv).xyz * 2.0f - 1.0f;       // [0,1] -> [-1,1]
    N = normalize(nTS.x * T + nTS.y * B + nTS.z * N);
    
    float3 V = normalize(gCameraPos - i.worldPos);
    float3 L = normalize(-gLightDir);
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
    
    float shadow = ShadowFactor(i.worldPos, Ng);
    float3 direct = (diffuse + specualr) * gLightColor * NdotL * shadow;            // 직접광에만 적용
    
    // Image-based lighting
    float3 ks_amb = F_SchilckRoughness(NdotV, F0, roughness);
    float3 kD_amb = (1.0f - ks_amb) * (1.0f - metallic);
    
    // Diffuse: irradiance around N
    float3 irradiance = gIrradianceMap.SampleLevel(gSampler, N, 0).rgb;
    float3 diffuseIBL = kD_amb * irradiance * albedo;
    
    // Specular: blurred enviornment along the reflection, scaled by the BRDF LUT
    float3 R = reflect(-V, N);
    float3 prefiltered = gPrefilterMap.SampleLevel(gSampler, R, roughness * kPrefilterMaxLod).rgb;
    float2 brdf = gBrdfLut.SampleLevel(gClampSampler, float2(NdotV, roughness), 0).rg;
    float3 specularIBL = prefiltered * (ks_amb * brdf.x + brdf.y);
    
    float3 ambient = (diffuseIBL + specularIBL) * gAmbient;
    float3 emissive = gEmissive.rgb * gEmissiveMap.Sample(gSampler, i.uv).rgb;
    
    return float4(direct + ambient + emissive, 1.0f);
}