#pragma once
#include <DirectXMath.h>
#include <string>

namespace baek
{
	class Texture;

	struct Material
	{
		std::string name;
		DirectX::XMFLOAT3 baseColor = { 1, 1, 1 };
		float metallic = 0.0f;
		float roughness = 0.5f;

		const Texture* albedoMap			= nullptr;		// sRGB
		const Texture* normalMap			= nullptr;		// linear
		const Texture* metallicRoughnessMap = nullptr;		// linear, G = roughness, B = metallic
	};
}