#pragma once

class Window;
class Graphics;
class BmFont;
struct NewFrameMsg;

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

	std::unique_ptr<DynamicVb<PosCol>> _vb;
	std::unique_ptr<DynamicIb<uint32_t>> _ib;

	CComPtr<ID3D11Buffer> _cb;
	CComPtr<ID3D11InputLayout> _layout;
	CComPtr<ID3D11VertexShader> _vs;
	CComPtr<ID3D11PixelShader> _ps;

	CComPtr<ID3D11RasterizerState> _rs;
	CComPtr<ID3D11DepthStencilState> _dss;
	CComPtr<ID3D11BlendState> _bs;

	std::unique_ptr<Window> _window;
	std::unique_ptr<Graphics> _graphics;

	std::unique_ptr<BmFont> _font;

	HANDLE _server_thread;
	void *_context;
};
