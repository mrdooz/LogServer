#include "stdafx.h"
#include "log_server.hpp"
#include "window.hpp"
#include "utils.hpp"
#include "file_utils.hpp"
#include "cairo/include/cairo/cairo.h"
#include "cairo/include/cairo/cairo-win32.h"
#include "log_messages.hpp"

#pragma comment(lib, "cairo/lib/cairo.lib")

static const uint32 WM_NEW_FRAME = WM_APP + 1;

using namespace std;

struct ThreadConfig {
	HWND wnd;
	LogServer *log_server;
	void *context;
};

struct NewFrameMsg {
	NewFrameMsg(uint8 *start, uint8 *end) : start(start), end(end) {}
	uint8 *start;
	uint8 *end; // 1 byte past end
};

LRESULT CALLBACK LogServer::WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	static LogServer *self = nullptr;

	switch (message) {
		case WM_CREATE: {
			CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
			self = (LogServer *)cs->lpCreateParams;
			break;
		}

		case WM_NEW_FRAME: {
			NewFrameMsg *msg = (NewFrameMsg *)wParam;
			self->handle_new_frame_msg(msg);
			delete msg;
			break;
		}

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

void LogServer::handle_new_frame_msg(const NewFrameMsg *msg) {

	cairo_t *ctx = _cairo._context;

	// scale factors
	double sx = 1, sy = 1;
	double ox = _cairo.x1, oy = _cairo.y1;

	// only call fill when the previous color changes
	double pr = -1, pg = -1, pb = -1, pa = -1;
	bool first_time = true;
	bool quads_remaining = false;

	uint8 *ptr = (uint8 *)msg->start;
	while (ptr != msg->end) {
		log_msg::Base *b = (log_msg::Base *)ptr;
		switch (b->cmd) {

			case log_msg::kCmdSetupWindow: {
				log_msg::SetupWindow *s = (log_msg::SetupWindow *)b;
				sx = s->width ? _cairo.width() / s->width : 1;
				sy = s->height ? _cairo.height() / s->height : 1;
				ptr += sizeof(log_msg::SetupWindow);
				break;
			}

			case log_msg::kCmdQuad: {
				log_msg::DrawQuads *q = (log_msg::DrawQuads *)b;
				for (uint32_t i = 0; i < q->count; ++i) {
					log_msg::Quad *cur = &q->quads[i];
					uint32_t col32 = cur->fill_color;
					double x = (double)cur->x;
					double y = (double)cur->y;
					double h = (double)cur->height;
					double w = (double)cur->width;
	#define MK_COL(x, shift) ((x >> shift) & 0xff) / 255.0f
					double r = MK_COL(col32, 24), g = MK_COL(col32, 16), b = MK_COL(col32, 8), a = MK_COL(col32, 0);
	#undef MK_COL

					const bool got_new_color = r != pr || g != pg || b != pb || a != pa;
					if (first_time || got_new_color) {
						// draw the old stuff first
						if (!first_time)
							cairo_fill(ctx);
						first_time = false;
						cairo_set_source_rgba(ctx, r, g, b, a);
						pr = r; pg = g; pb = b; pa = a;
						quads_remaining = true;
					}
					cairo_rectangle(ctx, ox + x * sx, oy + y * sy, w * sx, h * sy);
				}
				ptr += sizeof(log_msg::DrawQuads) + q->count * sizeof(log_msg::Quad);
				break;
			}
		}
	}

	if (quads_remaining)
		cairo_fill(ctx);
}

DWORD WINAPI LogServer::server_thread(void *data) {

	unique_ptr<ThreadConfig> config((ThreadConfig *)data);

	HWND wnd = config->wnd;
	LogServer *self = config->log_server;
	void *context = config->context;
	void *responder = zmq_socket(context, ZMQ_PULL);
	zmq_bind(responder, "tcp://*:5555");

	// create initial slab
	int slab_size = 10 * 1024 * 1024;
	self->_slabs.push_back(new uint8[slab_size]);
	uint8 *cur_slab = self->_slabs.back();
	int left = slab_size;

	int frame_balance = 0;
	bool skipping = false;
	uint8 *start_ptr = cur_slab;
	uint8 *cur_ptr = cur_slab;

	while (true) {
		zmq_msg_t request;
		zmq_msg_init(&request);
		if (zmq_recv(responder, &request, 0) == -1 && zmq_errno() == ETERM)
			break;

		int size = zmq_msg_size(&request);
		log_msg::Base	*msg = (log_msg::Base	*)zmq_msg_data(&request);

		switch (msg->cmd) {

			// handle frame markers
			case log_msg::kCmdBeginFrame: {
				if (++frame_balance != 1) {
					// skip nested frames..
					skipping = true;
				} else {
					start_ptr = cur_ptr;
					log_msg::BeginFrame *f = static_cast<log_msg::BeginFrame *>(msg);
				}
				break;
			}

			case log_msg::kCmdEndFrame: {
				if (--frame_balance != 0) {
					skipping = true;
				} else {
					log_msg::EndFrame *f = static_cast<log_msg::EndFrame *>(msg);
					// report a completed frame
					PostMessage(wnd, WM_NEW_FRAME, (WPARAM)new NewFrameMsg(start_ptr, cur_ptr), 0);
				}
				break;
			}

			// handle render commands
			case log_msg::kCmdQuad:
			case log_msg::kCmdSetupWindow: {
				if (!skipping) {
					int req_size = zmq_msg_size(&request);
					// if the frame doesn't fit in the current slab, we move the frame to a new slab
					if (req_size > left) {
						slab_size = max(slab_size, 2 * req_size);
						self->_slabs.push_back(new uint8[slab_size]);
						cur_slab = self->_slabs.back();
						memcpy(cur_slab, start_ptr, cur_ptr - start_ptr);
						start_ptr = cur_ptr = cur_slab;
					}
					memcpy(cur_ptr, msg, req_size);
					cur_ptr += req_size;
					left -= req_size;
				}
				break;
			}
		}

		zmq_msg_close(&request);
	}
	zmq_close(responder);

	return 0;
}

LogServer::LogServer()
{
}

bool LogServer::Cairo::init(HDC dc) {
	if (!(_surface = cairo_win32_surface_create(dc)))
		return false;

	if (!(_context = cairo_create(_surface)))
		return false;

	cairo_clip_extents(_context, &x1, &y1, &x2, &y2);

	return true;
}

void LogServer::Cairo::close() {
	cairo_destroy(_context);
	cairo_surface_destroy(_surface);
}

bool LogServer::init(HINSTANCE hInstance) {

	_window.reset(new Window(hInstance, 640, 480, "test", "test", &LogServer::WndProc));
	if (!_window->create(this))
		return false;

	if (!_cairo.init(_window->dc()))
		return false;

	ThreadConfig *config = new ThreadConfig();
	config->wnd = _window->hwnd();

	config->log_server = this;
	config->context = _zmq._context = zmq_init(1);
	_zmq._server_thread = CreateThread(NULL, 0, server_thread, config, 0, NULL);

	return true;
}

void LogServer::close() {
	zmq_term(_zmq._context);
	WaitForSingleObject(_zmq._server_thread, INFINITE);
	CloseHandle(_zmq._server_thread);
	_zmq._server_thread = INVALID_HANDLE_VALUE;
}


int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {

	LogServer server;
	if (!server.init(hInstance))
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
