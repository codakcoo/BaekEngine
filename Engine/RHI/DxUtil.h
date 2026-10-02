#pragma once
#include <directx/d3d12.h>				// 서브 모듈 것 (반드시 맨 먼저)
#include <directx/d3dx12.h>				
#include <dxgi1_6.h>					// DXGI는 그대로 Window SDK 것
#include <wrl/client.h>
#include <Windows.h>
#include <stdexcept>
#include <cstdio>

namespace baek
{
	using Microsoft::WRL::ComPtr;

	inline void ThrowIfFailed(HRESULT hr, const char* expr, const char* file, int line)
	{
		if (FAILED(hr))
		{
			char buf[512];
			sprintf_s(buf, "%s\n%s(%d)\nHRESULT = 0x%08x", expr, file, line, (unsigned)hr);
			throw std::runtime_error(buf);
		}
	}
}

#define ThrowIfFailed(x) ::baek::ThrowIfFailed((x), #x, __FILE__, __LINE__)