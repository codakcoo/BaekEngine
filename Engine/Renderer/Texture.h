#pragma once
#include "RHI/DxUtil.h"
#include "RHI/DescriptorHeap.h"
#include <string>
#include <cstdint>

namespace baek
{
	class Renderer;

	// 주의: 복사 금지 (SRV 슬롯을 소유함). 참조나 포인터로 넘길 것
	class Texture
	{
	public:
		// rgba: width * height * 4 바이트. srgb = true면 샘플링 시 자동으로 선형 변환됨(색상 텍스처용)
		void CreateFromPixels(Renderer& renderer, const uint8_t* rgba, UINT width, UINT height, bool srgb);
		void LoadFromFile(Renderer& renderer, const std::string& path, bool srgb);		// PNG / JPG / TGA / BMP
		void LoadHDR(Renderer& renderer, const std::string& path);						// .hdr -> R32G32B32A32
		void LoadFromMemory(Renderer& renderer, const uint8_t* data, size_t size, bool srgb);
		void Shutdown();

		D3D12_GPU_DESCRIPTOR_HANDLE Srv() const { return mSrv.gpu; }
		UINT Width() const { return mWidth; }
		UINT Height() const { return mHeight; }

	private:
		struct MipData
		{
			UINT width = 0, height = 0;
			const uint8_t* pixels = nullptr;
		};

		void CreateInternal(Renderer& renderer, DXGI_FORMAT format, UINT bytesPerPixel, const std::vector<MipData>& mips);

	// Propertys
	private:
		ComPtr<ID3D12Resource> mTex;
		DescriptorHeap* mSrvHeap = nullptr;
		DescriptorHandle mSrv{};
		UINT mWidth = 0, mHeight = 0;
	};
}