#include "RHI/DescriptorHeap.h"

namespace baek
{
	void DescriptorHeap::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible)
	{
		D3D12_DESCRIPTOR_HEAP_DESC d = {};
		d.Type = type;
		d.NumDescriptors = capacity;
		d.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&mHeap)));

		mCpuStart = mHeap->GetCPUDescriptorHandleForHeapStart();
		if (shaderVisible) mGpuStart = mHeap->GetGPUDescriptorHandleForHeapStart();
		mIncrement = device->GetDescriptorHandleIncrementSize(type);
		mCapacity = capacity;
		mShaderVisible = shaderVisible;

		mFreeList.resize(capacity);
		for(uint32_t i = 0; i < capacity; ++i)
			mFreeList[i] = capacity - 1 - i;						// pop_back 시 0번부터 나오도록
	}

	DescriptorHandle DescriptorHeap::Allocate()
	{
		if(mFreeList.empty())
			throw std::runtime_error("DescriptorHeap: 슬롯 부족");

		uint32_t idx = mFreeList.back();
		mFreeList.pop_back();
		return At(idx);
	}

	void DescriptorHeap::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu)
	{
		FreeIndex((uint32_t)((cpu.ptr - mCpuStart.ptr) / mIncrement));
	}
	DescriptorHandle DescriptorHeap::At(uint32_t index) const
	{
		DescriptorHandle h;
		h.index = index;
		h.cpu.ptr = mCpuStart.ptr + (SIZE_T)index * mIncrement;
		if (mShaderVisible) h.gpu.ptr = mGpuStart.ptr + (UINT64)index * mIncrement;
		return h;
	}
	void DescriptorHeap::FreeIndex(uint32_t index)
	{
		if(index < mCapacity) mFreeList.push_back(index);
	}
}
