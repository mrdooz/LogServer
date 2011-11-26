#include "stdafx.h"
#include "window.hpp"

Window::Window(HINSTANCE instance, int width, int height, const std::string &class_name, const std::string &window_name, _WindowProc wndproc)
	: _instance(instance)
	, _width(width)
	, _height(height)
	, _class_name(class_name)
	, _window_name(window_name)
	, _wnd_proc(wndproc)
{
}

void Window::set_client_size()
{
	RECT client_rect;
	RECT window_rect;
	GetClientRect(_hwnd, &client_rect);
	GetWindowRect(_hwnd, &window_rect);
	window_rect.right -= window_rect.left;
	window_rect.bottom -= window_rect.top;
	window_rect.left = window_rect.top = 0;
	const int dx = window_rect.right - client_rect.right;
	const int dy = window_rect.bottom - client_rect.bottom;

	SetWindowPos(_hwnd, NULL, -1, -1, _width + dx, _height + dy, SWP_NOZORDER | SWP_NOREPOSITION);
}

bool Window::create(void *lparam)
{
	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(wcex));

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = _wnd_proc;
	wcex.hInstance      = _instance;
	wcex.hbrBackground  = 0;
	wcex.lpszClassName  = _class_name.c_str();

	if (!RegisterClassEx(&wcex))
		return false;

	const UINT window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

	_hwnd = CreateWindow(_class_name.c_str(), _window_name.c_str(), window_style,
		CW_USEDEFAULT, CW_USEDEFAULT, _width, _height, NULL, NULL,
		_instance, lparam);

	set_client_size();

	ShowWindow(_hwnd, SW_SHOW);

	return true;
}
