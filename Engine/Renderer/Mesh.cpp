#include "Mesh.h"

namespace baek
{
	ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* device, const void* data, UINT64 size)
	{
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
		ComPtr<ID3D12Resource> buf;
		ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)));

		void* p = nullptr;
		CD3DX12_RANGE noRead(0, 0);
		ThrowIfFailed(buf->Map(0, &noRead, &p));
		memcpy(p, data, size);
		buf->Unmap(0, nullptr);
		return buf;
	}

	void Mesh::Create(ID3D12Device* device, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices)
	{
		const UINT vbBytes = (UINT)(vertices.size() * sizeof(Vertex));
		const UINT ibBytes = (UINT)(indices.size() * sizeof(uint16_t));
		mIndexCount = (UINT)indices.size();

		mVB = CreateUploadBuffer(device, vertices.data(), vbBytes);
		mIB = CreateUploadBuffer(device, indices.data(), ibBytes);

		mVbv = { mVB->GetGPUVirtualAddress(), vbBytes, sizeof(Vertex) };
		mIbv = { mIB->GetGPUVirtualAddress(), ibBytes, DXGI_FORMAT_R16_UINT };
	}

	void Mesh::Draw(ID3D12GraphicsCommandList* cmd) const
	{
		cmd->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &mVbv);
		cmd->IASetIndexBuffer(&mIbv);
		cmd->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
	}

	void Mesh::Shutdown()
	{
		mVB.Reset(); mIB.Reset();
	}

	Mesh Mesh::CreateCube(ID3D12Device* device)
	{
		const float h = 0.5f;
		const std::vector<Vertex> verts =
		{
			{ {-h,-h,-h}, {1,1,1} }, { {-h, h,-h}, {0,0,0} },
			{ { h, h,-h}, {1,0,0} }, { { h,-h,-h}, {0,1,0} },
			{ {-h,-h, h}, {0,0,1} }, { {-h, h, h}, {1,1,0} },
			{ { h, h, h}, {0,1,1} }, { { h,-h, h}, {1,0,1} },
		};
		const std::vector<uint16_t> idx =
		{
			0,1,2, 0,2,3,   4,6,5, 4,7,6,   4,5,1, 4,1,0,
			3,2,6, 3,6,7,   1,5,6, 1,6,2,   4,0,3, 4,3,7,
		};

		Mesh m;
		m.Create(device, verts, idx);
		return m;
	}
}
