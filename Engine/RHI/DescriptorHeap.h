#pragma once
#include "RHI/DxUtil.h"
#include <vector>
#include <cstdint>

namespace baek
{
	struct DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
		uint32_t					index = UINT32_MAX;
		bool IsValid() const { return index != UINT32_MAX; }
	};

	// 고정 크기 힙 + free list. (GPU가 아직 쓰는 슬롯의 지연 해제는 나중에)
	class DescriptorHeap
	{
	public:
		void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible);

		DescriptorHandle	Allocate();
		void				Free(const DescriptorHandle& h) { FreeIndex(h.index); }
		void				Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu);					// ImGui 콜백용
		DescriptorHandle	At(uint32_t index) const;

		ID3D12DescriptorHeap* Get() const { return mHeap.Get(); }

	private:
		void FreeIndex(uint32_t index);

		ComPtr<ID3D12DescriptorHeap> mHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE mCpuStart{};
		D3D12_GPU_DESCRIPTOR_HANDLE mGpuStart{};
		uint32_t					mIncrement = 0;
		uint32_t					mCapacity = 0;
		bool						mShaderVisible = false;
		std::vector<uint32_t>		mFreeList;
	};
}