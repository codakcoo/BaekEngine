#include "RHI\SwapChain.h"

namespace baek
{
	void SwapChain::Init(Device& device, HWND hwnd, uint32_t width, uint32_t height)
	{
		DXGI_SWAP_CHAIN_DESC1 sd = {};
		sd.Width						= width;
		sd.Height						= height;
		sd.Format						= Format;
		sd.SampleDesc					= { 1, 0 };
		sd.BufferUsage					= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount					= BufferCount;
		sd.SwapEffect					= DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.Scaling						= DXGI_SCALING_STRETCH;

		ComPtr<IDXGISwapChain1> sc1;
		ThrowIfFailed(device.Factory()->CreateSwapChainForHwnd(device.Queue(), hwnd, &sd, nullptr, nullptr, &sc1));
		ThrowIfFailed(sc1.As(&mSwap));
		device.Factory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

		D3D12_DESCRIPTOR_HEAP_DESC hd = {};
		hd.Type					= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		hd.NumDescriptors		= BufferCount;
		ThrowIfFailed(device.Get()->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&mRtvHeap)));
		mRtvSize = device.Get()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		CreateRtvs(device.Get());
	}
	void SwapChain::Resize(Device& device, uint32_t width, uint32_t height)
	{
		for(auto& b : mBuffers) b.Reset();
		ThrowIfFailed(mSwap->ResizeBuffers(BufferCount, width, height, Format, 0));
		CreateRtvs(device.Get());
	}
	
	void SwapChain::Present(bool vsync)
	{
		ThrowIfFailed(mSwap->Present(vsync ? 1: 0, 0));
	}

	D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::CurrentRtv() const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE h = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		h.ptr += (SIZE_T)mSwap->GetCurrentBackBufferIndex() * mRtvSize;
		return h;
	}

	void SwapChain::CreateRtvs(ID3D12Device* device)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE h = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		for (uint32_t i = 0; i < BufferCount; ++i)
		{
			ThrowIfFailed(mSwap->GetBuffer(i, IID_PPV_ARGS(&mBuffers[i])));
			device->CreateRenderTargetView(mBuffers[i].Get(), nullptr, h);
			h.ptr += mRtvSize;
		}
	}
}