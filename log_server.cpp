#include "stdafx.h"
#include "log_messages.hpp"
#include "log_server.hpp"
#include "window.hpp"
#include "graphics.hpp"
#include "dynamic_buffers.hpp"
#include "dx_utils.hpp"
#include "utils.hpp"
#include "file_utils.hpp"
#include "bmfont.hpp"

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

	ID3D11Device *device = _graphics->device();
	ID3D11DeviceContext *context = _graphics->context();
	XMMATRIX mtx_proj = XMMatrixIdentity();

	vector<PosCol> verts;
	vector<uint32> indices;

	uint8 *ptr = (uint8 *)msg->start;
	while (ptr != msg->end) {
		log_msg::Base *b = (log_msg::Base *)ptr;
		switch (b->cmd) {

			case log_msg::kCmdSetupWindow: {
				log_msg::SetupWindow *s = (log_msg::SetupWindow *)b;
				mtx_proj = XMMatrixOrthographicOffCenterLH(0, (float)s->width, 0, (float)s->height, 1, 100);
				ptr += sizeof(log_msg::SetupWindow);
				break;
			}
		
			case log_msg::kCmdQuad: {
				log_msg::DrawQuads *q = (log_msg::DrawQuads *)b;
				for (uint32_t i = 0; i < q->count; ++i) {
					log_msg::Quad *cur = &q->quads[i];
					uint32_t col32 = cur->fill_color;
					float x = (float)cur->x;
					float y = (float)cur->y;
					float h = (float)cur->height;
					float w = (float)cur->width;
#define MK_COL(x, shift) ((x >> shift) & 0xff) / 255.0f
					XMFLOAT4 col(MK_COL(col32, 24), MK_COL(col32, 16), MK_COL(col32, 8), MK_COL(col32, 0));
#undef MK_COL
					// 1, 2
					// 0, 3
					int v = (int)verts.size();
					verts.push_back(PosCol(x,   y+h, 1, col));
					verts.push_back(PosCol(x,   y,   1, col));
					verts.push_back(PosCol(x+w, y,   1, col));
					verts.push_back(PosCol(x+w, y+h, 1, col));

					indices.push_back(v+0); indices.push_back(v+1); indices.push_back(v+2);
					indices.push_back(v+0); indices.push_back(v+2); indices.push_back(v+3);
				}
				ptr += sizeof(log_msg::DrawQuads) + q->count * sizeof(log_msg::Quad);
				break;
			}
		}
	}

	if (!_vb)
		_vb.reset(new DynamicVb<PosCol>(device, context));
	_vb->fill(verts);


	if (!_ib)
		_ib.reset(new DynamicIb<uint32>(device, context));
	_ib->fill(indices);

	ID3D11Buffer *bufs[] = { _vb->get() };
	UINT strides[] = { sizeof(PosCol) };
	UINT ofs[] = { 0 };
	context->IASetVertexBuffers(0, 1, bufs, strides, ofs);
	context->IASetIndexBuffer(_ib->get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(_layout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->RSSetState(_rs);
	context->OMSetDepthStencilState(_dss, ~0);

	context->VSSetShader(_vs, NULL, 0);

	D3D11_MAPPED_SUBRESOURCE res;
	context->Map(_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
	*(XMMATRIX *)res.pData = XMMatrixTranspose(mtx_proj);
	context->Unmap(_cb, 0);

	context->VSSetConstantBuffers(0, 1, &(_cb.p));

	context->PSSetShader(_ps, NULL, 0);

	context->DrawIndexed(indices.size(), 0, 0);

	_graphics->present();
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
	: _server_thread(INVALID_HANDLE_VALUE)
	, _context(nullptr)
{
}

bool LogServer::init(HINSTANCE hInstance) {

	_window.reset(new Window(hInstance, 640, 480, "test", "test", &LogServer::WndProc));
	if (!_window->create(this))
		return false;

	_graphics.reset(new Graphics());
	if (!_graphics->init(_window.get()))
		return 1;

	ID3D11Device *device = _graphics->device();
	ID3D11DeviceContext *context = _graphics->context();

	D3D11_INPUT_ELEMENT_DESC desc[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	if (!create_shaders_from_file("log_server.hlsl", _graphics->feature_level(), device, "vs_main", "ps_main", desc, ELEMS_IN_ARRAY(desc), &_vs.p, &_ps.p, &_layout.p))
		return false;

	if (FAILED(create_dynamic_constant_buffer(device, sizeof(XMMATRIX), &_cb.p)))
		return false;

	CD3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
	CD3D11_RASTERIZER_DESC rasterize_desc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
	CD3D11_DEPTH_STENCIL_DESC dss_desc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());

	dss_desc.DepthEnable = FALSE;

	if (FAILED(device->CreateDepthStencilState(&dss_desc, &_dss.p)))
		return false;

	if (FAILED(device->CreateRasterizerState(&rasterize_desc, &_rs.p)))
		return false;

	if (FAILED(device->CreateBlendState(&blend_desc, &_bs.p)))
		return false;

	ThreadConfig *config = new ThreadConfig();
	config->wnd = _window->hwnd();

	config->log_server = this;
	config->context = _context = zmq_init(1);
	_server_thread = CreateThread(NULL, 0, server_thread, config, 0, NULL);

	_font.reset(new BmFont(_graphics.get()));
	if (!_font->create_from_file("test2.fnt"))
		return false;

	_font->render("magnus: %d", 52);

	return true;
}

void LogServer::close() {
	zmq_term(_context);
	WaitForSingleObject(_server_thread, INFINITE);
	CloseHandle(_server_thread);
	_server_thread = INVALID_HANDLE_VALUE;
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
