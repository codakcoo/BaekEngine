#include "Renderer/SceneRenderer.h"
#include "RHI/Shader.h"
#include "Renderer/Mesh.h"
#include "Scene/Scene.h"
#include <DirectXMath.h>
#include <vector>

using namespace DirectX;


namespace baek
{
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
	
	void SceneRenderer::Render(ID3D12GraphicsCommandList* cmd, const DirectX::XMMATRIX& viewProj, const Scene& scene)
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

		// --- Entities ---
		cmd->SetPipelineState(mPso.Get());
		for (const Entity& e : scene.Entities())
		{
			if (!e.visible || !e.mesh) continue;
			setMvp(e.transform.Matrix());
			e.mesh->Draw(cmd);
		}
	}
	
	
	void SceneRenderer::Shutdown()
	{
		mPso.Reset(); mLinePSO.Reset(); mRootSig.Reset();
	}
}
