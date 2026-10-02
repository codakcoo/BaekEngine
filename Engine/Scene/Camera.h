#pragma once
#include <DirectXMath.h>

namespace baek
{
	class Camera
	{
	public:
		void SetLens(float fovY, float aspect, float zn, float zf);
		void SetAspect(float aspect) { mAspect = aspect; }

		DirectX::XMMATRIX View() const;
		DirectX::XMMATRIX Proj() const;
		DirectX::XMMATRIX ViewProj() const { return View() * Proj(); }

		void Rotate(float dYaw, float dPitch);
		void MoveLocal(float right, float up, float forward);
		void Orbit(float dYaw, float dPitch);
		void Zoom(float amount);

		const DirectX::XMFLOAT3& Position() const { return mPos;}

	private:
		DirectX::XMVECTOR Forward() const;

	// Property
	private:
		DirectX::XMFLOAT3 mPos{ 0.0f, 2.0f, -5.0f };
		float mYaw = 0.0f;						// 0이면 +Z를 봄
		float mPitch = -0.38f;					// 음수면 아래를 봄 (원점 방향)
		float mOrbitDist = 5.385f;				// 피벗까지의 거리

		float mFovY = DirectX::XM_PIDIV4, mAspect = 1.0f, mNear = 0.1f, mFar = 1000.0f;
	};
}