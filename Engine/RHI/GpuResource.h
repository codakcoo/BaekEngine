#pragma once
#include "RHI/DxUtil.h"

namespace baek
{
	// ID3D12Resource + 현재 상태
	// 전체: 커맨드 리스트 하나에 순서대로 기록 (기록 순서 = GPU 실행 순서)
	class GpuResource
	{
	public:
		void Attach(ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES initial)
		{
			mRes = res;
			mState = initial;
		}

		// 지연 해제 큐로 넘길 때 사용
		ComPtr<ID3D12Resource> Detach()
		{
			mState = D3D12_RESOURCE_STATE_COMMON;
			return std::move(mRes);
		}

		void Reset()
		{
			mRes.Reset();
			mState = D3D12_RESOURCE_STATE_COMMON;
		}
		
		ID3D12Resource* Get() const { return mRes.Get(); }
		explicit operator bool() const { return mRes != nullptr; }
		D3D12_RESOURCE_STATES State() const { return mState; }

	protected:
		friend class BarrierBatch;
		ComPtr<ID3D12Resource> mRes;
		D3D12_RESOURCE_STATES mState = D3D12_RESOURCE_STATE_COMMON;
	};

	// 전환을 모았다가 한 번에 제출, 소멸 시 자동 Flush
	class BarrierBatch
	{
	public:
		explicit BarrierBatch(ID3D12GraphicsCommandList* cmd) : mCmd(cmd) {}
		~BarrierBatch() { Flush(); }
		BarrierBatch(const BarrierBatch&) = delete;
		BarrierBatch& operator=(const BarrierBatch&) = delete;

		BarrierBatch& Transition(GpuResource& r, D3D12_RESOURCE_STATES target)
		{
			if(!r.mRes || IsSatisfied(r.mState, target)) return *this;
			if(mCount == kMax) Flush();
			mBarriers[mCount++] = CD3DX12_RESOURCE_BARRIER::Transition(r.mRes.Get(), r.mState, target);
			r.mState = target;
			++mIssued;
			return *this;
		}

		BarrierBatch& Uav(GpuResource& r)						// 같은 UAV를 연속으로 쓸 때 쓰기 순서 보장
		{
			if (!r.mRes) return *this;
			if (mCount == kMax) Flush();
			mBarriers[mCount++] = CD3DX12_RESOURCE_BARRIER::UAV(r.mRes.Get());
			++mIssued;
			return *this;
		}

		void Flush()
		{
			if(mCount) { mCmd->ResourceBarrier(mCount, mBarriers); mCount = 0; }
		}

		UINT Issued() const { return mIssued; }

	private:
		// 읽기 상태끼리는 OR로 합칠 수 있음.
		// 예: 현재 PSR | NON_PSR, 목표 PSR -> 이미 만족, 전환 불필요
		static bool IsSatisfied(D3D12_RESOURCE_STATES cur, D3D12_RESOURCE_STATES target)
		{
			if (cur == target) return true;
			constexpr D3D12_RESOURCE_STATES kRead = D3D12_RESOURCE_STATE_GENERIC_READ | D3D12_RESOURCE_STATE_DEPTH_READ;
			const bool readOnly = target != D3D12_RESOURCE_STATE_COMMON && (target & ~kRead) == 0;
			return readOnly && (cur & target) == target;
		}

	// Propertys
	private:
		static constexpr UINT kMax = 16;
		D3D12_RESOURCE_BARRIER mBarriers[kMax]{};
		UINT mCount = 0;
		ID3D12GraphicsCommandList* mCmd;
		UINT mIssued = 0;
	};

	inline void Transition(ID3D12GraphicsCommandList* cmd, GpuResource& r, D3D12_RESOURCE_STATES target)
	{
		BarrierBatch(cmd).Transition(r, target);
	}

}