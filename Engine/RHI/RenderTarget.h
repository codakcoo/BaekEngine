#pragma once
#include "RHI/DxUtil.h"
#include "RHI/DescriptorHeap.h"
#include "RHI/DeferredRelease.h"
#include "RHI/GpuResource.h"

namespace baek
{
	class RenderTarget
	{
	public:
		void Init(ID3D12Device* device, DescriptorHeap* rtvHeap, DescriptorHeap* dsvHeap, DescriptorHeap* srvHeap,
			DXGI_FORMAT format, const float clearColor[4], bool widthDepth, DeferredReleaseQueue* release);
		void Resize(UINT w, UINT h);					// 옛 리소스는 지연 해제 큐로 (GPU 대기 없음)
		void Shutdown();

		void Begin(ID3D12GraphicsCommandList* cmd);				// PSR -> RT, Clear, 바인딩
		void End(ID3D12GraphicsCommandList* cmd);				// RT -> PSR
		void Bind(ID3D12GraphicsCommandList* cmd, bool clearColor, bool clearDepth = true);
		void BindWithDepth(ID3D12GraphicsCommandList* cmd, bool clearColor, D3D12_CPU_DESCRIPTOR_HANDLE dsv, bool clearDepth);

		D3D12_CPU_DESCRIPTOR_HANDLE DsvCpu() const { return mDsv.cpu; }
		D3D12_GPU_DESCRIPTOR_HANDLE Srv() const { return mSrv.gpu; }
		UINT Width() const { return mWidth; }
		UINT Height() const { return mHeight; }

		GpuResource& Color() { return mColor; }			// 다른 패스가 직접 상태를 바꿀 수 있게
		GpuResource& Depth() { return mDepth; }

	protected:
		void Retire();

	// property
	public:
		static constexpr DXGI_FORMAT DepthFormat = DXGI_FORMAT_D32_FLOAT;

	private:
		ID3D12Device* mDevice = nullptr;
		DescriptorHeap* mRtvHeap = nullptr;
		DescriptorHeap* mDsvHeap = nullptr;
		DescriptorHeap* mSrvHeap = nullptr;

		GpuResource mColor;					// mTex대체
		GpuResource mDepth;					// mDepth 대체
		DescriptorHandle mRtv{}, mSrv{}, mDsv{};
		DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		UINT mWidth = 0, mHeight = 0;
		float mClear[4] = { 0,0,0,1 };

		bool mHasDepth = true;

		DeferredReleaseQueue* mRelease = nullptr;

	};
}
