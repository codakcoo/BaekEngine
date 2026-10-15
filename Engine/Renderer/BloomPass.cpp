#include "BloomPass.h"
#include "Renderer\Renderer.h"
#include "Renderer\FrameGraph.h"
#include "RHI/Shader.h"
#include <string>

namespace baek
{
	struct BloomConstants
	{
		float srcTexel[2];
		float threshold, knee, radius;
		float pad[3];
	};
	static_assert(sizeof(BloomConstants) == 8 * 4, "root constants = 8 DWORDs");

	void BloomPass::Init(Renderer& renderer)
	{
		mDevice = renderer.GetDevice().Get();
		mSrvHeap = &renderer.SrvHeap();
		mRelease = &renderer.ReleaseQueue();

		// b0: 상수 8개, t0/t1/u0 : 각각 디스크립터 1개, s0: 선형 클램프
		CD3DX12_DESCRIPTOR_RANGE r0(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		CD3DX12_DESCRIPTOR_RANGE r1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		CD3DX12_DESCRIPTOR_RANGE r2(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		CD3DX12_ROOT_PARAMETER params[4];
		params[0].InitAsConstants(8, 0);
		params[1].InitAsDescriptorTable(1, &r0);
		params[2].InitAsDescriptorTable(1, &r1);
		params[3].InitAsDescriptorTable(1, &r2);

		CD3DX12_STATIC_SAMPLER_DESC linear(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		CD3DX12_ROOT_SIGNATURE_DESC desc(4, params, 1, &linear);
		ComPtr<ID3DBlob> sig, err;
		HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, sig.GetAddressOf(), err.GetAddressOf());
		if(err)
			OutputDebugStringA((const char*)err->GetBufferPointer());
		ThrowIfFailed(hr);
		ThrowIfFailed(mDevice->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&mRootSig)));

		auto makePso = [&](const char* entry, ComPtr<ID3D12PipelineState>& pso)
		{
			auto cs = CompileShader(ShaderPath(L"Bloom.hlsl"), entry, "cs_5_0");
			D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
			d.pRootSignature = mRootSig.Get();
			d.CS = CD3DX12_SHADER_BYTECODE(cs.Get());
			ThrowIfFailed(mDevice->CreateComputePipelineState(&d, IID_PPV_ARGS(&pso)));
		};
		makePso("CSPrefilter", mPrefilterPso);
		makePso("CSDownsample", mDownPso);
		makePso("CSUpsample", mUpPso);
	}

	void BloomPass::Resize(UINT sceneW, UINT sceneH)
	{
		const UINT w = std::max(1u, sceneW / 2), h = std::max(1u, sceneH / 2);
		if (mDown && w == mW && h == mH) { mSrcW = sceneW; mSrcH = sceneH; return; }

		Retire();
		mSrcW = sceneW; mSrcH = sceneH;
		mW = w; mH = h;

		// 가장 작은 밉이 8px 밑으로 내려가지 않게
		mMips = 1;
		while (mMips < kMaxMips && (w >> mMips) >= 8 && (h >> mMips) >= 8) ++mMips;
		mMips = std::max(mMips, 2u);

		CreateChain(mDown, mDownSrv, mDownUav, L"BloomDown");
		CreateChain(mUp, mUpSrv, mUpUav, L"BloomUp");
	}

