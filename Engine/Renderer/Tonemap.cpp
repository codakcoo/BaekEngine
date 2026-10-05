#include "Tonemap.h"
#include "RHI/Shader.h"

namespace baek
{
	void TonemapPass::Init(ID3D12Device* device, DXGI_FORMAT outputFormat)
	{
		// t0 = scene texture (desciptor table), b0 = params (root constants), s0 = static sampler
		CD3DX12_DESCRIPTOR_RANGE range;
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER params[2];
		params[0].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);
		params[1].InitAsConstants(4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		CD3DX12_ROOT_SIGNATURE_DESC rsDesc(2, params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

		ComPtr<ID3DBlob> sig, err;
		HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, sig.GetAddressOf(), err.GetAddressOf());
		if(err)
			OutputDebugStringA((const char*)err->GetBufferPointer());
		ThrowIfFailed(hr);
		ThrowIfFailed(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&mRootSig)));

		auto vs = CompileShader(ShaderPath(L"Tonemap.hlsl"), "VSMain", "vs_5_0");
		auto ps = CompileShader(ShaderPath(L"Tonemap.hlsl"), "PSMain", "ps_5_0");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = mRootSig.Get();
		pso.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		pso.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		pso.InputLayout = { nullptr, 0 };								// 버텍스 버퍼 없음
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthEnable = FALSE;						// 깊이 테스트 안 함
		pso.DepthStencilState.StencilEnable = FALSE;
		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = outputFormat;
		pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
		pso.SampleDesc.Count = 1;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&mPso)));
	}

	void TonemapPass::Render(ID3D12GraphicsCommandList* cmd, ID3D12DescriptorHeap* srvHeap, D3D12_GPU_DESCRIPTOR_HANDLE sceneSrv, float exposure)
	{
		ID3D12DescriptorHeap* heaps[] = { srvHeap };
		cmd->SetDescriptorHeaps(1, heaps);

		cmd->SetGraphicsRootSignature(mRootSig.Get());
		cmd->SetPipelineState(mPso.Get());
		cmd->SetGraphicsRootDescriptorTable(0, sceneSrv);

		const float params[4] = { exposure, 0.0f, 0.0f, 0.0f };
		cmd->SetGraphicsRoot32BitConstants(1, 4, params, 0);

		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->DrawInstanced(3, 1, 0, 0);
	}

	void TonemapPass::Shutdown()
	{
		mPso.Reset(); mRootSig.Reset();
	}
}
