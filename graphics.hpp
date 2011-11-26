#pragma once

class Window;
struct RenderTargetData;

class Graphics {
public:
	bool init(const Window *window);
	void clear(const XMFLOAT4 &c);
	void present();

	ID3D11Device *device() { return _device; }
	ID3D11DeviceContext *context() { return _immediate_context; }

	D3D_FEATURE_LEVEL feature_level() { return _feature_level; }
private:

	bool create_back_buffers(int width, int height);
	void set_render_target(RenderTargetData *rt);

	CComPtr<ID3D11Device> _device;
	CComPtr<IDXGISwapChain> _swap_chain;
	CComPtr<ID3D11DeviceContext> _immediate_context;
	CComPtr<ID3D11Debug> _d3d_debug;

	CComPtr<ID3D11RasterizerState> _default_rasterizer_state;
	CComPtr<ID3D11DepthStencilState> _default_depth_stencil_state;
	CComPtr<ID3D11SamplerState> _default_sampler_state;
	float _default_blend_factors[4];
	CComPtr<ID3D11BlendState> _default_blend_state;

	RenderTargetData *_default_render_target;
	D3D11_VIEWPORT _viewport;

	D3D_FEATURE_LEVEL _feature_level;
	DXGI_FORMAT _buffer_format;
};

