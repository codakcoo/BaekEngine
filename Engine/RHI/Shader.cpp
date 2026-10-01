#include "RHI/Shader.h"
#include <stdexcept>

namespace baek
{
	ComPtr<ID3DBlob> CompileShader(const std::wstring& path, const char* entry, const char* target)
	{
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
		flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
		ComPtr<ID3DBlob> code, err;
		HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target, flags, 0, &code, &err);
		if(err)
			OutputDebugStringA((const char*)err->GetBufferPointer());
		if (FAILED(hr))
		{
			std::string msg = err ? (const char*)err->GetBufferPointer() : "shader file not found (check BAEK_SHADER_DIR)";
			throw std::runtime_error("Failed to compile shader: " + msg);
		}

		return code;
	}
}

