#pragma once
#include "RHI/DxUtil.h"
#include <vector>
#include <algorithm>

namespace baek
{
	// 픽셀/컴퓨트 어느 셰이더에서든 읽기 (읽기 상태는 OR로 합칠 수 있음)
	constexpr D3D12_RESOURCE_STATES kShaderReadState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	// ID3D12Resource + 현재 상태
	// 전체: 커맨드 리스트 하나에 순서대로 기록 (기록 순서 = GPU 실행 순서)
	class GpuResource
	{
	public:
		GpuResource() = default;
		GpuResource(const GpuResource&) = delete;
		GpuResource& operator=(const GpuResource&) = delete;
		GpuResource(GpuResource&&) = default;
		GpuResource& operator=(GpuResource&&) = default;

		static constexpr UINT kAll = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		void Attach(ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES initial)
		{
			mRes = std::move(res);
			const auto d = mRes->GetDesc();
			const UINT arrays = (d.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? 1u : d.DepthOrArraySize;
			mStates.assign(d.MipLevels * arrays, initial);				// 서브리소스 = 밉 x 배열
		}

		// 지연 해제 큐로 넘길 때 사용
		ComPtr<ID3D12Resource> Detach()
		{
			mStates.clear();
			return std::move(mRes);
		}

		void Reset()
		{
			mRes.Reset();
			mStates.clear();
		}
		
		ID3D12Resource* Get() const { return mRes.Get(); }
		explicit operator bool() const { return mRes != nullptr; }
		D3D12_RESOURCE_STATES State(UINT sub = 0) const { return mStates[sub]; }

	private:
		friend class BarrierBatch;
		ComPtr<ID3D12Resource> mRes;
		std::vector<D3D12_RESOURCE_STATES> mStates;
	};

	// 전환을 모았다가 한 번에 제출, 소멸 시 자동 Flush
	class BarrierBatch
	{
	public:
		explicit BarrierBatch(ID3D12GraphicsCommandList* cmd) : mCmd(cmd) {}
		~BarrierBatch() { Flush(); }
		BarrierBatch(const BarrierBatch&) = delete;
		BarrierBatch& operator=(const BarrierBatch&) = delete;

		BarrierBatch& Transition(GpuResource& r, D3D12_RESOURCE_STATES target, UINT sub = GpuResource::kAll)
		{
			if (!r.mRes)return *this;
			if (sub != GpuResource::kAll) { Push(r, sub, target); return *this; }
			
			// 전체 요청: 모든 서브리소스가 같은 상태면 배리어 1개, 아니면 서브리소스별로
			const auto& st = r.mStates;
			const bool uniform = std::all_of(st.begin(), st.end(), [&](auto s){ return s == st[0]; });
			if(uniform) Push(r, GpuResource::kAll, target);
			else for (UINT i = 0; i < (UINT)st.size(); ++i) Push(r, i, target);
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
		void Push(GpuResource& r, UINT sub, D3D12_RESOURCE_STATES target)
		{
			D3D12_RESOURCE_STATES& cur = r.mStates[sub == GpuResource::kAll ? 0 : sub];
			if(IsSatisfied(cur, target)) return;
			if(mCount == kMax) Flush();
			mBarriers[mCount++] = CD3DX12_RESOURCE_BARRIER::Transition(r.mRes.Get(), cur, target, sub);
			++mIssued;
			if (sub == GpuResource::kAll) std::fill(r.mStates.begin(), r.mStates.end(), target);
			else cur = target;
		}

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
		ID3D12GraphicsCommandList* mCmd;
		UINT mCount = 0;
		UINT mIssued = 0;
	};

	inline void Transition(ID3D12GraphicsCommandList* cmd, GpuResource& r, D3D12_RESOURCE_STATES target, UINT sub = GpuResource::kAll)
	{
		BarrierBatch(cmd).Transition(r, target, sub);
	}

}