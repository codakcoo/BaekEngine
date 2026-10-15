#pragma once
#include "RHI/GpuResource.h"
#include <functional>
#include <string>
#include <vector>

namespace baek
{
	// 패스가 리소스를 어떤 상태로 쓰는지
	struct ResourceUse
	{
		GpuResource* res;
		D3D12_RESOURCE_STATES state;
		UINT sub = GpuResource::kAll;

		static ResourceUse Srv(GpuResource& r, UINT sub = GpuResource::kAll) { return { &r, kShaderReadState, sub }; }
		static ResourceUse Rtv(GpuResource& r, UINT sub = GpuResource::kAll) { return { &r, D3D12_RESOURCE_STATE_RENDER_TARGET, sub }; }
		static ResourceUse Dsv(GpuResource& r, UINT sub = GpuResource::kAll) { return { &r, D3D12_RESOURCE_STATE_DEPTH_WRITE, sub }; }
		static ResourceUse Uav(GpuResource& r, UINT sub = GpuResource::kAll) { return { &r, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, sub }; }
	};

	class FrameGraph
	{
	public:
		using ExecFn = std::function<void(ID3D12GraphicsCommandList*)>;
		struct PassStat
		{
			std::string name;
			UINT barriers;
		};

		void Reset() { mPasses.clear(); mExports.clear(); }
		void AddPass(std::string name, std::vector<ResourceUse> uses, ExecFn exec);

		// 그래프 밖(ImGui 등)에서 쓸 리소스의 최종 상태
		void Export(GpuResource& r, D3D12_RESOURCE_STATES state, UINT sub = GpuResource::kAll) 
		{ 
			mExports.push_back({ &r, state, sub }); 
		}
		void Execute(ID3D12GraphicsCommandList* cmd);
		const std::vector<PassStat>& Stats() const { return mStats; }			// 직전 Execute 결과

	private:
		struct Pass
		{
			std::string name;
			std::vector<ResourceUse> uses;
			ExecFn exec;
		};
		std::vector<Pass> mPasses;
		std::vector<ResourceUse> mExports;
		std::vector<PassStat> mStats;
	};
}