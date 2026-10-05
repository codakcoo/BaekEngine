#include "Texture.h"
#include "Renderer/Renderer.h"
#include <stdexcept>

#pragma warning(push, 0)          // 서드파티 헤더 경고 끄기
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#pragma warning(pop)S

namespace baek
{
	void Texture::CreateFromPixels(Renderer& renderer, const uint8_t* rgba, UINT width, UINT height, bool srgb)
	{
		ID3D12Device* device = renderer.GetDevice().Get();
		mWidth = width; mHeight = height;

		const DXGI_FORMAT format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);

		// 1) 최종 텍스처 (DEFAULT 힙, 복사 대상 상태로 시작)
		CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mTex)));

		// 2) 업로드 버퍼: GPU가 요구하는 행 간격(RowPitch)을 조회
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
		UINT64 uploadBytes = 0;
		device->GetCopyableFootprints(&desc, 0, 1, 0, &fp, nullptr, nullptr, &uploadBytes);

		CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
		auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBytes);
		ComPtr<ID3D12Resource> upload;
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)));

		// 3) 한 줄씩 복사 (RowPitch가 width * 4보다 클 수 있어서 통째로 memcpy하면 안 됨)
		uint8_t* dst = nullptr;
		CD3DX12_RANGE noRead(0, 0);
		ThrowIfFailed(upload->Map(0, &noRead, reinterpret_cast<void**>(&dst)));
		for(UINT y = 0; y < height; ++y)
			memcpy(dst + fp.Offset + (size_t)y * fp.Footprint.RowPitch, rgba + (size_t)y * width * 4, (size_t)width * 4);
		upload->Unmap(0, nullptr);

		// 4) 업로드 버퍼 -> 텍스처 복사, 셰이더에서 읽을 수 있는 상태로 전환
		renderer.Immediate([&](ID3D12GraphicsCommandList* cmd)
		{
			CD3DX12_TEXTURE_COPY_LOCATION dstLoc(mTex.Get(), 0);
			CD3DX12_TEXTURE_COPY_LOCATION srvLoc(upload.Get(), fp);
			cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srvLoc, nullptr);

			auto b = CD3DX12_RESOURCE_BARRIER::Transition(mTex.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			cmd->ResourceBarrier(1, &b);
		});

		// 5) SRV
		mSrvHeap = &renderer.SrvHeap();
		mSrv = mSrvHeap->Allocate();
		device->CreateShaderResourceView(mTex.Get(), nullptr, mSrv.cpu);
	}

	void Texture::LoadFromFile(Renderer& renderer, const std::string& path, bool srgb)
	{
		int w = 0, h = 0, comp = 0;
		stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &comp, 4);			// 항상 RGBA 4채널로 변환
		if (!pixels)
		{
			throw std::runtime_error("Texture load failed: " + path + " (" + stbi_failure_reason() + ")");

			CreateFromPixels(renderer, pixels, (UINT)w, (UINT)h, srgb);
			stbi_image_free(pixels);
		}
	}

	void Texture::Shutdown()
	{
		mTex.Reset();
		if(mSrvHeap) mSrvHeap->Free(mSrv);
		mSrvHeap = nullptr;
	}
}
