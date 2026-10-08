#pragma once
#include <string>

namespace baek
{
	// Assets 폴더 기준 상대 경로 -> 절대 경로. 실행 위치와 무관하게 동작한다.
	inline std::string AssetPath(const std::string& relative)
	{
		return std::string(BAEK_ROOT_DIR "Assets/") + relative;
	}
}