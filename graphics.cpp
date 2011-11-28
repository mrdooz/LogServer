#include "stdafx.h"
#include "graphics.hpp"
#include "window.hpp"

using namespace std;

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "DXGUID.lib")
#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "D3DX11.lib")

#define B_ERR_HR(x) if (FAILED(x)) return false;
#define B_ERR_BOOL(x) if (!(x)) return false;

struct RenderTargetData {

	RenderTargetData() { reset(); }
	void reset() { texture = NULL; depth_stencil = NULL; rtv = NULL; dsv = NULL; srv = NULL; }
	operator bool() { return texture || depth_stencil || rtv || dsv || srv; }

	D3D11_TEXTURE2D_DESC texture_desc;
	D3D11_TEXTURE2D_DESC depth_buffer_desc;
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	CComPtr<ID3D11Texture2D> texture;
	CComPtr<ID3D11Texture2D> depth_stencil;
	CComPtr<ID3D11RenderTargetView> rtv;
	CComPtr<ID3D11DepthStencilView> dsv;
	CComPtr<ID3D11ShaderResourceView> srv;
};

bool Graphics::init(const Window *window) {

	_buffer_format = DXGI_FORMAT_B8G8R8A8_UNORM; //DXGI_FORMAT_R8G8B8A8_UNORM;

	int width = window->width();
	int height = window->height();

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof( sd ) );
	sd.BufferCount = 2;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = _buffer_format;
	sd.BufferDesc.RefreshRate.Numerator = 0;
	sd.BufferDesc.RefreshRate.Denominator = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = window->hwnd();
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	// Create DXGI factory to enumerate adapters
	CComPtr<IDXGIFactory1> dxgi_factory;
	B_ERR_HR(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&dxgi_factory));

	// Use the first adapter
	vector<CComPtr<IDXGIAdapter1> > adapters;
	UINT i = 0;
	IDXGIAdapter1 *adapter = nullptr;
	int perfhud = -1;
	for (int i = 0; SUCCEEDED(dxgi_factory->EnumAdapters1(i, &adapter)); ++i) {
		adapters.push_back(adapter);
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);
		if (wcscmp(desc.Description, L"NVIDIA PerfHud") == 0) {
			perfhud = i;
		}
	}

	B_ERR_BOOL(!adapters.empty());

	adapter = perfhud != -1 ? adapters[perfhud] : adapters.front();

#ifdef _DEBUG
	const int flags = D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#else
	const int flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#endif

	// Create the DX11 device
	B_ERR_HR(D3D11CreateDeviceAndSwapChain(
		adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &sd, &_swap_chain, &_device, &_feature_level, &_immediate_context.p));

	B_ERR_BOOL(_feature_level >= D3D_FEATURE_LEVEL_9_3);

	B_ERR_HR(_device->QueryInterface(IID_ID3D11Debug, (void **)&_d3d_debug));

	B_ERR_BOOL(create_back_buffers(width, height));

	B_ERR_HR(_device->CreateDepthStencilState(&CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT()), &_default_depth_stencil_state.p));
	B_ERR_HR(_device->CreateRasterizerState(&CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT()), &_default_rasterizer_state.p));
	B_ERR_HR(_device->CreateBlendState(&CD3D11_BLEND_DESC(CD3D11_DEFAULT()), &_default_blend_state.p));
	for (int i = 0; i < 4; ++i)
		_default_blend_factors[i] = 1.0f;
	return true;

	return true;
}


bool Graphics::create_back_buffers(int width, int height) {

	RenderTargetData *rt = _default_render_target = new RenderTargetData;

	// Get the dx11 back buffer
	B_ERR_HR(_swap_chain->GetBuffer(0, IID_PPV_ARGS(&rt->texture.p)));
	rt->texture->GetDesc(&rt->texture_desc);

	B_ERR_HR(_device->CreateRenderTargetView(rt->texture, NULL, &rt->rtv));
	rt->rtv->GetDesc(&rt->rtv_desc);

	// depth buffer
	B_ERR_HR(_device->CreateTexture2D(
		&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL), 
		NULL, &rt->depth_stencil.p));

	B_ERR_HR(_device->CreateDepthStencilView(rt->depth_stencil, NULL, &rt->dsv.p));
	rt->dsv->GetDesc(&rt->dsv_desc);

	_viewport = CD3D11_VIEWPORT (0.0f, 0.0f, (float)width, (float)height);

	set_render_target(rt);

	return true;
}

void Graphics::set_render_target(RenderTargetData *rt) {
	_immediate_context->OMSetRenderTargets(1, &(rt->rtv.p), rt->dsv);
	_immediate_context->RSSetViewports(1, &_viewport);
}

void Graphics::clear(const XMFLOAT4 &c) {
	_immediate_context->ClearRenderTargetView(_default_render_target->rtv, &c.x);
	_immediate_context->ClearDepthStencilView(_default_render_target->dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
}

void Graphics::present() {
	_swap_chain->Present(0,0);
}

bool Graphics::create_texture_from_file(const char *filename, TextureData *out) {

	out->srv = nullptr;
	out->texture = nullptr;

	if (FAILED(D3DX11CreateShaderResourceViewFromFile(_device, filename, NULL, NULL, &out->srv.p, NULL)))
		return false;

	return true;
}

bool Graphics::create_texture(const D3D11_TEXTURE2D_DESC &desc, TextureData *out)
{
	out->srv = nullptr;
	out->texture = nullptr;

	// create the texture
	ZeroMemory(&out->texture_desc, sizeof(out->texture_desc));
	out->texture_desc = desc;
	if (FAILED(_device->CreateTexture2D(&out->texture_desc, NULL, &out->texture.p)))
		return false;

	// create the shader resource view if the texture has a shader resource bind flag
	if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
		out->srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, out->texture_desc.Format);
		if (FAILED(_device->CreateShaderResourceView(out->texture, &out->srv_desc, &out->srv.p)))
			return false;
	}

	return true;
}
