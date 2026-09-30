#pragma once
#include <Windows.h>
#include <cstdint>

namespace baek
{
	class Window
	{
	public:
		bool Create(const wchar_t* title, uint32_t width, uint32_t height);
		bool PumpMessages();													// WM_QUIT이면 false

		HWND Handle() const				{ return mHwnd; }
		uint32_t Width() const			{ return mWidth; }
		uint32_t Height() const			{ return mHeight; }
		bool IsMinimized() const		{ return mMinimized; }
		bool ConsumeResize()			{ bool r = mResized; mResized = false; return r; }


	private:
		static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
		LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

		HWND mHwnd = nullptr;
		uint32_t mWidth = 0, mHeight = 0;
		bool mResized = false;
		bool mMinimized = false;
	};
}