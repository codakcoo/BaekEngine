#pragma	once
#include "RHI/Device.h"
#include <d3dcompiler.h>
#include <string>
#include <wrl/client.h>

#define BAEK_WIDEN2(x) L##x
#define BAEK_WIDEN(x) BAEK_WIDEN2(x)

using Microsoft::WRL::ComPtr;

namespace baek
{
	inline std::wstring ShaderPath(const wchar_t* file)
	{
		return std::wstring(BAEK_WIDEN(BAEK_SHADER_DIR)) + file;
	}
	ComPtr<ID3DBlob> CompileShader(const std::wstring& path, const char* entry, const char* target);
}
