#pragma once
#include "RHI/DxUtil.h"
#include "RHI/DescriptorHeap.h"

namespace baek
{
	class ShadowMap
	{
	public:
		static constexpr DXGI_FORMAT DsvFormat = DXGI_FORMAT_D32_FLOAT;

		void Init(ID3D12Device* device, DescriptorHeap* dsvHeap, DescriptorHeap* srvHeap, UINT size);
		void Begin(ID3D12GraphicsCommandList* cmd);				// SRV -> 깊이 쓰기, Clear, 바인딩
		void End(ID3D12GraphicsCommandList* cmd);				// 깊이 쓰기 -> SRV
		void Shutdown();

		D3D12_GPU_DESCRIPTOR_HANDLE Srv() const { return mSrv.gpu; }
		UINT Size() const { return mSize; }

	private:
		ComPtr<ID3D12Resource> mTex;
		DescriptorHeap* mDsvHeap = nullptr;
		DescriptorHeap* mSrvHeap = nullptr;
		DescriptorHandle mDsv{}, mSrv{};
		UINT mSize = 0;
	};
}