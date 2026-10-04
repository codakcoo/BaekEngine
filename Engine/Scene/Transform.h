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
			// ImGuizmo가 행렬을 오일러 각으로 분해할 때 쓰는 순서(X->Y->Z)와 우리 쪽 순서가 같아야 함.
			// 다를경우 기즈모가 다른 각도로 튈 수 있음.
			return XMMatrixScaling(scale.x, scale.y, scale.z)
				* XMMatrixRotationX(XMConvertToRadians(rotation.x))
				* XMMatrixRotationY(XMConvertToRadians(rotation.y))
				* XMMatrixRotationZ(XMConvertToRadians(rotation.z))
				* XMMatrixTranslation(position.x, position.y, position.z);
		}
	};
}