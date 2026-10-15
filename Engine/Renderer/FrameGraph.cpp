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
		for (size_t i = 0; i < uses.size(); ++i)
			for (size_t j = i + 1; j < uses.size(); ++j)
			{
				const ResourceUse& a = uses[i];
				const ResourceUse& b = uses[j];
				const bool overlap = a.sub == b.sub || a.sub == GpuResource::kAll || b.sub == GpuResource::kAll;
				if (a.res == b.res && overlap && a.state != b.state)
				{
					char msg[256];
					sprintf_s(msg, "FrameGraph conflict in pass '%s': use[%zu] sub=%d state=0x%X vs use[%zu] sub=%d state=0x%X\n",
						name.c_str(), i, (int)a.sub, a.state, j, (int)b.sub, b.state);
					OutputDebugStringA(msg);
					assert(false && "FrameGraph: conflicting states for the same subresource in one pass");
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
