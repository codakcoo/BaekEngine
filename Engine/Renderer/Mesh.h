#pragma once
#include "RHI/DxUtil.h"
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

namespace baek
{
	struct Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
		DirectX::XMFLOAT3 color;
	};

	// 임시: UPLOAD 힙에 직접 올린다 (에셋 시스템에서 DEFAULT 힙 + 복사로 교체)
	ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* device, const void* data, UINT64 size);

	class Mesh
	{
	public:
		void Create(ID3D12Device* device, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices);
		void Draw(ID3D12GraphicsCommandList* cmd) const;
		void Shutdown();

		static Mesh CreateCube(ID3D12Device* device);			// 1x1x1, 원점 중심
		static Mesh CreateSphere(ID3D12Device* device, int slices = 32, int stacks = 16);		// 지름 1, 원점 중심

	private:
		ComPtr<ID3D12Resource> mVB, mIB;
		D3D12_VERTEX_BUFFER_VIEW mVbv{};
		D3D12_INDEX_BUFFER_VIEW mIbv{};
		UINT mIndexCount = 0;
	};
}