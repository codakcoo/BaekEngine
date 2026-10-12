#include "Renderer/SceneRenderer.h"
#include "RHI/Shader.h"
#include "Renderer/Mesh.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Camera.h"
#include <DirectXMath.h>
#include <vector>

using namespace DirectX;


namespace baek
{
	struct PerFrameCB
	{
		XMFLOAT4X4 viewProj;
		XMFLOAT3 cameraPos;		float pad0;
		XMFLOAT3 lightDir;		float pad1;
		XMFLOAT3 lightColor;	float ambient;
		XMFLOAT4X4 lightViewProj;
		XMFLOAT4 shadowParams;			
		// x = bias, 
		// y = 1 / shadow map size, 
		// z = normal offset
	};

	struct PerObjectCB
	{
		XMFLOAT4X4 world;
		XMFLOAT4X4 worldInvTranspose;
		XMFLOAT4 baseColor;
		XMFLOAT4 material;			// x = metallic, y = roughness
		XMFLOAT4 emissive;			// rgb = emissive factor
	};

	void SceneRenderer::Init(Renderer& renderer, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
	{
		ID3D12Device* device = renderer.GetDevice().Get();
		mSrvHeap = renderer.SrvHeap().Get();

		const uint8_t whitePixel[4] = { 255, 255, 255, 255 };
		mWhite.CreateFromPixels(renderer, whitePixel, 1, 1, true);

		const uint8_t flatNormal[4] = { 128, 128, 255, 255 };			// (0, 0, 1) = 요철 없음
		mFlatNormal.CreateFromPixels(renderer, flatNormal, 1, 1, false);

		// Root Signature: b0 PerFrame, b1 PerObject, t0 albedo, t1 normal, t2 metallic-roughness, s0 sampler, s1 clamp sampler
		CD3DX12_DESCRIPTOR_RANGE ranges[8];
		for(UINT i = 0; i < 8; ++i)
			ranges[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i);				// t0~t7

		// --- Root Signature: b0 = MVP (root constants 16개) ---
		CD3DX12_ROOT_PARAMETER params[10];
		params[0].InitAsConstantBufferView(0);
		params[1].InitAsConstantBufferView(1);
		for(UINT i = 0; i < 8; ++i)
			params[2+i].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_PIXEL);
		
		CD3DX12_STATIC_SAMPLER_DESC samplers[3] = 
		{
			CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_ANISOTROPIC),												// s0: 일반 텍스처
			CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR,											// s1: LUT
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
			// s2: 그림자 비교용, 범위 밖은 흰색(=그림자 없음)으로 처리
			CD3DX12_STATIC_SAMPLER_DESC(2, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				0.0f, 16, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE),
		};
		CD3DX12_ROOT_SIGNATURE_DESC rsDesc(10, params, 3, samplers, 
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> sig, err;
		HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
		if(err)
			OutputDebugStringA((const char*)err->GetBufferPointer());
		ThrowIfFailed(hr);
		ThrowIfFailed(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&mRootSig)));

		// Shaders
		auto vs =		CompileShader(ShaderPath(L"Scene.hlsl"), "VSMain",		"vs_5_0");
		auto psLit =	CompileShader(ShaderPath(L"Scene.hlsl"), "PSLit",		"ps_5_0");
		auto psUnlit =	CompileShader(ShaderPath(L"Scene.hlsl"), "PSUnlit",		"ps_5_0");


		// Input layout
		D3D12_INPUT_ELEMENT_DESC layout[] = 
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 0,							D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, offsetof(Vertex, normal),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, offsetof(Vertex, tangent),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0, offsetof(Vertex, uv),		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, offsetof(Vertex, color),		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// PSO
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = mRootSig.Get();
		pso.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		pso.PS = CD3DX12_SHADER_BYTECODE(psLit.Get());						// 메시용
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
		linePSO.PS = CD3DX12_SHADER_BYTECODE(psUnlit.Get());				// 그리드용
		linePSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&linePSO, IID_PPV_ARGS(&mLinePSO)));

		auto shadowPso = pso;
		shadowPso.PS = {};													// 픽셀 셰이더 없음 = 깊이만 기록
		shadowPso.NumRenderTargets = 0;
		shadowPso.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
		shadowPso.DSVFormat = ShadowMap::DsvFormat;
		shadowPso.RasterizerState.DepthBias = 1000;							// 그림자 여드름(acne) 방지
		shadowPso.RasterizerState.SlopeScaledDepthBias = 2.0f;				// 비스듬한 면일수록 더 밀어냄
		shadowPso.RasterizerState.DepthBiasClamp = 0.0f;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&shadowPso, IID_PPV_ARGS(&mShadowPso)));

		// 상수 버퍼링 (1MB씩)
		for (auto& cb : mCB) cb.Init(device, 1024 * 1024);

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
			grid.push_back({ { f, 0.0f, -e }, { 0, 1, 0 }, { 1, 0, 0, 1 }, { 0, 0 }, cz});
			grid.push_back({ { f, 0.0f,  e }, { 0, 1, 0 }, { 1, 0, 0, 1 }, { 0, 0 }, cz });

			const XMFLOAT3& cx = (i == 0) ? red : gray;				// x와 평행한 선 (z = i)
			grid.push_back({ { -e, 0.0f, f }, { 0, 1, 0 }, { 1, 0, 0, 1 }, { 0, 0 }, cx });
			grid.push_back({ {  e, 0.0f, f }, { 0, 1, 0 }, { 1, 0, 0, 1 }, { 0, 0 }, cx });
		}
		mGridVertexCount = (UINT)grid.size();

		const UINT gridByte = (UINT)(grid.size() * sizeof(Vertex));
		mGridVB = CreateUploadBuffer(device, grid.data(), gridByte);
		mGridVB->SetName(L"GridVB");
		mGridVbv = { mGridVB->GetGPUVirtualAddress(), gridByte, sizeof(Vertex) };
	}
	
	void SceneRenderer::Render(ID3D12GraphicsCommandList* cmd, UINT frameIndex, const Camera& camera, const Scene& scene)
	{
		UploadRing& cb = mCB[frameIndex];

		cmd->SetGraphicsRootSignature(mRootSig.Get());

		ID3D12DescriptorHeap* heaps[] = { mSrvHeap };
		cmd->SetDescriptorHeaps(1, heaps);

		// --- PerFrame ---
		PerFrameCB pf{};
		XMStoreFloat4x4(&pf.viewProj, XMMatrixTranspose(camera.ViewProj()));
		pf.cameraPos = camera.Position();
		XMStoreFloat3(&pf.lightDir, XMVector3Normalize(XMLoadFloat3(&scene.light.direction)));
		pf.lightColor = { scene.light.color.x * scene.light.intensity,
						  scene.light.color.y * scene.light.intensity, 
						  scene.light.color.z * scene.light.intensity, };
		pf.ambient = scene.light.iblIntensity;
		XMStoreFloat4x4(&pf.lightViewProj, XMMatrixTranspose(XMLoadFloat4x4(&mLightViewProj)));
		pf.shadowParams = { scene.light.shadowBias, mShadowTexel, scene.light.normalOffset, 0.0f };
		cmd->SetGraphicsRootConstantBufferView(0, cb.Alloc(&pf, sizeof(pf)));

		
		// --- PerObject ---
		auto setObject = [&](const XMMATRIX& world, const Material& mat)
		{
			XMMATRIX w = world;
			w.r[3] = XMVectorSet(0, 0, 0, 1);				// 노멸 변환에는 이동 성분이 필요 없음
			const XMMATRIX invT = XMMatrixTranspose(XMMatrixInverse(nullptr, w));

			PerObjectCB po{};
			XMStoreFloat4x4(&po.world,						XMMatrixTranspose(world));
			XMStoreFloat4x4(&po.worldInvTranspose,			XMMatrixTranspose(invT));
			po.baseColor = { mat.baseColor.x, mat.baseColor.y, mat.baseColor.z, 1.0f };
			po.material = { mat.metallic, mat.roughness, 0.0f, 0.0f };
			po.emissive = { mat.emissive.x, mat.emissive.y, mat.emissive.z, 0.0f };
			cmd->SetGraphicsRootConstantBufferView(1, cb.Alloc(&po, sizeof(po)));

			cmd->SetGraphicsRootDescriptorTable(2, (mat.albedoMap ?				mat.albedoMap : &mWhite)->Srv());
			cmd->SetGraphicsRootDescriptorTable(3, (mat.normalMap ?				mat.normalMap : &mFlatNormal)->Srv());
			cmd->SetGraphicsRootDescriptorTable(4, (mat.metallicRoughnessMap ?	mat.metallicRoughnessMap : &mWhite)->Srv());
			cmd->SetGraphicsRootDescriptorTable(5, (mat.emissiveMap ?			mat.emissiveMap : &mWhite)->Srv());
			cmd->SetGraphicsRootDescriptorTable(6, mIrradianceSrv);
			cmd->SetGraphicsRootDescriptorTable(7, mPrefilteredSrv);
			cmd->SetGraphicsRootDescriptorTable(8, mBrdfLutSrv);
			cmd->SetGraphicsRootDescriptorTable(9, mShadowSrv);
		};
		
		// --- Grid ---
		setObject(XMMatrixIdentity(), mDefaultMaterial);				// grid
		cmd->SetPipelineState(mLinePSO.Get());
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		cmd->IASetVertexBuffers(0, 1, &mGridVbv);
		cmd->DrawInstanced(mGridVertexCount, 1, 0, 0);

		// --- Entities ---
		cmd->SetPipelineState(mPso.Get());
		for (const Entity& e : scene.Entities())
		{
			if (!e.visible || !e.mesh) continue;
			setObject(e.transform.Matrix(), e.material ? *e.material : mDefaultMaterial);		// entity
			//cmd->SetGraphicsRootDescriptorTable(2, (e.albedoMap ? e.albedoMap : &mWhite)->Srv());
			e.mesh->Draw(cmd);
		}
	}
	
	
	void SceneRenderer::RenderShadowPass(ID3D12GraphicsCommandList* cmd, UINT frameIndex, const Scene& scene, ShadowMap& shadow)
	{
		UploadRing& cb = mCB[frameIndex];
		cb.Reset();				// 프레임의 첫 패스에서 링을 비운다.

		// --- 빛의 시점: 원점을 중심으로 한 정사영 박스 ---
		const float R = scene.light.shadowRadius;
		const XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&scene.light.direction));
		const XMVECTOR center = XMVectorZero();
		const XMVECTOR eye = center - dir * (R * 2.0f);
		const XMVECTOR up = fabsf(XMVectorGetY(dir)) > 0.99f ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);

		const XMMATRIX lightVP = XMMatrixLookAtLH(eye, center, up) * XMMatrixOrthographicLH(2.0f * R, 2.0f * R, 0.1f, 4.0f * R);
		XMStoreFloat4x4(&mLightViewProj, lightVP);
		mShadowSrv = shadow.Srv();
		mShadowTexel = 1.0f / (float)shadow.Size();

		// -- 깊이만 그리기 ---
		shadow.Begin(cmd);
		cmd->SetGraphicsRootSignature(mRootSig.Get());
		cmd->SetPipelineState(mShadowPso.Get());

		PerFrameCB pf{};
		XMStoreFloat4x4(&pf.viewProj, XMMatrixTranspose(lightVP));			// 정점 셰이더는 그대로, 시점만 빛으로
		cmd->SetGraphicsRootConstantBufferView(0, cb.Alloc(&pf, sizeof(pf)));

		for (const Entity& e : scene.Entities())
		{
			if(!e.visible || !e.mesh) continue;

			PerObjectCB po{};
			XMStoreFloat4x4(&po.world, XMMatrixTranspose(e.transform.Matrix()));
			cmd->SetGraphicsRootConstantBufferView(1, cb.Alloc(&po, sizeof(po)));
			e.mesh->Draw(cmd);
		}
		shadow.End(cmd);
	}

	void SceneRenderer::Shutdown()
	{
		for (auto& cb : mCB) cb.Shutdown();
		mWhite.Shutdown();
		mFlatNormal.Shutdown();
		mPso.Reset(); mLinePSO.Reset(); mRootSig.Reset();
	}
}
