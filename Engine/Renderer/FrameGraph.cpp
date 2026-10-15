#include "FrameGraph.h"
#include <cassert>

namespace baek
{
	// PIX / RenderDoc에서 패스 이름으로 구간이 보이게
	static void BeginEvent(ID3D12GraphicsCommandList* cmd, const std::string& name)
	{
		const std::wstring w(name.begin(), name.end());
		cmd->BeginEvent(0, w.c_str(), (UINT)((w.size() + 1) * sizeof(wchar_t)));   // 0 = 유니코드 문자열
	}


	void FrameGraph::AddPass(std::string name, std::vector<ResourceUse> uses, ExecFn exec)
	{
#if defined(_DEBUG)
		// 한 패스 안에서 같은 리소스를 서로 다른 상태로 요구하면 모순
		for (size_t i = 0; i < uses.size(); ++i)
		{
			for (size_t j = i + 1; j < uses.size(); ++j)
			{
				const auto& a = uses[i];
				const auto& b = uses[j];
				assert(!(a.res == b.res && a.state != b.state) && "FrameGraph: conflicting states for the same resource in one pass");
			}
		}
#endif
		mPasses.push_back({std::move(name), std::move(uses), std::move(exec)});
	}
	
	void FrameGraph::Execute(ID3D12GraphicsCommandList* cmd)
	{
		mStats.clear();

		for (Pass& p : mPasses)
		{
			UINT issued = 0;
			{
				BarrierBatch batch(cmd);
				for (const ResourceUse& u : p.uses)
					batch.Transition(*u.res, u.state, u.sub);
				issued = batch.Issued();
			}											// 여기서 Flush (패스 실행 전에 한 번에 제출)

			BeginEvent(cmd, p.name);
			p.exec(cmd);
			cmd->EndEvent();

			mStats.push_back({ p.name, issued });
		}

		BarrierBatch batch(cmd);
		for(const ResourceUse& u : mExports)
			batch.Transition(*u.res, u.state, u.sub);
	}
}
