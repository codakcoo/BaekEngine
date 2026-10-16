cbuffer BloomCB : register(b0)
{
	float2 gSrvTexel;				// 1 / sourse size
	float gThreshold;
	float gKnee;
	float gRadius;					// upsample tent radiusu (source texels)
	float3 _pad;
};

Texture2D			gSrc : register(t0);						// prefiilter: scene, down: previous mip, up: smaller level
Texture2D			gHigh : register(t1);						// up: same-size down mip
RWTexture2D<float4> gOut : register(u0);
SamplerState		gLinear : register(s0);

static const float kBoxWeight[5] = { 0.5f, 0.125f, 0.125f, 0.125f, 0.125f };

float Luma(float3 c)
{
	return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 Tap(float2 uv, float2 offset)
{
    float3 c = gSrc.SampleLevel(gLinear, uv + offset * gSrvTexel, 0).rgb;
    return min(max(c, 0.0f), 65000.0f); // NaN -> 0, inf -> 65000, 음수 -> 0
}

// 13-tap downsample = 5 overlapping 2x2 boxes (center 0.5, corners 0.125 each)
void Boxes13(float2 uv, out float3 boxes[5])
{
	float3 a = Tap(uv, float2(-2, -2)), b = Tap(uv, float2(0, -2)), c = Tap(uv, float2(2, -2));
	float3 d = Tap(uv, float2(-1, -1)), e = Tap(uv, float2(1, -1));
	float3 f = Tap(uv, float2(-2,  0)), g = Tap(uv, float2(0,  0)), h = Tap(uv, float2(2, 0));
	float3 i = Tap(uv, float2(-1,  1)), j = Tap(uv, float2(1,  1));
	float3 k = Tap(uv, float2(-2,  2)), l = Tap(uv, float2(0,  2)), m = Tap(uv, float2(2,  2));

	boxes[0] = (d + e + i + j) * 0.25f;
	boxes[1] = (a + b + f + g) * 0.25f;
	boxes[2] = (b + c + g + h) * 0.25f;
	boxes[3] = (f + g + k + l) * 0.25f;
	boxes[4] = (g + h + l + m) * 0.25f;
}

// Soft threshold: smooth ramp of width 2*knee around the threshold
float3 Threshold(float3 c)
{
	float br = max(c.r, max(c.g, c.b));
	float soft = clamp(br - gThreshold + gKnee, 0.0f, 2.0f * gKnee);
	soft = soft * soft / (4.0f * gKnee + 1e-5f);
	float contrib = max(soft, br - gThreshold) / max(br, 1e-5f);
	return c * contrib;
}

bool OutofBounds(uint2 id, out float2 uv)
{
	uint w, h;
	gOut.GetDimensions(w, h);
	uv = (id + 0.5f) / float2(w, h);
	return id.x >= w || id.y >= h;
}

[numthreads(8, 8, 1)]
void CSPrefilter(uint3 id : SV_DispatchThreadID)
{
	float2 uv;
	if (OutofBounds(id.xy, uv))
		return;
	
	// Karis average: weight each box by 1/(1+luma) so single bright pixels don't filcker
	float3 boxes[5];
	Boxes13(uv, boxes);
	float3 sum = 0.0f;
	float wsum = 0.0f;
	[unroll]
	for (int k = 0; k < 5; ++k)
	{
		float w = kBoxWeight[k] / (1.0f + Luma(boxes[k]));
		sum += boxes[k] * w;
		wsum += w;
	}
	float3 c = max(sum / wsum, 0.0f);				// also kills NaN / negattive
	gOut[id.xy] = float4(Threshold(c), 1.0f);
}

[numthreads(8, 8, 1)]
void CSDownsample(uint3 id : SV_DispatchThreadID)
{
	float2 uv;
	if (OutofBounds(id.xy, uv))
		return;
	
	float3 boxes[5];
	Boxes13(uv, boxes);
	float3 c = 0.0f;
	[unroll]
	for (int k = 0; k < 5; ++k)
	{
		c += boxes[k] * kBoxWeight[k];
	}
	gOut[id.xy] = float4(c, 1.0f);

}

[numthreads(8, 8, 1)]
void CSUpsample(uint3 id : SV_DispatchThreadID)
{
	float2 uv;
	if (OutofBounds(id.xy, uv))
		return;
	
	// 3x3 tent (1 2 1 / 2 4 2 / 1 2 1) / 16 on the smaller level
	float r = gRadius;
	float3 s = Tap(uv, float2(-r, -r))			+ Tap(uv, float2(0, -r)) * 2.0f		+ Tap(uv, float2(r, -r))
			 + Tap(uv, float2(-r, 0)) * 2.0f	+ Tap(uv, float2(0, 0)) * 4.0f		+ Tap(uv, float2(r, 0)) * 2.0f
			 + Tap(uv, float2(-r, r))			+ Tap(uv, float2(0, r)) * 2.0f		+ Tap(uv, float2(r, r));
	
	float3 high = gHigh.Load(int3(id.xy, 0)).rgb;
	gOut[id.xy] = float4(high + s / 16.0f, 1.0f);
}
