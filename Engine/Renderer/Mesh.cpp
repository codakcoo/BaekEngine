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
		using namespace DirectX;
		const float h = 0.5f;
		const XMFLOAT3 white{ 1, 1, 1 };

		std::vector<Vertex> verts;
		std::vector<uint16_t> idx;

		// 정점 4개는 바깥에서 봤을 때 시계 방향 (LH 기준 앞면)
		auto face = [&](XMFLOAT3 n, XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c, XMFLOAT3 d)
		{
			const int base = (int)verts.size();
			for(const XMFLOAT3& p : {a, b, c, d})
				verts.push_back({p, n, white});
			for(int o : { 0, 1, 2, 0, 2 , 3})
				idx.push_back((uint16_t)(base+o));
		};

		face({ 0, 0,-1 }, { -h,-h,-h }, { -h, h,-h }, { h, h,-h }, { h,-h,-h });   // front
		face({ 0, 0, 1 }, { -h,-h, h }, { h,-h, h }, { h, h, h }, { -h, h, h });   // back
		face({ 0, 1, 0 }, { -h, h,-h }, { -h, h, h }, { h, h, h }, { h, h,-h });   // top
		face({ 0,-1, 0 }, { -h,-h,-h }, { h,-h,-h }, { h,-h, h }, { -h,-h, h });   // bottom
		face({ -1, 0, 0 }, { -h,-h, h }, { -h, h, h }, { -h, h,-h }, { -h,-h,-h });   // left
		face({ 1, 0, 0 }, { h,-h,-h }, { h, h,-h }, { h, h, h }, { h,-h, h });   // right

		Mesh m;
		m.Create(device, verts, idx);
		return m;
	}
}
