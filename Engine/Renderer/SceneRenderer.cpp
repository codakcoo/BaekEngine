#include "Renderer/SceneRenderer.h"
#include "RHI/Shader.h"
#include <DirectXMath.h>

using namespace DirectX;


namespace baek
{
	namespace
	{
		struct Vertex
		{
			XMFLOAT3 pos;
			XMFLOAT3 color;
		};

		// 임시: 작은 정점 메시라 UPLOAD 힙에 직접 둔다 (에셋 시스템에서 DEFAULT 힙 + 복사로 교체)
		ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* d, const void* data, UINT64 size)
		{
			CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
			auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
			ComPtr<ID3D12Resource> buf;
			ThrowIfFailed(d->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, 
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)));

			void* p = nullptr;
			CD3DX12_RANGE noRead(0, 0);
			ThrowIfFailed(buf->Map(0, &noRead, &p));
			memcpy(p, data, size);
			buf->Unmap(0, nullptr);
			return buf;
		}
	}

	void SceneRenderer::Init(ID3D12Device* device, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
	{
		// --- Root Signature: b0 = MVP (root constants 16개) ---
		CD3DX12_ROOT_PARAMETER param;
		param.InitAsConstants(16, 0);
		CD3DX12_ROOT_SIGNATURE_DESC rsDesc(1, &param, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> sig, err;
		HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
		if(err)
			OutputDebugStringA((const char*)err->GetBufferPointer());
		ThrowIfFailed(hr);
		ThrowIfFailed(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&mRootSig)));

		// --- Shaders / PSO ---
		auto vs = CompileShader(ShaderPath(L"Basic.hlsl"), "VSMain", "vs_5_0");
		auto ps = CompileShader(ShaderPath(L"Basic.hlsl"), "PSMain", "ps_5_0");

		D3D12_INPUT_ELEMENT_DESC layout[] = 
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{ "COLOR",		0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = mRootSig.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.InputLayout = { layout, _countof(layout) };
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = rtvFormat;
		pso.DSVFormat = dsvFormat;
		pso.SampleDesc.Count = 1;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&mPso)));

		// --- Cube geometry ---
		const Vertex verts[] = 
		{
			{ {-1,-1,-1}, {1,1,1} }, { {-1, 1,-1}, {0,0,0} },
			{ { 1, 1,-1}, {1,0,0} }, { { 1,-1,-1}, {0,1,0} },
			{ {-1,-1, 1}, {0,0,1} }, { {-1, 1, 1}, {1,1,0} },
			{ { 1, 1, 1}, {0,1,1} }, { { 1,-1, 1}, {1,0,1} },
		};
		const uint16_t idx[] =
		{
			0,1,2, 0,2,3,   4,6,5, 4,7,6,   4,5,1, 4,1,0,
			3,2,6, 3,6,7,   1,5,6, 1,6,2,   4,0,3, 4,3,7,
		};
		mIndexCount = _countof(idx);

		mVB = CreateUploadBuffer(device, verts, sizeof(verts));
		mIB = CreateUploadBuffer(device, idx, sizeof(idx));
		mVB->SetName(L"CubeVB");
		mIB->SetName(L"CubeIB");

		mVbv = { mVB->GetGPUVirtualAddress(), sizeof(verts), sizeof(Vertex) };
		mIbv = { mIB->GetGPUVirtualAddress(), sizeof(idx), DXGI_FORMAT_R16_UINT };
	}
	
	void SceneRenderer::Render(ID3D12GraphicsCommandList* cmd, float aspect, float time)
	{
		XMMATRIX world = XMMatrixRotationY(time) * XMMatrixRotationX(time * 0.5f);
		XMMATRIX view = XMMatrixLookAtLH(XMVectorSet(0, 2, -5, 1), XMVectorZero(), XMVectorSet(0, 1, 0, 0));
		XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);

		XMFLOAT4X4 mvp;
		XMStoreFloat4x4(&mvp, XMMatrixTranspose(world * view * proj));

		cmd->SetGraphicsRootSignature(mRootSig.Get());
		cmd->SetPipelineState(mPso.Get());
		cmd->SetGraphicsRoot32BitConstants(0, 16, &mvp, 0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &mVbv);
		cmd->IASetIndexBuffer(&mIbv);
		cmd->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
	}
	
	
	void SceneRenderer::Shutdown()
	{
		mVB.Reset(); mIB.Reset(); mPso.Reset(); mRootSig.Reset();
	}
}
