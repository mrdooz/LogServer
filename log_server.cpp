// DebugServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>
#include <memory>
#include "log_messages.hpp"
#include "log_server.hpp"

using namespace std;

class Window {
public:
	typedef LRESULT (CALLBACK *_WindowProc)(__in  HWND hwnd, __in  UINT uMsg, __in  WPARAM wParam, __in  LPARAM lParam);

	Window(HINSTANCE instance, int width, int height, const std::string &class_name, const std::string &window_name, _WindowProc wndproc);
	bool create();
private:
	void set_client_size();

	HWND _hwnd;
	HINSTANCE _instance;
	int _width, _height;
	std::string _class_name;
	std::string _window_name;
	_WindowProc _wnd_proc;
};

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

bool Window::create()
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
		_instance, NULL);

	set_client_size();

	ShowWindow(_hwnd, SW_SHOW);

	return true;
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

DWORD WINAPI LogServer::server_thread(void *context) {

	context = zmq_init(1);

	void *responder = zmq_socket(context, ZMQ_PULL);
	zmq_bind(responder, "tcp://*:5555");

	while (true) {
		zmq_msg_t request;
		zmq_msg_init(&request);
		if (zmq_recv(responder, &request, 0) == -1 && zmq_errno() == ETERM)
			break;

		int size = zmq_msg_size(&request);
		log_msg::Base	*msg = (log_msg::Base	*)zmq_msg_data(&request);

		switch (msg->cmd) {

			case log_msg::kCmdBeginFrame: {
				log_msg::BeginFrame *f = static_cast<log_msg::BeginFrame *>(msg);
				break;
			}

			case log_msg::kCmdEndFrame: {
				log_msg::EndFrame *f = static_cast<log_msg::EndFrame *>(msg);
				break;
			}
		}


		zmq_msg_close(&request);
	}
	zmq_close(responder);

	return 0;
}


bool LogServer::init() {

	_context = zmq_init(1);
	_server_thread = CreateThread(NULL, 0, server_thread, _context, 0, NULL);

	return true;
}

void LogServer::close() {
	zmq_term(_context);
	WaitForSingleObject(_server_thread, INFINITE);
}

//
// Hello World server in C++
// Binds REP socket to tcp://*:5555
// Expects "Hello" from client, replies with "World"
//
#include <zmq.hpp>
#include <string>
#include <iostream>
//#include <unistd.h>

int main2() {
	// Prepare our context and socket
	zmq::context_t context (1);
	zmq::socket_t socket (context, ZMQ_REP);
	socket.bind ("tcp://*:5555");

	while (true) {
		zmq::message_t request;

		// Wait for next request from client
		socket.recv (&request);
		std::cout << "Received Hello" << std::endl;

		// Do some 'work'
		//sleep (1);

		// Send reply back to client
		zmq::message_t reply (5);
		memcpy ((void *) reply.data (), "World", 5);
		socket.send (reply);
	}
	return 0;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {

	//return main2();

	Window window(hInstance, 640, 480, "test", "test", WndProc);
	if (!window.create())
		return 1;

	LogServer server;
	if (!server.init())
		return 1;

	MSG msg = {0};

	while (WM_QUIT != msg.message) {
		if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
		}
	}

	server.close();


	return 0;
}
