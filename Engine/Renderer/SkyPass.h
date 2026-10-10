#pragma	once
#include "RHI/DxUtil.h"

namespace baek
{
	class Camera;

	class SkyPass
	{
	public:
		void Init(ID3D12Device* device, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);
		void Render(ID3D12GraphicsCommandList* cmd, ID3D12DescriptorHeap* srvHeap, D3D12_GPU_DESCRIPTOR_HANDLE envSrv, const Camera& camera, float intensity);
		void Shutdown();

	private:
		ComPtr<ID3D12RootSignature> mRootSig;
		ComPtr<ID3D12PipelineState> mPso;
	};
}