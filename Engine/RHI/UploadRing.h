// 프레임마다 상수 버퍼 데이터를 순서대로 잘라 쓰는 업로드 버퍼
#pragma once
#include "RHI/DxUtil.h"
#include <cstdint>

namespace baek
{
	class UploadRing
	{
	public:
		void Init(ID3D12Device* device, UINT64 capacity);
		void Reset() { mOffset = 0; }												// 프레임 시작 시 호출
		D3D12_GPU_VIRTUAL_ADDRESS Alloc(const void* data, UINT size);				// 256바이트 정렬
		void Shutdown();

	private:
		ComPtr<ID3D12Resource> mBuffer;
		uint8_t* mMapped = nullptr;
		UINT64 mCapacity = 0, mOffset = 0;
	};
}