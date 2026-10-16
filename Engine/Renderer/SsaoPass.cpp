#include "SsaoPass.h"
#include "Renderer\Renderer.h"
#include "Renderer\FrameGraph.h"
#include "Scene\Camera.h"
#include "RHI/Shader.h"
#include <cmath>
#include <vector>

namespace baek
{
	void SsaoPass::Init(Renderer& renderer)
	{
		mDevice = renderer.GetDevice().Get();
		mSrvHeap = &renderer.SrvHeap();
		mRelease = &renderer.ReleaseQueue();

		// Bloom과 같은 배치: b0 상수 8개, t0, t1, u0 (샘플러 업음: 전부 Load)
		CD3DX12_DESCRIPTOR_RANGE r0(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		CD3DX12_DESCRIPTOR_RANGE r1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		CD3DX12_DESCRIPTOR_RANGE r2(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		CD3DX12_ROOT_PARAMETER params[4];
		params[0].InitAsConstants(8, 0);
		params[1].InitAsDescriptorTable(1, &r0);
		params[2].InitAsDescriptorTable(1, &r1);
		params[3].InitAsDescriptorTable(1, &r2);

		CD3DX12_ROOT_SIGNATURE_DESC desc(4, params, 0, nullptr);
		ComPtr<ID3DBlob> sig, err;
		HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, sig.GetAddressOf(), err.GetAddressOf());
		if(err)
			OutputDebugStringA((const char*)err->GetBufferPointer());
		ThrowIfFailed(hr);
		mDevice->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&mRootSig));

		auto makePso = [&](const char* entry, ComPtr<ID3D12PipelineState>& pso)
		{
			auto cs = CompileShader(ShaderPath(L"Ssao.hlsl"), entry, "cs_5_0");
			D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
			d.pRootSignature = mRootSig.Get();
			d.CS = CD3DX12_SHADER_BYTECODE(cs.Get());
			ThrowIfFailed(mDevice->CreateComputePipelineState(&d, IID_PPV_ARGS(&pso)));
		};
		makePso("CSSsao", mSsaoPso);
		makePso("CSBlur", mBlurPso);
	}
	void SsaoPass::Resize(UINT w, UINT h)
	{
		if (w == 0 || h == 0 || (mRaw && w == mW && h == mH)) return;
		Retire();
		mW = w; mH = h;
		Create(mRaw, mRawSrv, mRawUav, L"SsaoRaw");
		Create(mBlur, mBlurSrv, mBlurUav, L"SsaoBlur");
	}
	void SsaoPass::AddPasses(FrameGraph& g, GpuResource& normalDepth, D3D12_GPU_DESCRIPTOR_HANDLE nd, const Camera& camera, const SsaoSettings& s)
	{
		using U = ResourceUse;
		const float tanY = tanf(camera.FovY() * 0.5f);
		const Constants c{ tanY * camera.Aspect(), tanY, s.radius, s.bias, s.power, {} };

		g.AddPass("SSAO", { U::Srv(normalDepth), U::Uav(mRaw) },
			[this, nd, c](ID3D12GraphicsCommandList* cmd)
			{ Dispatch(cmd, mSsaoPso.Get(), nd, nd, mRawUav.gpu, c); });

		g.AddPass("SSAO Blur", { U::Srv(normalDepth), U::Srv(mRaw), U::Uav(mBlur) },
			[this, nd, c](ID3D12GraphicsCommandList* cmd)
			{ Dispatch(cmd, mBlurPso.Get(), nd, mRawSrv.gpu, mBlurUav.gpu, c); });
	}
	void SsaoPass::Shutdown()
	{
		Retire();
		mSsaoPso.Reset(); mBlurPso.Reset(); mRootSig.Reset();
	}

	void SsaoPass::Retire()
	{
		mRelease->Release(mRaw.Detach());
		mRelease->Release(mBlur.Detach());

		std::vector<DescriptorHandle> old;
		for (DescriptorHandle* h : { &mRawSrv, &mRawUav, &mBlurSrv, &mBlurUav })
		{
			if (h->cpu.ptr) old.push_back(*h);
			*h = {};
		}
		if (!old.empty())
		{
			DescriptorHeap* heap = mSrvHeap;
			mRelease->Defer([heap, old]() mutable { for (auto& h : old) heap->Free(h); });
		}
	}
	void SsaoPass::Create(GpuResource& tex, DescriptorHandle& srv, DescriptorHandle& uav, const wchar_t* name)
	{
		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(kFormat, mW, mH, 1, 1, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		ComPtr<ID3D12Resource> res;
		ThrowIfFailed(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			kShaderReadState, nullptr, IID_PPV_ARGS(&res)));
		res->SetName(name);
		tex.Attach(res, kShaderReadState);

		srv = mSrvHeap->Allocate();
		mDevice->CreateShaderResourceView(tex.Get(), nullptr, srv.cpu);
		uav = mSrvHeap->Allocate();
		mDevice->CreateUnorderedAccessView(tex.Get(), nullptr, nullptr, uav.cpu);
	}
	void SsaoPass::Dispatch(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso,
		D3D12_GPU_DESCRIPTOR_HANDLE t0, D3D12_GPU_DESCRIPTOR_HANDLE t1,
		D3D12_GPU_DESCRIPTOR_HANDLE u0, const Constants& c)
	{
		ID3D12DescriptorHeap* heaps[] = { mSrvHeap->Get() };
		cmd->SetDescriptorHeaps(1, heaps);
		cmd->SetComputeRootSignature(mRootSig.Get());
		cmd->SetPipelineState(pso);
		cmd->SetComputeRoot32BitConstants(0, 8, &c, 0);
		cmd->SetComputeRootDescriptorTable(1, t0);
		cmd->SetComputeRootDescriptorTable(2, t1);
		cmd->SetComputeRootDescriptorTable(3, u0);
		cmd->Dispatch((mW + 7) / 8, (mH + 7) / 8, 1);
	}
}