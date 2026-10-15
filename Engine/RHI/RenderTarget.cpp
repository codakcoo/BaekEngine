#include "RHI/RenderTarget.h"

namespace baek
{
	void RenderTarget::Init(ID3D12Device* device, 
							DescriptorHeap* rtvHeap, DescriptorHeap* dsvHeap, DescriptorHeap* srvHeap, 
							DXGI_FORMAT format, const float clearColor[4], bool widthDepth, DeferredReleaseQueue* release)
	{
		mDevice = device; mRtvHeap = rtvHeap; mDsvHeap = dsvHeap; mSrvHeap = srvHeap; mFormat = format; mHasDepth = widthDepth;	mRelease = release;
		memcpy(mClear, clearColor, sizeof(mClear));
		// 리소스와 디스크립터는 Resize에서 생성
	}

	void RenderTarget::Retire()
	{
		// RTV / DSV: CPU 전용 힙이라 즉시 해제해도 안전
		if (mRtv.cpu.ptr) { mRtvHeap->Free(mRtv); mRtv = {}; }
		if (mDsv.cpu.ptr) { mDsvHeap->Free(mDsv); mDsv = {}; }

		// 리소스와 SRV: GPU가 다 쓴 뒤에 해제
		mRelease->Release(std::move(mColor.Detach()));
		mRelease->Release(std::move(mDepth.Detach()));
		if (mSrv.cpu.ptr)
		{
			DescriptorHeap* heap = mSrvHeap;
			DescriptorHandle srv = mSrv;
			mRelease->Defer([heap, srv]() mutable { heap->Free(srv); });
			mSrv = {};
		}
	}

	void RenderTarget::Resize(UINT w, UINT h)
	{
		if (w == 0 || h == 0 || (mColor && w == mWidth && h == mHeight)) return;

		Retire();
		mWidth = w; mHeight = h;

		// 새 디스크립터 슬롯
		mRtv = mRtvHeap->Allocate();
		mSrv = mSrvHeap->Allocate();
		if (mHasDepth) mDsv = mDsvHeap->Allocate();

		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(mFormat, w, h, 1, 1);
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE cv{ mFormat, { mClear[0], mClear[1], mClear[2], mClear[3] } };
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);

		ComPtr<ID3D12Resource> tex;
		ThrowIfFailed(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
		&desc, kShaderReadState, &cv, IID_PPV_ARGS(&tex)));
		tex->SetName(L"RenderTarget");
		mColor.Attach(tex, kShaderReadState);					// 생성 시 상태를 그대로 기록
		
		mDevice->CreateRenderTargetView(mColor.Get(), nullptr, mRtv.cpu);
		mDevice->CreateShaderResourceView(mColor.Get(), nullptr, mSrv.cpu);

		// 깊이
		if (mHasDepth)
		{
			auto ddesc = CD3DX12_RESOURCE_DESC::Tex2D(DepthFormat, w, h, 1, 1);
			ddesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			D3D12_CLEAR_VALUE dcv{};
			dcv.Format = DepthFormat;
			dcv.DepthStencil.Depth = 1.0f;

			ComPtr<ID3D12Resource> depth;
			ThrowIfFailed(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
				&ddesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &dcv, IID_PPV_ARGS(&depth)));
			depth->SetName(L"RenderTargetDepth");
			mDepth.Attach(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			mDevice->CreateDepthStencilView(mDepth.Get(), nullptr, mDsv.cpu);
		}
	}
	
	void RenderTarget::Shutdown()
	{
		mColor.Reset();
		mDepth.Reset();
		if (mRtv.cpu.ptr) { mRtvHeap->Free(mRtv); mRtv = {}; }
		if (mDsv.cpu.ptr) { mDsvHeap->Free(mDsv); mDsv = {}; }
		if (mSrv.cpu.ptr) { mSrvHeap->Free(mSrv); mSrv = {}; }
	}

	void RenderTarget::Begin(ID3D12GraphicsCommandList* cmd)			// 그래프 밖에서 슬 때용으로 유지
	{
		// 컬러와 깊이 전환을 한 번에 (깊이가 없으면 자동으로 건너뜀)
		BarrierBatch(cmd).Transition(mColor, D3D12_RESOURCE_STATE_RENDER_TARGET);
		BarrierBatch(cmd).Transition(mDepth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		Bind(cmd, true);
	}

	void RenderTarget::End(ID3D12GraphicsCommandList* cmd)
	{
		Transition(cmd, mColor, kShaderReadState);
	}

	void RenderTarget::Bind(ID3D12GraphicsCommandList* cmd, bool clear)
	{
		cmd->OMSetRenderTargets(1, &mRtv.cpu, FALSE, mHasDepth ? &mDsv.cpu : nullptr);
		if (clear)
		{
			cmd->ClearRenderTargetView(mRtv.cpu, mClear, 0, nullptr);
			if(mHasDepth) cmd->ClearDepthStencilView(mDsv.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0 ,0, nullptr);
		}

		D3D12_VIEWPORT vp{ 0, 0, (float)mWidth, (float)mHeight, 0, 1 };
		D3D12_RECT sc{ 0, 0, (LONG)mWidth, (LONG)mHeight };
		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &sc);
	}
}
