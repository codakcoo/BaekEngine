#pragma once
#include "RHI/DxUtil.h"
#include "RHI/DescriptorHeap.h"
#include "RHI/GpuResource.h"

namespace baek
{
	class ShadowMap
	{
	public:
		static constexpr DXGI_FORMAT DsvFormat = DXGI_FORMAT_D32_FLOAT;
		static constexpr UINT		 kCascades = 4;

		void Init(ID3D12Device* device, DescriptorHeap* dsvHeap, DescriptorHeap* srvHeap, UINT size);
		void Begin(ID3D12GraphicsCommandList* cmd);								// 전체: SRV -> 깊이 쓰기
		void BeginCascade(ID3D12GraphicsCommandList* cmd, UINT cascade);		// 한 장: 바인딩, Clear
		void End(ID3D12GraphicsCommandList* cmd);								// 전체: 깊이 쓰기 -> SRV
		void Shutdown();

		D3D12_GPU_DESCRIPTOR_HANDLE Srv() const { return mSrv.gpu; }
		UINT Size() const { return mSize; }
		GpuResource& Resource() { return mTex; }

	private:
		GpuResource mTex;				// ComPtr 대체
		DescriptorHeap* mDsvHeap = nullptr;
		DescriptorHeap* mSrvHeap = nullptr;
		DescriptorHandle mDsv[kCascades]{}, mSrv{};
		UINT mSize = 0;
	};
}