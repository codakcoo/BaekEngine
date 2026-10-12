#pragma once
#include "RHI/DxUtil.h"
#include "RHI/UploadRing.h"
#include "Renderer/Texture.h"
#include "Renderer/Material.h"
#include "Renderer\ShadowMap.h"
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
		// 매 프레임 Render보다 먼저 호출(상수 버퍼 링을 여기서 비움)
		void RenderShadowPass(ID3D12GraphicsCommandList* cmd, UINT frameIndex, const Camera& camera, const Scene& scene, ShadowMap& shadow);
		void Shutdown();
		
		void SetIBL(D3D12_GPU_DESCRIPTOR_HANDLE irradiance, D3D12_GPU_DESCRIPTOR_HANDLE prefiltered, D3D12_GPU_DESCRIPTOR_HANDLE brdfLut) 
		{ mIrradianceSrv = irradiance; mPrefilteredSrv = prefiltered; mBrdfLutSrv = brdfLut; }
	private:
		ComPtr<ID3D12RootSignature> mRootSig;
		ComPtr<ID3D12PipelineState> mPso;

		// 그리드
		ComPtr<ID3D12PipelineState> mLinePSO;
		ComPtr<ID3D12Resource> mGridVB;
		D3D12_VERTEX_BUFFER_VIEW mGridVbv{};
		UINT mGridVertexCount = 0;

		UploadRing mCB[2];
		
		// 섀도우맵
		ComPtr<ID3D12PipelineState> mShadowPso;
		DirectX::XMFLOAT4X4 mCascadeVP[ShadowMap::kCascades]{};
		DirectX::XMFLOAT4 mCascadeSplits{};						// 구간별 끝 거리 (카메라 기준)
		DirectX::XMFLOAT4 mCascadeTexel{};						// 구간별 텍셀 하나의 월드 크기
		D3D12_GPU_DESCRIPTOR_HANDLE mShadowSrv{};
		float mShadowTexel = 0.0f;

		Texture mWhite;								// 텍스처가 없는 엔티티용 1x1 흰색 (albedo / metallic-roughness 기본값)
		Texture mFlatNormal;						// normal 기본값
		Material mDefaultMaterial;
		ID3D12DescriptorHeap* mSrvHeap = nullptr;

		D3D12_GPU_DESCRIPTOR_HANDLE mIrradianceSrv{}, mPrefilteredSrv{}, mBrdfLutSrv;
	};
}