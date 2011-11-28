#pragma once
#include <map>

namespace bmfont {
	struct Info;
	struct Common;
	struct Pages;
	struct CharData;
	struct Chars;
	struct KerningPairs;
}

template<typename Idx> class DynamicIb;
template<typename Vtx> class DynamicVb;

struct TextureData;
class Graphics;

class BmFont {
public:
	BmFont(Graphics *graphics);
	bool create_from_file(const char *filename);
	void render(const char *fmt, ...);
private:
	bool extract_fields(const uint8 *buf, size_t len);
	bool create_resources();

	bmfont::Info *_info;
	bmfont::Common *_common;
	bmfont::Pages *_pages;
	bmfont::Chars *_chars;
	bmfont::KerningPairs *_kerning_pairs;
	std::unique_ptr<const uint8> _buf;

	std::map<uint8, bmfont::CharData *> _character_map;

	Graphics *_graphics;
	CComPtr<ID3D11Device> _device;
	CComPtr<ID3D11DeviceContext> _context;
	std::unique_ptr<DynamicVb<PosTex>> _vb;
	std::unique_ptr<DynamicIb<uint32>> _ib;
	std::unique_ptr<TextureData> _texture;

	CComPtr<ID3D11Buffer> _cb;
	CComPtr<ID3D11InputLayout> _layout;
	CComPtr<ID3D11VertexShader> _vs;
	CComPtr<ID3D11PixelShader> _ps;

	CComPtr<ID3D11RasterizerState> _rs;
	CComPtr<ID3D11DepthStencilState> _dss;
	CComPtr<ID3D11BlendState> _bs;
	CComPtr<ID3D11SamplerState> _sampler;
};
