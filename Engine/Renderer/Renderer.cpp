#include "Renderer\Renderer.h"
#include "d3dx12.h"

namespace baek
{
	void Renderer::Init(Window& window)
	{
		mWindow = &window;
#if defined(_DEBUG)
		mDevice.Init(true);
#else
		mDevice.Init(false);
#endif
		mSwapChain.Init(mDevice, window.Handle(), window.Width(), window.Height());
		mSrvHeap.Init(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true);
		mRtvHeap.Init(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, false);

		for (auto& a : mAllocators)
		{
			ThrowIfFailed(mDevice.Get()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a)));
		}
		ThrowIfFailed(mDevice.Get()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mAllocators[0].Get(), nullptr, IID_PPV_ARGS(&mCmd)));
		ThrowIfFailed(mCmd->Close());
	}

	void Renderer::Shutdown()
	{
		mDevice.Shutdown();
	}

	// shader-visible이 false인 힙에서는 GetGPUDescriptorHandleForHeapStart()을 호출하면 안됨
	// DescriptorHeap::Init이 mShaderVisible일 때만 GPU 핸들을 계산하는지 확인해야함
	void Renderer::BindBackBuffer()
	{
		auto rtv = mSwapChain.CurrentRtv();
		mCmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	}

	void Renderer::BeginFrame(const float clearColor[4])
	{
		if (mWindow->ConsumeResize())
		{
			mDevice.Flush();
			mSwapChain.Resize(mDevice, mWindow->Width(), mWindow->Height());
		}

		mDevice.WaitForValue(mFrameFence[mFrameIndex]);

		auto* alloc = mAllocators[mFrameIndex].Get();
		ThrowIfFailed(alloc->Reset());
		ThrowIfFailed(mCmd->Reset(alloc, nullptr));

		auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChain.CurrentBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCmd->ResourceBarrier(1, &toRT);

		auto rtv = mSwapChain.CurrentRtv();
		mCmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		mCmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);						// ImGui가 여기에 그림
	}
	void Renderer::EndFrame()
	{
		auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChain.CurrentBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mCmd->ResourceBarrier(1, &toPresent);

		ThrowIfFailed(mCmd->Close());
		ID3D12CommandList* lists[] = { mCmd.Get() };
		mDevice.Queue()->ExecuteCommandLists(1, lists);

		mSwapChain.Present(true);
		mFrameFence[mFrameIndex] = mDevice.Signal();
		mFrameIndex = (mFrameIndex + 1) % FrameCount;
	}
}