	void BloomPass::AddPasses(FrameGraph& g, GpuResource& scene, D3D12_GPU_DESCRIPTOR_HANDLE sceneSrv, const BloomSettings& s)
	{
		using U = ResourceUse;
		const UINT N = mMips;

		// 1) Prefilter: scene(원본) -> Down[0](1/2)
		g.AddPass("Bloom Prefilter", { U::Srv(scene), U::Uav(mDown, 0) },
			[this, sceneSrv, s](ID3D12GraphicsCommandList* c)
			{
				Dispatch(c, mPrefilterPso.Get(), sceneSrv, sceneSrv, mDownUav[0].gpu, mSrcW, mSrcH, MipW(0), MipH(0), s);
			});

		// 2) Down[i-1] -> Down[i]
		for (UINT i = 0; i < N; ++i)
		{
			g.AddPass("Bloom Down" + std::to_string(i), {U::Srv(mDown, i - 1), U::Uav(mDown, i)},
				[this, i, s](ID3D12GraphicsCommandList* c)
				{
					Dispatch(c, mDownPso.Get(), mDownSrv[i-1].gpu, mDownSrv[i - 1].gpu, mDownUav[i].gpu,
						MipW(i-1), MipH(i-1), MipW(i), MipH(i), s);
				});
		}

		// 3) Up[i-1] = Down[i-1] + Tent(작은 단계). 가장 작은 단계는 Down[N-1]에서 시작
		for (UINT i = N - 1; i >= 1; --i)
		{
			const bool first = (i == N - 1);
			GpuResource& low = first ? mDown : mUp;
			const D3D12_GPU_DESCRIPTOR_HANDLE lowSrv = first ? mDownSrv[i].gpu : mUpSrv[i].gpu;
			g.AddPass("Bloom Up" + std::to_string(i-1), { U::Srv(low, i), U::Uav(mDown, i-1), U::Uav(mUp, i-1)},
				[this, i, lowSrv, s](ID3D12GraphicsCommandList* c)
				{
					Dispatch(c, mUpPso.Get(), lowSrv, mDownSrv[i - 1].gpu, mUpUav[i].gpu,
						MipW(i), MipH(i), MipW(i - 1), MipH(i - 1), s);
				});
		}
	}
	void BloomPass::Shutdown()
	{
		Retire();								// Renderer::Shutdown의 Flush에서 실제 해제
		mPrefilterPso.Reset(); mDownPso.Reset(); mUpPso.Reset();
		mRootSig.Reset();
	}

	void BloomPass::Retire()
	{
		mRelease->Release(mDown.Detach());
		mRelease->Release(mUp.Detach());

		// SRV/UAV 모두 shader-visible 힙이라 지연 해제
		std::vector<DescriptorHandle> old;
		for (auto* v : { &mDownSrv, &mDownUav, &mUpSrv, &mUpUav })
		{
			old.insert(old.end(), v->begin(), v->end());
			v->clear();
		}
		if (!old.empty())
		{
			DescriptorHeap* heap = mSrvHeap;
			mRelease->Defer([heap, old]() mutable
			{
				for(auto& h : old)
					heap->Free(h);
			});
		}
	}
	void BloomPass::CreateChain(GpuResource& tex, std::vector<DescriptorHandle>& srv, std::vector<DescriptorHandle>& uav, const wchar_t* name)
	{
		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(kFormat, mW, mH, 1, (UINT16)mMips, 1, 0, 
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		ComPtr<ID3D12Resource> res;
		ThrowIfFailed(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, kShaderReadState, nullptr, IID_PPV_ARGS(&res)));
		res->SetName(name);
		tex.Attach(res, kShaderReadState);

		srv.resize(mMips);
		uav.resize(mMips);
		for (UINT i = 0; i < mMips; ++i)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC s{};
			s.Format = kFormat;
			s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			s.Texture2D.MostDetailedMip = i;				// 이 밉 하나만 보이는 뷰
			s.Texture2D.MipLevels = 1;
			srv[i] = mSrvHeap->Allocate();
			mDevice->CreateShaderResourceView(tex.Get(), &s, srv[i].cpu);

			D3D12_UNORDERED_ACCESS_VIEW_DESC u{};
			u.Format = kFormat;
			u.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			u.Texture2D.MipSlice = i;
			uav[i] = mSrvHeap->Allocate();
			mDevice->CreateUnorderedAccessView(tex.Get(), nullptr, &u, uav[i].cpu);
		}
	}
	void BloomPass::Dispatch(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso,
		D3D12_GPU_DESCRIPTOR_HANDLE src, D3D12_GPU_DESCRIPTOR_HANDLE high,
		D3D12_GPU_DESCRIPTOR_HANDLE out,
		UINT srcW, UINT srcH, UINT outW, UINT outH, const BloomSettings& s)
	{
		ID3D12DescriptorHeap* heaps[] = { mSrvHeap->Get() };
		cmd->SetDescriptorHeaps(1, heaps);
		cmd->SetComputeRootSignature(mRootSig.Get());
		cmd->SetPipelineState(pso);

		BloomConstants c{ {1.0f/srcW, 1.0f/srcH}, s.threshold, s.knee, s.radius, {} };
		cmd->SetComputeRoot32BitConstants(0, 8, &c, 0);
		cmd->SetComputeRootDescriptorTable(1, src);
		cmd->SetComputeRootDescriptorTable(2, high);
		cmd->SetComputeRootDescriptorTable(3, out);
		cmd->Dispatch((outW + 7)/8, (outH + 7)/8, 1);
	}
}