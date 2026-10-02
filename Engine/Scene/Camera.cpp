#include "Scene/Camera.h"
#include <algorithm>

using namespace DirectX;

namespace baek
{
	void Camera::SetLens(float fovY, float aspect, float zn, float zf)
	{
		mFovY = fovY; mAspect = aspect; mNear = zn; mFar = zf;
	}

	DirectX::XMMATRIX Camera::View() const
	{
		return XMMatrixLookToLH(XMLoadFloat3(&mPos), Forward(), XMVectorSet(0, 1, 0, 0));
	}

	DirectX::XMMATRIX Camera::Proj() const
	{
		return XMMatrixPerspectiveFovLH(mFovY, mAspect, mNear, mFar);
	}

	void Camera::Rotate(float dYaw, float dPitch)
	{
		const float limit = XMConvertToRadians(89.0f);				// 수직으로 넘어가면 뒤집히므로 제한
		mYaw += dYaw;
		mPitch = std::clamp(mPitch + dPitch, -limit, limit);
	}

	void Camera::MoveLocal(float right, float up, float forward)
	{
		const XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
		const XMVECTOR f = Forward();
		const XMVECTOR r = XMVector3Normalize(XMVector3Cross(worldUp, f));			// LH: up x forward = right (직교)

		XMVECTOR p = XMLoadFloat3(&mPos);
		p += r * right + worldUp * up + f * forward;
		XMStoreFloat3(&mPos, p);
	}

	void Camera::Orbit(float dYaw, float dPitch)
	{
		const XMVECTOR pivot = XMLoadFloat3(&mPos) + Forward() * mOrbitDist;
		Rotate(dYaw, dPitch);
		XMStoreFloat3(&mPos, pivot - Forward() * mOrbitDist);
	}

	void Camera::Zoom(float amount)
	{
		amount = std::min(amount, mOrbitDist - 0.2f);					// 피벗을 뚫고 지나가지 않게
		XMStoreFloat3(&mPos, XMLoadFloat3(&mPos) + Forward() * amount);
		mOrbitDist -= amount;
	}

	DirectX::XMVECTOR Camera::Forward() const
	{
		const float cp = cosf(mPitch);
		return XMVectorSet(cp * sinf(mYaw), sinf(mPitch), cp * cosf(mYaw), 0.0f);
	}
}

