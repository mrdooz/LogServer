#pragma once

class Window;
class Graphics;
class BmFont;
struct NewFrameMsg;

struct _cairo_surface;
struct _cairo;

template <typename vtx> class DynamicVb;
template <typename idx> class DynamicIb;

class LogServer {
public:
	LogServer();
	bool init(HINSTANCE hInstance);
	void close();

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
private:
	void handle_new_frame_msg(const NewFrameMsg *msg);

	static DWORD WINAPI server_thread(LPVOID data);
	std::vector<uint8 *> _slabs;

	std::unique_ptr<Window> _window;

	struct Zmq {
		Zmq() : _server_thread(INVALID_HANDLE_VALUE), _context(nullptr) {}
		HANDLE _server_thread;
		void *_context;
	} _zmq;

	struct Cairo {
		Cairo() : _surface(nullptr), _context(nullptr) {}
		bool init(HDC dc);
		void close();
		double width() { return x2 - x1; }
		double height() { return y2 - y1; }
		struct _cairo_surface *_surface;
		struct _cairo *_context;
		double x1, y1, x2, y2;
	} _cairo;

};
