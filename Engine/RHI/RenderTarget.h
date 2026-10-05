#pragma once
#include "RHI/DxUtil.h"
#include "RHI/DescriptorHeap.h"

namespace baek
{
	class RenderTarget
	{
	public:
		void Init(ID3D12Device* device, DescriptorHeap* rtvHeap, DescriptorHeap* dsvHeap, DescriptorHeap* srvHeap,
			DXGI_FORMAT format, const float clearColor[4], bool widthDepth = true);
		void Resize(UINT w, UINT h);					// 호출 전 GPU Idle 보장 필요
		void Shutdown();

		void Begin(ID3D12GraphicsCommandList* cmd);				// PSR -> RT, Clear, 바인딩
		void End(ID3D12GraphicsCommandList* cmd);				// RT -> PSR

		D3D12_GPU_DESCRIPTOR_HANDLE Srv() const { return mSrv.gpu; }
		UINT Width() const { return mWidth; }
		UINT Height() const { return mHeight; }


	// property
	public:
		static constexpr DXGI_FORMAT DepthFormat = DXGI_FORMAT_D32_FLOAT;

	private:
		ID3D12Device* mDevice = nullptr;
		DescriptorHeap* mRtvHeap = nullptr;
		DescriptorHeap* mDsvHeap = nullptr;
		DescriptorHeap* mSrvHeap = nullptr;

		ComPtr<ID3D12Resource> mTex;
		ComPtr<ID3D12Resource> mDepth;
		DescriptorHandle mRtv{}, mSrv{}, mDsv;
		DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		UINT mWidth = 0, mHeight = 0;
		float mClear[4] = { 0,0,0,1 };

		bool mHasDepth = true;
	};
}
