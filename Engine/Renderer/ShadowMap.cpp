#include "ShadowMap.h"

namespace baek
{
	void ShadowMap::Init(ID3D12Device* device, DescriptorHeap* dsvHeap, DescriptorHeap* srvHeap, UINT size)
	{
		mDsvHeap = dsvHeap; mSrvHeap = srvHeap; mSize = size;

		// TYPELESS: 같은 메모리를 DSV(D32_FLOAT)와 SRV(R32_FLOAT)로 다르게 해석
		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, size, size, 1, 1, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		D3D12_CLEAR_VALUE clear{};
		clear.Format = DsvFormat;
		clear.DepthStencil.Depth = 1.0f;

		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&mTex)));
		mTex->SetName(L"ShadowMap");

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
		dsv.Format = DsvFormat;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		mDsv = mDsvHeap->Allocate();
		device->CreateDepthStencilView(mTex.Get, &dsv, mDsv.cpu);

		D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
		srv.Format = DXGI_FORMAT_R32_FLOAT;
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.Texture2D.MipLevels = 1;
		mSrv = mSrvHeap->Allocate();
		device->CreateShaderResourceView(mTex.Get(), &srv, mSrv.cpu);
	}

	void ShadowMap::Begin(ID3D12GraphicsCommandList* cmd)
	{
		auto b = CD3DX12_RESOURCE_BARRIER::Transition(mTex.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		cmd->ResourceBarrier(1, &b);

		cmd->OMSetRenderTargets(0, nullptr, FALSE, &mDsv.cpu);		// 컬러 타깃 없음, 깊이만
		cmd->ClearDepthStencilView(mDsv.cpu, nullptr, 1.0f, 0, 0, nullptr);

		D3D12_VIEWPORT vp{ 0, 0, (float)mSize, (float)mSize, 0, 1 };
		D3D12_RECT sc{ 0, 0, (LONG)mSize, (LONG)mSize };
		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &sc);
	}

	void ShadowMap::End(ID3D12GraphicsCommandList* cmd)
	{
		auto b = CD3DX12_RESOURCE_BARRIER::Transition(mTex.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmd->ResourceBarrier(1, &b);
	}
	void ShadowMap::Shutdown()
	{
		mTex.Reset();
		if(mDsvHeap) mDsvHeap->Free(mDsv);
		if(mSrvHeap) mSrvHeap->Free(mSrv);
	}
}