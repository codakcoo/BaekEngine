#include "RHI/UploadRing.h"
#include <stdexcept>

namespace baek
{
	void UploadRing::Init(ID3D12Device* device, UINT64 capacity)
	{
		mCapacity = capacity;
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(capacity);
		ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, 
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mBuffer)));

		CD3DX12_RANGE noRead(0, 0);
		ThrowIfFailed(mBuffer->Map(0, &noRead, reinterpret_cast<void**>(&mMapped)));
	}
	
	D3D12_GPU_VIRTUAL_ADDRESS UploadRing::Alloc(const void* data, UINT size)
	{
		const UINT64 aligned = (size + 255ull) & ~255ull;			// 상수 버퍼는 256바이트 단위
		if(mOffset + aligned > mCapacity)
			throw std::runtime_error("UploadRing overflow");

		memcpy(mMapped + mOffset, data, size);
		const D3D12_GPU_VIRTUAL_ADDRESS addr = mBuffer->GetGPUVirtualAddress() + mOffset;
		mOffset += aligned;

		return addr;
	}
	
	void UploadRing::Shutdown()
	{
		if (mBuffer) mBuffer->Unmap(0, nullptr);
		mMapped = nullptr;
		mBuffer.Reset();
	}
}
