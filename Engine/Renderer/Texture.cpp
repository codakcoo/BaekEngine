#include "Texture.h"
#include "Renderer/Renderer.h"
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cmath>

#pragma warning(push, 0)          // 서드파티 헤더 경고 끄기
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#pragma warning(pop)

namespace baek
{

	namespace 
	{
		struct MipLevel
		{
			UINT width = 0, height = 0;
			std::vector<uint8_t> pixels;   // RGBA8
		};

		// sRGB 바이트 -> 선형 float (256개뿐이라 표로 만들어 줌)
		float SrgbToLinear(uint8_t v)
		{
			static float table[256];
			static bool ready = false;
			if (!ready)
			{
				for (int i = 0; i < 256; ++i)
				{
					const float c = i / 255.0f;
					table[i] = (c <= 0.04045f) ? c / 12.92f : powf((c+0.055f) / 1.055f, 2.4f);
				}
				ready = true;
			}

			return table[v];
		}

		uint8_t LinearToSrgb(float c)
		{
			c = std::clamp(c, 0.0f, 1.0f);
			const float s = (c <= 0.0031308f) ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
			return (uint8_t)(s * 255.0f + 0.5f);
		}

		// 2x2 평균으로 절반씩 줄여 1x1까지 만든다
		std::vector<MipLevel> BuildMipChain(const uint8_t* rgba, UINT width, UINT height, bool srgb)
		{
			std::vector<MipLevel> mips;
			mips.push_back({ width, height, std::vector<uint8_t>(rgba, rgba + (size_t)width * height * 4) });

			while (mips.back().width > 1 || mips.back().height > 1)
			{
				const MipLevel& src = mips.back();

				MipLevel dst;
				dst.width = std::max(1u, src.width / 2);
				dst.height = std::max(1u, src.height / 2);
				dst.pixels.resize((size_t)dst.width * dst.height * 4);

				for (UINT y = 0; y < dst.height; ++y)
				{
					// 홀수 크기나 1픽셀 폭일 때 범위를 벗어나지 않게 clamp
					const UINT y0 = std::min(y * 2, src.height - 1);
					const UINT y1 = std::min(y * 2 + 1, src.height - 1);

					for (UINT x = 0; x < dst.width; ++x)
					{
						const UINT x0 = std::min(x*2, src.width - 1);
						const UINT x1 = std::min(x*2+1, src.width - 1);

						const uint8_t* p[4] = 
						{
							&src.pixels[((size_t)y0 * src.width + x0) * 4],
							&src.pixels[((size_t)y0 * src.width + x1) * 4],
							&src.pixels[((size_t)y1 * src.width + x0) * 4],
							&src.pixels[((size_t)y1 * src.width + x1) * 4],
						};
						uint8_t* out = &dst.pixels[((size_t)y * dst.width + x) * 4];

						for (int c = 0; c < 4; ++c)
						{
							if (srgb && c < 3)		// 색상 채널: 선형 공간에서 평균
							{
								const float sum = SrgbToLinear(p[0][c]) + SrgbToLinear(p[1][c])
												+ SrgbToLinear(p[2][c]) + SrgbToLinear(p[3][c]);
								out[c] = LinearToSrgb(sum * 0.25f);
							}
							else                   // 알파, 그리고 데이터 텍스처: 값 그대로 평균
							{
								out[c] = (uint8_t)((p[0][c] + p[1][c] + p[2][c] + p[3][c] + 2) / 4);
							}
						}
					}
				}
				mips.push_back(std::move(dst));
			}
			return mips;
		}
	}

	void Texture::CreateFromPixels(Renderer& renderer, const uint8_t* rgba, UINT width, UINT height, bool srgb)
	{
		const std::vector<MipLevel> chain = BuildMipChain(rgba, width, height, srgb);

		std::vector<MipData> mips;
		for (const MipLevel& m : chain)
			mips.push_back({ m.width, m.height, m.pixels.data()});
		CreateInternal(renderer, srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM, 4, mips);
	}

	void Texture::LoadFromFile(Renderer& renderer, const std::string& path, bool srgb)
	{
		int w = 0, h = 0, comp = 0;
		stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &comp, 4);			// 항상 RGBA 4채널로 변환
		if (!pixels)
			throw std::runtime_error("Texture load failed: " + path + " (" + stbi_failure_reason() + ")");

