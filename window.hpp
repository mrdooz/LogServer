#pragma once

class Window {
public:
	typedef LRESULT (CALLBACK *_WindowProc)(__in  HWND hwnd, __in  UINT uMsg, __in  WPARAM wParam, __in  LPARAM lParam);

	Window(HINSTANCE instance, int width, int height, const std::string &class_name, const std::string &window_name, _WindowProc wndproc);
	~Window();
	bool create(void *lparam = nullptr);
	HWND hwnd() const { return _hwnd; }
	int width() const { return _width; }
	int height() const { return _height; }
	HDC dc() const { return _dc; }
private:
	void set_client_size();

	HWND _hwnd;
	HDC _dc;
	HINSTANCE _instance;
	int _width, _height;
	std::string _class_name;
	std::string _window_name;
	_WindowProc _wnd_proc;
};
