#include "SkyPass.h"
#include "RHI/Shader.h"
#include "Scene\Camera.h"
#include <DirectXMath.h>

using namespace DirectX;


namespace baek
{
	void SkyPass::Init(ID3D12Device* device, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
	{
		// b0 = invViewProj + intensity (root constants 20개)
		// t0 = env map,
		// s0 = sampler
		CD3DX12_DESCRIPTOR_RANGE range;
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER params[2];
		params[0].InitAsConstants(20, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		params[1].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);

		// 가로는 반복(파라미터 이음매), 세로는 극점에서 고정
		CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		CD3DX12_ROOT_SIGNATURE_DESC rsDesc(2, params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

		ComPtr<ID3DBlob> sig, err;
		HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, sig.GetAddressOf(), err.GetAddressOf());
		if(err)
			OutputDebugStringA((const char*)err->GetBufferPointer());
		ThrowIfFailed(hr);
		ThrowIfFailed(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&mRootSig)));

		auto vs = CompileShader(ShaderPath(L"Sky.hlsl"), "VSMain", "vs_5_0");
		auto ps = CompileShader(ShaderPath(L"Sky.hlsl"), "PSMain", "ps_5_0");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = mRootSig.Get();
		pso.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		pso.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		pso.InputLayout = { nullptr, 0 };
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

		// 깊이: 이미 그려진 물체 뒤(= 아무것도 없는 픽셀)에만 그린다. 깊이는 쓰지 않음
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = rtvFormat;
		pso.DSVFormat = dsvFormat;
		pso.SampleDesc.Count = 1;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&mPso)));
	}
	
	void SkyPass::Render(ID3D12GraphicsCommandList* cmd, ID3D12DescriptorHeap* srvHeap, D3D12_GPU_DESCRIPTOR_HANDLE envSrv, const Camera& camera, float intensity)
	{
		// 카메라 이동은 무시하고 회전만 반영 (하늘은 무한히 멀리 있으므로)
		XMMATRIX view = camera.View();
		view.r[3] = XMVectorSet(0, 0, 0, 1);
		const XMMATRIX invVP = XMMatrixInverse(nullptr, view * camera.Proj());

		struct
		{
			XMFLOAT4X4 invWorldProj;
			float intensity;
			float pad[3];
		} cb{};
		XMStoreFloat4x4(&cb.invWorldProj, XMMatrixTranspose(invVP));
		cb.intensity = intensity;

		ID3D12DescriptorHeap* heaps[] = { srvHeap };
		cmd->SetDescriptorHeaps(1, heaps);
		cmd->SetGraphicsRootSignature(mRootSig.Get());
		cmd->SetPipelineState(mPso.Get());
		cmd->SetGraphicsRoot32BitConstants(0, 20, &cb, 0);
		cmd->SetGraphicsRootDescriptorTable(1, envSrv);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->DrawInstanced(3, 1, 0, 0);
	}
	
	void SkyPass::Shutdown()
	{
		mPso.Reset(); mRootSig.Reset();
	}
}
