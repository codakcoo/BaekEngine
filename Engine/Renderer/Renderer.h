#pragma once
#include "Core/Window.h"
#include "RHI/Device.h"
#include "RHI/SwapChain.h"

namespace baek
{
	class Renderer
	{
	public:
		static constexpr uint32_t FrameCount = 2;						// CPU가 앞서갈 수 있는 프레임 수

		void Init(Window& window);
		void Shutdown();
		void Render(const float clearColor[4]);

	private:
		Window* mWindow = nullptr;
		Device mDevice;
		SwapChain mSwapChain;

		ComPtr<ID3D12CommandAllocator>			mAllocators[FrameCount];
		uint64_t								mFrameFence[FrameCount] = {};
		uint64_t								mFrameIndex = 0;
		ComPtr<ID3D12GraphicsCommandList>		mCmd;
	};
}