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

	void Renderer::Render(const float clearColor[4])
	{
		static uint64_t frame = 0;
		if ((++frame % 60) == 0)
		{
			char buf[64];
			sprintf_s(buf, "[Render] frame %llu\n", frame);
			OutputDebugStringA(buf);
		}

		// 리사이즈: 진행 중인 GPU 작업 전부 끝낸 뒤 버퍼 재생성
		if (mWindow->ConsumeResize())
		{
			mDevice.Flush();
			mSwapChain.Resize(mDevice, mWindow->Width(), mWindow->Height());
		}

		// 이 슬롯의 이전 프레임이 끝났는지 확인 (기존 FrameResource 대기와 같은 역할)
		mDevice.WaitForValue(mFrameFence[mFrameIndex]);

		auto* alloc = mAllocators[mFrameIndex].Get();
		ThrowIfFailed(alloc->Reset());
		ThrowIfFailed(mCmd->Reset(alloc, nullptr));

		ID3D12Resource* back = mSwapChain.CurrentBuffer();
		auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(back, 
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCmd->ResourceBarrier(1, &toRT);

		auto rtv = mSwapChain.CurrentRtv();
		mCmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

		auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(back,
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