		CreateFromPixels(renderer, pixels, (UINT)w, (UINT)h, srgb);
		stbi_image_free(pixels);
	}

	void Texture::LoadHDR(Renderer& renderer, const std::string& path)
	{
		int w = 0, h = 0, comp = 0;
		float* pixels = stbi_loadf(path.c_str(), &w, &h, &comp, 4);				// 선형 float RGBA (알파 = 1)
		if(!pixels)
			throw std::runtime_error("HDR load failed: " + path + " (" + stbi_failure_reason() + ")");

		CreateInternal(renderer, DXGI_FORMAT_R32G32B32A32_FLOAT, 16, {{ (UINT)w, (UINT)h, reinterpret_cast<const uint8_t*>(pixels)}});
		stbi_image_free(pixels);
	}

	void Texture::LoadFromMemory(Renderer& renderer, const uint8_t* data, size_t size, bool srgb)
	{
		int w = 0, h = 0, comp = 0;
		stbi_uc* pixels = stbi_load_from_memory(data, (int)size, &w, &h, &comp, 4);
		if(!pixels)
			throw std::runtime_error(std::string("Texture decode failed (") + stbi_failure_reason() + ")");

		CreateFromPixels(renderer, pixels, w, h, srgb);
		stbi_image_free(pixels);
	}

	void Texture::Shutdown()
	{
		mTex.Reset();
		if(mSrvHeap) mSrvHeap->Free(mSrv);
		mSrvHeap = nullptr;
	}

	void Texture::CreateInternal(Renderer& renderer, DXGI_FORMAT format, UINT bytesPerPixel, const std::vector<MipData>& mips)
	{
		ID3D12Device* device = renderer.GetDevice().Get();
		const UINT mipCount = (UINT)mips.size();
		mWidth = mips[0].width; 
		mHeight = mips[0].height;


		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, mWidth, mHeight, 1, (UINT16)mipCount);

		// 1) 최종 텍스처 (DEFAULT 힙, 복사 대상 상태로 시작)
		CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mTex)));

		// 2) 모든 밉 레벨의 배치 정보를 한 번에 조회
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> fps{ mipCount };
		UINT64 uploadBytes = 0;
		device->GetCopyableFootprints(&desc, 0, mipCount, 0, fps.data(), nullptr, nullptr, &uploadBytes);

		CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
		auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBytes);
		ComPtr<ID3D12Resource> upload;
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)));

		// 3) 한 줄씩 복사 (RowPitch가 width * 4보다 클 수 있어서 통째로 memcpy하면 안 됨)
		uint8_t* dst = nullptr;
		CD3DX12_RANGE noRead(0, 0);
		ThrowIfFailed(upload->Map(0, &noRead, reinterpret_cast<void**>(&dst)));
		for (UINT m = 0; m < mipCount; ++m)
		{
			const MipData& mip = mips[m];
			const size_t rowBytes = (size_t)mip.width * bytesPerPixel;
			for (UINT y = 0; y < mip.height; ++y)
				memcpy(dst + fps[m].Offset + (size_t)y * fps[m].Footprint.RowPitch,
					mip.pixels + (size_t)y * rowBytes, rowBytes);
		}
		upload->Unmap(0, nullptr);

		// 4) 레벨마다 복사 명령, 마지막에 전체 상태 전환
		renderer.Immediate([&](ID3D12GraphicsCommandList* cmd)
			{
				for (UINT m = 0; m < mipCount; ++m)
				{
					CD3DX12_TEXTURE_COPY_LOCATION dstLoc(mTex.Get(), m);				// 서브리소스 인덱스 = 밉레벨
					CD3DX12_TEXTURE_COPY_LOCATION srvLoc(upload.Get(), fps[m]);
					cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srvLoc, nullptr);
				}

				auto b = CD3DX12_RESOURCE_BARRIER::Transition(mTex.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);		// 모든 서브리소스
				cmd->ResourceBarrier(1, &b);
			});

		// 5) SRV (desc를 nullptr로 주면 모든 밉 레벨을 포함)
		mSrvHeap = &renderer.SrvHeap();
		mSrv = mSrvHeap->Allocate();
		device->CreateShaderResourceView(mTex.Get(), nullptr, mSrv.cpu);
	}
}
