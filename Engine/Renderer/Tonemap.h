#pragma once
#include "RHI/DxUtil.h"

namespace baek
{
	class TonemapPass
	{
	public:
		void Init(ID3D12Device* device, DXGI_FORMAT outputFormat);
		void Render(ID3D12GraphicsCommandList* cmd, ID3D12DescriptorHeap* srvHeap, D3D12_GPU_DESCRIPTOR_HANDLE sceneSrv, float exposure);
		void Shutdown();

	private:
		ComPtr<ID3D12RootSignature> mRootSig;
		ComPtr<ID3D12PipelineState> mPso;
	};
}