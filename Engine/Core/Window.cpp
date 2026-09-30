#include "Core/WIndow.h"

namespace baek
{
	bool Window::Create(const wchar_t* title, uint32_t width, uint32_t height)
	{
		HINSTANCE hInst = GetModuleHandleW(nullptr);

		WNDCLASSEXW wc = { sizeof(wc) };
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc;
		wc.hInstance = hInst;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.lpszClassName = L"BaekEngineWindow";
		if(!RegisterClassExW(&wc)) return false;

		RECT r = { 0, 0, (LONG)width, (LONG)height };
		AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

		mWidth = width; mHeight = height;
		// 마지막 인자로 this 전달 -> WM_NCCREATE에서 HWND에 붙임 (전역 mApp 불필요)
		mHwnd = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
			nullptr, nullptr, hInst, this);
		if(!mHwnd) return false;

		ShowWindow(mHwnd, SW_SHOW);
		return true;
	}

	bool Window::PumpMessages()
	{
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT) return false;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		return true;
	}

	LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		if (msg == WM_NCCREATE)
		{
			auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
			auto self = static_cast<Window*>(cs->lpCreateParams);
			self->mHwnd = hwnd;
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
		}
		auto self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		return self ? self->HandleMessage(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
	}

	LRESULT Window::HandleMessage(UINT msg, WPARAM wp, LPARAM lp)
	{
		switch (msg)
		{
		case WM_SIZE:
			mMinimized = (wp == SIZE_MINIMIZED);
			if (!mMinimized)
			{
				mWidth = LOWORD(lp);
				mHeight = HIWORD(lp);
				mResized = true;						// 실제 처리는 렌더라가 프레임 시작 시
			}
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(mHwnd, msg, wp, lp);
	}

};
