#pragma once
#include "RHI/GpuResource.h"
#include "RHI/DescriptorHeap.h"
#include "RHI/DeferredRelease.h"

namespace baek
{
	class Renderer;
	class FrameGraph;
	class Camera;

	struct SsaoSettings
	{
		bool enabled = true;
		bool showAO = false;
		float radius = 0.5f;
		float bias = 0.025f;
		float power = 1.5f;
	};

	class SsaoPass
	{
	public:
		static constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R16_FLOAT;

		void Init(Renderer& renderer);
		void Resize(UINT w, UINT h);
		void AddPasses(FrameGraph& g, GpuResource& normalDepth, D3D12_GPU_DESCRIPTOR_HANDLE nd, const Camera& camera, const SsaoSettings& s);
		void Shutdown();

		GpuResource& Output() { return mBlur; }
		D3D12_GPU_DESCRIPTOR_HANDLE OutputSrv() const { return mBlurSrv.gpu; }

	private:
		struct Constants
		{
			float tanX, tanxY, radius, bias, power, pad[3];
		};

		void Retire();
		void Create(GpuResource& tex, DescriptorHandle& srv, DescriptorHandle& uav, const wchar_t* name);
		void Dispatch(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso,
			D3D12_GPU_DESCRIPTOR_HANDLE t0, D3D12_GPU_DESCRIPTOR_HANDLE t1,
			D3D12_GPU_DESCRIPTOR_HANDLE u0, const Constants& c);

	private:
		ID3D12Device* mDevice = nullptr;
		DescriptorHeap* mSrvHeap = nullptr;
		DeferredReleaseQueue* mRelease = nullptr;

		ComPtr<ID3D12RootSignature> mRootSig;
		ComPtr<ID3D12PipelineState> mSsaoPso, mBlurPso;

		GpuResource mRaw, mBlur;
		DescriptorHandle mRawSrv{}, mRawUav{}, mBlurSrv{}, mBlurUav{};
		UINT mW = 0, mH = 0;
	};
}