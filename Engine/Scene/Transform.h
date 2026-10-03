#pragma once
#include <DirectXMath.h>

namespace baek
{
	struct Transform
	{
		DirectX::XMFLOAT3 position{ 0, 0, 0 };
		DirectX::XMFLOAT3 rotation{ 0, 0, 0};				// 오일러 각 (도 단위, 에디터 표시 -> 계산은 쿼터니언으로) 
		DirectX::XMFLOAT3 scale{ 1, 1, 1 };


		// S * R * T			(S = Scale, R = Rotation, T = Translation)
		DirectX::XMMATRIX Matrix() const
		{
			using namespace DirectX;
			return XMMatrixScaling(scale.x, scale.y, scale.z)
					* XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotation.x),
												   XMConvertToRadians(rotation.y),
												   XMConvertToRadians(rotation.z))
					* XMMatrixTranslation(position.x, position.y, position.z);
		}
	};
}