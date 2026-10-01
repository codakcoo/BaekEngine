#include "RHI/RenderTarget.h"
#include "d3dx12.h"				// 다른 파일에서 쓰는 include 경로와 맞출 것

namespace baek
{
	void RenderTarget::Init(ID3D12Device* device, DescriptorHeap* rtvHeap, DescriptorHeap* srvHeap, DXGI_FORMAT format, const float clearColor[4])
	{
		mDevice = device; mRtvHeap = rtvHeap; mSrvHeap = srvHeap; mFormat = format;
		memcpy(mClear, clearColor, sizeof(mClear));
		mRtv = mRtvHeap->Allocate();					// 디스크립터는 한 번만 할당하고 재사용
		mSrv = mSrvHeap->Allocate();
	}

	void RenderTarget::Resize(UINT w, UINT h)
	{
		if (w == 0 || h == 0 || (w == mWidth && h == mHeight)) return;
		mWidth = w; mHeight = h;
		mTex.Reset();

		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(mFormat, w, h, 1, 1);
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE cv{ mFormat, { mClear[0], mClear[1], mClear[2], mClear[3] } };
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);

		ThrowIfFailed(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
		&desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cv, IID_PPV_ARGS(&mTex)));

		mDevice->CreateRenderTargetView(mTex.Get(), nullptr, mRtv.cpu);
		mDevice->CreateShaderResourceView(mTex.Get(), nullptr, mSrv.cpu);
	}
	
	void RenderTarget::Shutdown()
	{
		mTex.Reset();
		if (mRtvHeap) mRtvHeap->Free(mRtv);
		if (mSrvHeap) mSrvHeap->Free(mSrv);
	}

	void RenderTarget::Begin(ID3D12GraphicsCommandList* cmd)
	{
		auto b = CD3DX12_RESOURCE_BARRIER::Transition(mTex.Get(), 
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd->ResourceBarrier(1, &b);

		cmd->OMSetRenderTargets(1, &mRtv.cpu, FALSE, nullptr);
		cmd->ClearRenderTargetView(mRtv.cpu, mClear, 0, nullptr);

		D3D12_VIEWPORT vp{ 0,0,(float)mWidth,(float)mHeight,0,1 };
		D3D12_RECT sc{ 0, 0, (LONG)mWidth,(LONG)mHeight };
		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &sc);
	}

	void RenderTarget::End(ID3D12GraphicsCommandList* cmd)
	{
		auto b = CD3DX12_RESOURCE_BARRIER::Transition(mTex.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmd->ResourceBarrier(1, &b);
	}
}
