#include "Renderer/SceneRenderer.h"
#include "RHI/Shader.h"
#include <DirectXMath.h>
#include <vector>

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
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{ "COLOR",		0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = mRootSig.Get();
		pso.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		pso.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
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

		auto linePSO = pso;
		linePSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&linePSO, IID_PPV_ARGS(&mLinePSO)));

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

		// --- Grid geometry (XZ plane, 1m spacing, -20...20) ---
		const int half = 20;
		const XMFLOAT3 gray = { 0.35f, 0.35f, 0.35f };
		const XMFLOAT3 red = { 0.85f, 0.20f, 0.20f };			// X axis
		const XMFLOAT3 blue = { 0.20f, 0.35f, 0.95f };			// Z axis

		std::vector<Vertex> grid;
		grid.resize((half * 2 + 1) * 4);
		for (int i = -half; i <= half; ++i)
		{
			const float f = (float)i, e = (float)half;

			const XMFLOAT3& cz = (i == 0) ? blue : gray;			// Z와 평행한 선 (x = i)
			grid.push_back({ { f, 0.0f, -e }, cz });
			grid.push_back({ { f, 0.0f,  e }, cz });

			const XMFLOAT3& cx = (i == 0) ? red : gray;				// x와 평행한 선 (z = i)
			grid.push_back({ { -e, 0.0f, f }, cx });
			grid.push_back({ {  e, 0.0f,  f }, cx });
		}
		mGridVertexCount = (UINT)grid.size();

		const UINT gridByte = (UINT)(grid.size() * sizeof(Vertex));
		mGridVB = CreateUploadBuffer(device, grid.data(), gridByte);
		mGridVB->SetName(L"GridVB");
		mGridVbv = { mGridVB->GetGPUVirtualAddress(), gridByte, sizeof(Vertex) };
	}
	
	void SceneRenderer::Render(ID3D12GraphicsCommandList* cmd, const DirectX::XMMATRIX& viewProj)
	{
		cmd->SetGraphicsRootSignature(mRootSig.Get());

		auto setMvp = [&](const XMMATRIX& world)
		{
			XMFLOAT4X4 mvp;
			XMStoreFloat4x4(&mvp, XMMatrixTranspose(world * viewProj));
			cmd->SetGraphicsRoot32BitConstants(0, 16, &mvp, 0);
		};
		
		// --- Grid ---
		setMvp(XMMatrixIdentity());
		cmd->SetPipelineState(mLinePSO.Get());
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		cmd->IASetVertexBuffers(0, 1, &mGridVbv);
		cmd->DrawInstanced(mGridVertexCount, 1, 0, 0);

		// --- Cube (바닥 위에 올려놓기: y + 1) ---
		setMvp(XMMatrixTranslation(0.0f, 1.0f, 0.0f));
		cmd->SetPipelineState(mPso.Get());
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &mVbv);
		cmd->IASetIndexBuffer(&mIbv);
		cmd->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
	}
	
	
	void SceneRenderer::Shutdown()
	{
		mVB.Reset(); mIB.Reset(); mPso.Reset(); mLinePSO.Reset(); mRootSig.Reset();
	}
}
