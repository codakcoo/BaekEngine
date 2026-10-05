#include "Renderer\Renderer.h"

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
		mDsvHeap.Init(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, false);

		for (auto& a : mAllocators)
		{
			ThrowIfFailed(mDevice.Get()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a)));
		}
		ThrowIfFailed(mDevice.Get()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mAllocators[0].Get(), nullptr, IID_PPV_ARGS(&mCmdList)));
		ThrowIfFailed(mCmdList->Close());
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
		mCmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	}

	void Renderer::Immediate(const std::function<void(ID3D12GraphicsCommandList*)>& fn)
	{
		WaitIdle();

		auto& alloc = mAllocators[mFrameIndex];
		ThrowIfFailed(alloc->Reset());
		ThrowIfFailed(mCmdList->Reset(alloc.Get(), nullptr));

		fn(mCmdList.Get());

		ThrowIfFailed(mCmdList->Close());
		ID3D12CommandList* lists[] = { mCmdList.Get() };
		mDevice.Queue()->ExecuteCommandLists(1, lists);		// EndFrame에서 쓰는 큐 접근 방식과 동일

		WaitIdle();											// 복사가 끝날 때까지 대기 -> 업로드 버퍼를바로 해제해도 안전
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
		ThrowIfFailed(mCmdList->Reset(alloc, nullptr));

		auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChain.CurrentBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCmdList->ResourceBarrier(1, &toRT);

		auto rtv = mSwapChain.CurrentRtv();
		mCmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		mCmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);						// ImGui가 여기에 그림
	}
	void Renderer::EndFrame()
	{
		auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChain.CurrentBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mCmdList->ResourceBarrier(1, &toPresent);

		ThrowIfFailed(mCmdList->Close());
		ID3D12CommandList* lists[] = { mCmdList.Get() };
		mDevice.Queue()->ExecuteCommandLists(1, lists);

		mSwapChain.Present(true);
		mFrameFence[mFrameIndex] = mDevice.Signal();
		mFrameIndex = (mFrameIndex + 1) % FrameCount;
	}
}
