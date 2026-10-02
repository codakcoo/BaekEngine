#pragma once
#include "RHI/DxUtil.h"

namespace baek
{
	class SceneRenderer
	{
	public:
		void Init(ID3D12Device* device, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);
		void Render(ID3D12GraphicsCommandList* cmd, float aspect, float time);
		void Shutdown();
		
	private:
		ComPtr<ID3D12RootSignature> mRootSig;
		ComPtr<ID3D12PipelineState> mPso;
		ComPtr<ID3D12Resource> mVB, mIB;
		D3D12_VERTEX_BUFFER_VIEW mVbv{};
		D3D12_INDEX_BUFFER_VIEW mIbv{};
		UINT mIndexCount = 0;
	};
}
