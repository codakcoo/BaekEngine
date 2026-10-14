#pragma	once
#include "RHI/DxUtil.h"
#include <wrl/client.h>
#include <deque>
#include <functional>

namespace baek
{
	// GPU가 아직 사용 중일 수 있는 리소스를 펜스 값과 함께 보관했다가, 그 펜스가 지나면 해제
	class DeferredReleaseQueue
	{
	public:
		// 다음에 Signal될 펜스 값을 알려주는 함수 (Renderer::Init에서 연결)
		void Init(std::function<uint64_t()> nextFence) { mNextFence = std::move(nextFence); }


		void Release(Microsoft::WRL::ComPtr<ID3D12Object> obj)
		{
			if(obj) mItems.push_back({ mNextFence(), std::move(obj), nullptr}); 
			Track();
		}

		void Defer(std::function<void()> fn)
		{
			mItems.push_back({ mNextFence(), nullptr, std::move(fn)});
			Track();
		}

		// completed = 펜스의 GetCompletedValue()
		void Collect(UINT64 completed)
		{
			while (!mItems.empty() && mItems.front().fence <= completed)
			{
				if(mItems.front().fn) mItems.front().fn();
				mItems.pop_front();							// ComPtr 소멸 = 실제 Release
				++mTotalReleaseed;
			}
		}

		void Flush()
		{
			for(auto& it : mItems) if(it.fn) it.fn();
			mItems.clear();
		}

		size_t Pending() const { return mItems.size(); }

		size_t Peak() const { return mPeak; }
		size_t TotalReleased() const { return mTotalReleaseed; }

	private:
		void Track()
		{
			mPeak = std::max(mPeak, mItems.size());
		}

	private:
		struct Item
		{
			UINT64 fence;
			Microsoft::WRL::ComPtr<ID3D12Object> obj;					// ComPtr<ID3D12Resource>는 ComPtr<ID3D12Object>로 자동 변환되므로, PSO나 힙 등 무엇이든 넣을 수 있다.
			std::function<void()> fn;
		};
		std::deque<Item> mItems;					// 펜스 값이 증가하는 순서로만 들어오므로 앞에서부터 확인
		std::function<uint64_t()> mNextFence;

		size_t mPeak = 0;
		size_t mTotalReleaseed = 0;
	};
}