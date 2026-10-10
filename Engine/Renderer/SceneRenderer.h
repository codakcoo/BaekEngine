#pragma once
#include "RHI/DxUtil.h"
#include "RHI/UploadRing.h"
#include "Renderer/Texture.h"
#include "Renderer/Material.h"
#include <DirectXMath.h>

namespace baek
{
	class Scene;
	class Camera;
	class Renderer;

	class SceneRenderer
	{
	public:
		void Init(Renderer& renderer, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);
		void Render(ID3D12GraphicsCommandList* cmd, UINT frameIndex, const Camera& camera, const Scene& scene);
		void Shutdown();
		
		void SetIrradiance(D3D12_GPU_DESCRIPTOR_HANDLE srv) { mIrradianceSrv = srv; }
	private:
		ComPtr<ID3D12RootSignature> mRootSig;
		ComPtr<ID3D12PipelineState> mPso;

		// 그리드
		ComPtr<ID3D12PipelineState> mLinePSO;
		ComPtr<ID3D12Resource> mGridVB;
		D3D12_VERTEX_BUFFER_VIEW mGridVbv{};
		UINT mGridVertexCount = 0;

		UploadRing mCB[2];
		

		Texture mWhite;								// 텍스처가 없는 엔티티용 1x1 흰색 (albedo / metallic-roughness 기본값)
		Texture mFlatNormal;						// normal 기본값
		Material mDefaultMaterial;
		ID3D12DescriptorHeap* mSrvHeap = nullptr;

		D3D12_GPU_DESCRIPTOR_HANDLE mIrradianceSrv{};
	};
}