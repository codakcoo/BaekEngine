#pragma once
#include "Core/Window.h"
#include "RHI/Device.h"
#include "RHI/SwapChain.h"
#include "RHI/DescriptorHeap.h"

namespace baek
{
	class Renderer
	{
	public:
		static constexpr uint32_t FrameCount = 2;						// CPU가 앞서갈 수 있는 프레임 수

		void Init(Window& window);
		void Shutdown();
		void WaitIdle() { mDevice.Flush(); }

		void BeginFrame(const float clearColor[4]);						// 대기, 리셋, RT 전이, 클리어, RT 바인딩
		void EndFrame();												// Present 전이, 실행, Present

		Device& GetDevice() { return mDevice; }
		DescriptorHeap& SrvHeap() { return mSrvHeap; }
		ID3D12GraphicsCommandList* CommandList() const { return mCmd.Get(); }

	private:
		Window* mWindow = nullptr;
		Device mDevice;
		SwapChain mSwapChain;
		DescriptorHeap mSrvHeap;										// shader-visible CBV/SRV/UAV (엔진 전체 공용)

		ComPtr<ID3D12CommandAllocator>			mAllocators[FrameCount];
		uint64_t								mFrameFence[FrameCount] = {};
		uint64_t								mFrameIndex = 0;
		ComPtr<ID3D12GraphicsCommandList>		mCmd;
	};
}