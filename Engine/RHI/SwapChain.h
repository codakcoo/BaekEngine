#pragma once
#include "RHI/Device.h"

namespace baek
{
	class SwapChain
	{
	public:
		static constexpr uint32_t BufferCount = 2;
		static constexpr DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;

		void Init(Device& device, HWND hwnd, uint32_t width, uint32_t height);
		void Resize(Device& device, uint32_t width, uint32_t height);
		void Present(bool vsync);

		ID3D12Resource* CurrentBuffer() const { return mBuffers[mSwap->GetCurrentBackBufferIndex()].Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const;

	private:
		void CreateRtvs(ID3D12Device* device);

		ComPtr<IDXGISwapChain3> mSwap;
		ComPtr<ID3D12Resource> mBuffers[BufferCount];
		ComPtr<ID3D12DescriptorHeap> mRtvHeap;
		uint32_t mRtvSize = 0;
	};
}