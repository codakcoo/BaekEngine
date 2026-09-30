#include "RHI/Device.h"

namespace baek
{
	void Device::Init(bool enableDebug)
	{
		UINT factoryFlags = 0;
		if (enableDebug)
		{
			ComPtr<ID3D12Debug> debug;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
			{
				debug->EnableDebugLayer();
				factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}
		ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&mFactory)));

		// 고성능 GPU 우선 (노트북 내장/외장 구분)
		ComPtr<IDXGIAdapter1> adapter;
		for (UINT i = 0; mFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice))))
				break;
		}
		if (!mDevice)
		{
			ComPtr<IDXGIAdapter> warp;
			ThrowIfFailed(mFactory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
			ThrowIfFailed(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice)));
		}

		// 디버그 레이어 에러 즉시 중단 → 배리어/디스크립터 실수를 Close() 전에 해당 줄에서 잡음
		if (enableDebug)
		{
			ComPtr<ID3D12InfoQueue> iq;
			if (SUCCEEDED(mDevice.As(&iq)))
			{
				iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
				iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			}
		}

		D3D12_COMMAND_QUEUE_DESC qd = {};
		qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ThrowIfFailed(mDevice->CreateCommandQueue(&qd, IID_PPV_ARGS(&mQueue)));

		ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);    // 매번 만들지 않고 재사용
	}
	
	void Device::Shutdown()
	{
		if (mQueue) Flush();
		if (mFenceEvent) { CloseHandle(mFenceEvent); mFenceEvent = nullptr; }
	}
	
	uint64_t Device::Signal()
	{
		ThrowIfFailed(mQueue->Signal(mFence.Get(), ++mFenceValue));
		return mFenceValue;
	}
	
	void Device::WaitForValue(uint64_t value)
	{
		if (mFence->GetCompletedValue() < value)
		{
			ThrowIfFailed(mFence->SetEventOnCompletion(value, mFenceEvent));
			WaitForSingleObject(mFenceEvent, INFINITE);
		}
	}
}
