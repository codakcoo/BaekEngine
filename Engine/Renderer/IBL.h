#pragma once
#include "RHI/DxUtil.h"
#include "RHI/DescriptorHeap.h"

namespace baek
{
	class Renderer;
	class Texture;

	// 환경맵(파노라마)으로부터 이미지 기반 조명용 큐브맵들을 미리 계산해 소유한다.
	class IBL
	{
	public:
		void Init(Renderer& renderer, const Texture& equirect, UINT envSize = 512, UINT irradianceSize = 32);
		void Shutdown();

		D3D12_GPU_DESCRIPTOR_HANDLE EnvCubeSrv() const { return mEnv.srv.gpu; }
		D3D12_GPU_DESCRIPTOR_HANDLE IrradianceSrv() const { return mIrradiance.srv.gpu; }


	private:
		struct Cube
		{
			ComPtr<ID3D12Resource> res;
			DescriptorHandle srv{}, uav{};
			UINT size = 0;
		};

		void CreateCube(ID3D12Device* device, UINT size, DXGI_FORMAT format, Cube& out, const wchar_t* name);
		void FreeCube(Cube& c);

	private:
		DescriptorHeap* mHeap = nullptr;
		Cube mEnv, mIrradiance;

		ComPtr<ID3D12RootSignature> mRootSig;
		ComPtr<ID3D12PipelineState> mEquirectPso, mIrradiancePso;
	};
}