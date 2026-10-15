#pragma once
#include "RHI/GpuResource.h"
#include "RHI/DescriptorHeap.h"
#include "RHI/DeferredRelease.h"
#include <vector>
#include <algorithm>

namespace baek
{
	class Renderer;
	class FrameGraph;

	struct BloomSettings
	{
		bool enabled = true;
		float threshold = 1.0f;
		float knee = 0.5f;
		float radius = 1.0f;
		float intensity = 0.3f;
	};
	
	class BloomPass
	{
	public:
		static constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		static constexpr UINT kMaxMips = 6;
		
		void Init(Renderer& renderer);
		void Resize(UINT sceneW, UINT sceneH);
		void AddPasses(FrameGraph& g, GpuResource& scene, D3D12_GPU_DESCRIPTOR_HANDLE sceneSrv, const BloomSettings& s);
		void Shutdown();

		GpuResource& Output() { return mUp; }
		D3D12_GPU_DESCRIPTOR_HANDLE OutputSrv() const { return mUpSrv[0].gpu; }
		UINT Mips() const { return mMips; }

	private:
		void Retire();
		void CreateChain(GpuResource& tex, std::vector<DescriptorHandle>& srv, std::vector<DescriptorHandle>& uav, const wchar_t* name);
		void Dispatch(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso,
					  D3D12_GPU_DESCRIPTOR_HANDLE src, D3D12_GPU_DESCRIPTOR_HANDLE high,
					  D3D12_GPU_DESCRIPTOR_HANDLE out,
					  UINT srcW, UINT srcH, UINT outW, UINT outH, const BloomSettings& s);

		UINT MipW(UINT i) const { return std::max(1u, mW >> i); }
		UINT MipH(UINT i) const { return std::max(1u, mH >> i); }

	private:
		ID3D12Device* mDevice = nullptr;
		DescriptorHeap* mSrvHeap = nullptr;
		DeferredReleaseQueue* mRelease = nullptr;

		ComPtr<ID3D12RootSignature> mRootSig;
		ComPtr<ID3D12PipelineState> mPrefilterPso, mDownPso, mUpPso;

		GpuResource mDown, mUp;
		std::vector<DescriptorHandle> mDownSrv, mDownUav, mUpSrv, mUpUav;			// 밉마다 하나씩
		UINT mSrcW = 0, mSrcH = 0;					// sceneRT 크기
		UINT mW = 0, mH = 0;
		UINT mMips = 0;
	};
}