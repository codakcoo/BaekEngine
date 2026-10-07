#pragma once
#include <DirectXMath.h>
#include <cmath>

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

		static Transform FromMatrix(DirectX::FXMMATRIX m)
		{
			using namespace DirectX;
			XMFLOAT4X4 f;
			XMStoreFloat4x4(&f, m);


			Transform t;
			t.position = { f._41, f._42, f._43 };

			// 각 행의 길이 = 그 축의 스케일
			float s[3];
			for (int i = 0; i < 3; ++i)
			{
				s[i] = sqrtf(f.m[i][0] * f.m[i][0] + f.m[i][1] * f.m[i][1] + f.m[i][2] * f.m[i][2]);
				if (s[i] < 1e-8f) s[i] = 1e-8f;
			}
			t.scale = { s[0], s[1], s[2] };

			// 스케일을 제거한 회전 행렬에서 오일러 각 추출
			const float r01 = f.m[0][1] / s[0], r00 = f.m[0][0] / s[0], r02 = f.m[0][2] / s[0];
			const float r12 = f.m[1][2] / s[1], r22 = f.m[2][2] / s[2];

			t.rotation =
			{
				XMConvertToDegrees(atan2f(r12, r22)),
				XMConvertToDegrees(atan2f(-r02, sqrtf(r12 * r12 + r22 * r22))),
				XMConvertToDegrees(atan2f(r01, r00)),
			};

			return t;
		}
	};
}