#include "IBL.h"
#include "Renderer\Renderer.h"
#include "Renderer\Texture.h"
#include "RHI/Shader.h"

namespace baek
{
	namespace
	{
		constexpr DXGI_FORMAT kCubeFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		constexpr D3D12_RESOURCE_STATES kReadState =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		ComPtr<ID3D12PipelineState> CreateComputePso(ID3D12Device* device, ID3D12RootSignature* rs, const wchar_t* file)
		{
			auto cs = CompileShader(ShaderPath(file), "CSMain", "cs_5_0");

			D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
			desc.pRootSignature = rs;
			desc.CS = CD3DX12_SHADER_BYTECODE(cs.Get());

			ComPtr<ID3D12PipelineState> pso;
			ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso)));
			return pso;
		}
	}

	void IBL::Init(Renderer& renderer, const Texture& equirect, UINT envSize, UINT irradianceSize)
	{
		ID3D12Device* device = renderer.GetDevice().Get();
		mHeap = &renderer.SrvHeap();

		// --- Root signature (모든 IBL 컴퓨트 패스 공용) ---
		// b0 = size,
		// t0 = 입력 텍스처,
		// s0 = 선형 샘플러
		CD3DX12_DESCRIPTOR_RANGE srvRange, uavRange;
		srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER params[3];
		params[0].InitAsConstants(4, 0);
		params[1].InitAsDescriptorTable(1, &srvRange);
		params[2].InitAsDescriptorTable(1, &uavRange);

		CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		CD3DX12_ROOT_SIGNATURE_DESC rsDesc(3, params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

		ComPtr<ID3DBlob> sig, err;
		HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, sig.GetAddressOf(), err.GetAddressOf());
		if(err)
			OutputDebugStringA((const char*)err->GetBufferPointer());
		ThrowIfFailed(hr);
		ThrowIfFailed(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&mRootSig)));

		mEquirectPso = CreateComputePso(device, mRootSig.Get(), L"EquirectToCube.hlsl");
		mIrradiancePso = CreateComputePso(device, mRootSig.Get(), L"Irradiance.hlsl");

		// --- 출력 큐브맵 ---
		CreateCube(device, envSize, kCubeFormat, mEnv, L"IBL_EnvCube");
		CreateCube(device, irradianceSize, kCubeFormat, mIrradiance, L"IBL_Irradiance");

		// --- 계산 (한 번만) ---
		renderer.Immediate([&](ID3D12GraphicsCommandList* cmd)
		{
			ID3D12DescriptorHeap* heaps[] = { mHeap->Get() };
			cmd->SetDescriptorHeaps(1, heaps);
			cmd->SetComputeRootSignature(mRootSig.Get());

			auto dispatch = [&](ID3D12PipelineState* pso, D3D12_GPU_DESCRIPTOR_HANDLE input, const Cube& out)
			{
				const UINT params[4] = { out.size, 0, 0, 0 };
				cmd->SetPipelineState(pso);
				cmd->SetComputeRoot32BitConstants(0, 4, params, 0);
				cmd->SetComputeRootDescriptorTable(1, input);
				cmd->SetComputeRootDescriptorTable(2, out.uav.gpu);

				const UINT groups = (out.size + 7) / 8;			// numthreads(8, 8, 1)
				cmd->Dispatch(groups, groups, 6);				// z = 6면체

				// 쓰기 끝 -> 다음 패스와 렌더링에서 읽을 수 있게
				auto b = CD3DX12_RESOURCE_BARRIER::Transition(out.res.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kReadState);
				cmd->ResourceBarrier(1, &b);
			};

			dispatch(mEquirectPso.Get(), equirect.Srv(), mEnv);				// 파노라마 -> 환경 큐브맵
			dispatch(mIrradiancePso.Get(), mEnv.srv.gpu, mIrradiance);		// 환경 큐브맵 -> irradiance
		});
	}
	void IBL::Shutdown()
	{
		FreeCube(mEnv);
		FreeCube(mIrradiance);
		mEquirectPso.Reset(); mIrradiancePso.Reset(); mRootSig.Reset();
		mHeap = nullptr;
	}

	void IBL::CreateCube(ID3D12Device* device, UINT size, DXGI_FORMAT format, Cube& out, const wchar_t* name)
	{
		out.size = size;

		// 6장짜리 텍스처 배열 = 큐브맵. 컴퓨트 셰이더가 쓸 수 있게 UAV 허용
		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, size, size, 6, 1, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&out.res)));
		out.res->SetName(name);

		// 읽을 때: 큐브맵으로 해석 (방향 벡터로 샘플링)
		D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
		srv.Format = format;
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.TextureCube.MipLevels = 1;
		out.srv = mHeap->Allocate();
		device->CreateShaderResourceView(out.res.Get(), &srv, out.srv.cpu);

		// 쓸 때: 2D 배열로 해석 (면 번호 = 배열 인덱스)
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
		uav.Format = format;
		uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uav.Texture2DArray.ArraySize = 6;				// 육면체
		out.uav = mHeap->Allocate();
		device->CreateUnorderedAccessView(out.res.Get(), nullptr, &uav, out.uav.cpu);
	}
	void IBL::FreeCube(Cube& c)
	{
		c.res.Reset();
		if (mHeap)
		{
			mHeap->Free(c.srv);
			mHeap->Free(c.uav);
		}
	}
}
