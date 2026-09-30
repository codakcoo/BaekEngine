#pragma once
#include "RHI/DxUtil.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>

namespace baek
{
	class Device
	{
	public:
		void Init(bool enableDebug);
		void Shutdown();

		ID3D12Device* Get() const			{ return mDevice.Get(); }
		IDXGIFactory6* Factory() const		{ return mFactory.Get(); }
		ID3D12CommandQueue* Queue() const	{ return mQueue.Get(); }

		uint64_t	Signal();								// 큐에 ++fence 시그널, 값 반환
		void		WaitForValue(uint64_t value);			// CPu 대기
		void		Flush() { WaitForValue(Signal()); }

	private:
		ComPtr<IDXGIFactory6>		mFactory;
		ComPtr<ID3D12Device>		mDevice;
		ComPtr<ID3D12CommandQueue>	mQueue;
		ComPtr<ID3D12Fence>			mFence;
		uint64_t					mFenceValue = 0;
		HANDLE						mFenceEvent = nullptr;
	};
}