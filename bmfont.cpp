#include "stdafx.h"
#include "bmfont.hpp"
#include "file_utils.hpp"
#include "string_utils.hpp"
#include "graphics.hpp"
#include "dynamic_buffers.hpp"
#include "utils.hpp"

using namespace std;

// bmfont gen format
namespace bmfont {
#pragma pack(push, 1)
	struct Header {
		char bmf[3];
		uint8 version;
	};

	struct BlockBase {
		uint8 id;
		uint32 len;
	};

	struct Info : public BlockBase {
		int16 fontSize;      //	2		int 	0 	
		//	1		bits 	2 	bit 0: smooth, bit 1: unicode, bit 2: italic, bit 3: bold, bit 4: fixedHeigth, bits 5-7: reserved
uint8 : 3; // reserved
		uint8 fixedHeight : 1;
		uint8 bold : 1;
		uint8 italic : 1;
		uint8 unicode : 1;
		uint8 smooth : 1;
		uint8 charSet;       // 1		uint 	3 	
		uint16 stretchH;     // 2		uint 	4 	
		uint8 aa;            //	1 	uint 	6 	
		uint8 paddingUp;     // 1 	uint 	7 	
		uint8 paddingRight;  // 1 	uint 	8 	
		uint8 paddingDown;   // 1 	uint 	9 	
		uint8 paddingLeft;   // 1 	uint 	10	
		uint8 spacingHoriz;  // 1 	uint 	11	
		uint8 spacingVert;   // 1 	uint 	12	
		uint8 outline;       //	1 	uint 	13	added with version 2
		char fontName[0];    //	n+1	string 	14	null terminated string with length n
	};

	struct Common : public BlockBase  {
		uint16 lineHeight; // 	2 	uint 	0
		uint16 base; // 	2 	uint 	2
		uint16 scaleW; // 	2 	uint 	4
		uint16 scaleH; // 	2 	uint 	6
		uint16 pages; // 	2 	uint 	8
		uint8 packed : 1;  //	1 	bits 	10 	bits 0-6: reserved, bit 7: packed
		uint8 : 7;	
		uint8 alphaChnl; // 	1 	uint 	11 	
		uint8 redChnl; // 	1 	uint 	12 	
		uint8 greenChnl; // 	1 	uint 	13 	
		uint8 blueChnl; // 	1 	uint 	14 	
	};

	struct Pages : public BlockBase {
		char pageNames[0];  // 	p null terminated strings, each with length n
	};

	struct CharData {
		uint32 id;       //  	4 	uint 	0+c*20 	These fields are repeated until all characters have been described
		uint16 x;        // 	2 	uint 	4+c*20 	
		uint16 y;        //  	2 	uint 	6+c*20 	
		uint16 width;    // 	2 	uint 	8+c*20 	
		uint16 height;   // 	2 	uint 	10+c*20 	
		uint16 xoffset;  // 	2 	int 	12+c*20 	
		uint16 yoffset;  // 	2 	int 	14+c*20 	
		uint16 xadvance; // 	2 	int 	16+c*20 	
		uint8 page;      // 	1 	uint 	18+c*20 	
		uint8 chnl;      // 	1 	uint 	19+c*20
	};

	struct Chars : public BlockBase {
		CharData data[0];
	};

	struct KerningPairs : public BlockBase {
		struct Elem {
			uint32 first;  // 	4 	uint 	0+c*10 	These fields are repeated until all kerning pairs have been described
			uint32 second; // 	4 	uint 	4+c*10 	
			uint16 amount; // 	2 	int 	8+c*6
		} data[0];
	};

#pragma pack(pop)
}


BmFont::BmFont(Graphics *graphics)
	: _info(nullptr)
	, _common(nullptr)
	, _pages(nullptr)
	, _chars(nullptr)
	, _kerning_pairs(nullptr)
	, _graphics(graphics)
	, _device(graphics->device())
	, _context(graphics->context())
{
}

bool BmFont::extract_fields(const uint8 *buf, size_t len) {

	bmfont::Header *header = (bmfont::Header *)buf;
	if (header->bmf[0] != 'B' || header->bmf[1] != 'M' || header->bmf[2] != 'F' || header->version != 3)
		return false;

	const uint8 *ptr = buf + sizeof(bmfont::Header);
	while (ptr < buf + len) {
		bmfont::BlockBase *b = (bmfont::BlockBase *)ptr;
		switch (b->id) {
		case 1: _info = (bmfont::Info *)ptr; break;
		case 2: _common = (bmfont::Common *)ptr; break;
		case 3: _pages = (bmfont::Pages *)ptr; break;
		case 4: 
			_chars = (bmfont::Chars *)ptr; 
			for (uint32 i = 0; i < b->len / sizeof(bmfont::CharData); ++i)
				_character_map[_chars->data[i].id] = &_chars->data[i];
			break;
		case 5: _kerning_pairs = (bmfont::KerningPairs *)ptr; break;
		default:
			return false;
		}
		ptr += b->len + sizeof(bmfont::BlockBase);
	}

	return true;
}

bool BmFont::create_from_file(const char *filename) {

	uint8 *buf;
	size_t len;
	if (!load_file(filename, (void **)&buf, &len))
		return false;

	_buf.reset(buf);

	if (!extract_fields(buf, len))
		return false;

	_texture.reset(new TextureData);
	if (!_graphics->create_texture_from_file((const char *)&_pages->pageNames[0], _texture.get()))
		return false;

	if (!create_resources())
		return false;


	return true;
}

bool BmFont::create_resources() {

	D3D11_INPUT_ELEMENT_DESC desc[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	if (!create_shaders_from_file("bmfont.hlsl", _graphics->feature_level(), _device, "vs_main", "ps_main", desc, ELEMS_IN_ARRAY(desc), &_vs.p, &_ps.p, &_layout.p))
		return false;

	if (FAILED(create_dynamic_constant_buffer(_device, sizeof(XMMATRIX), &_cb.p)))
		return false;

	CD3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
	CD3D11_RASTERIZER_DESC rasterize_desc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
	CD3D11_DEPTH_STENCIL_DESC dss_desc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
	CD3D11_SAMPLER_DESC sampler_desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());

	dss_desc.DepthEnable = FALSE;

	if (FAILED(_device->CreateDepthStencilState(&dss_desc, &_dss.p)))
		return false;

	if (FAILED(_device->CreateRasterizerState(&rasterize_desc, &_rs.p)))
		return false;

	if (FAILED(_device->CreateBlendState(&blend_desc, &_bs.p)))
		return false;

	if (FAILED(_device->CreateSamplerState(&sampler_desc, &_sampler.p)))
		return false;

	return true;
}

void BmFont::render(const char *fmt, ...) {

	va_list arg;
	va_start(arg, fmt);

	const int len = _vscprintf(fmt, arg) + 1;
	char* buf = (char*)_alloca(len);
	vsprintf_s(buf, len, fmt, arg);
	va_end(arg);

	// see if we need to resize the buffers
	const int num_verts = len * 4;
	const int num_indices = len * 6;

	if (!_vb) {
		_vb.reset(new DynamicVb<PosTex>(_device, _context));
		_ib.reset((new DynamicIb<uint32>(_device, _context)));
	}

	if (num_verts > _vb->capacity()) {
		_vb->create(num_verts * 2);
		_ib->create(num_indices * 2);
	}

	PosTex *vtx = _vb->map();
	uint32 *idx = _ib->map();

	float cursor_x = 0;
	float cursor_y = 480;

	float w = _common->scaleW;
	float h = _common->scaleH;

	for (int i = 0; i < len; ++i) {
		uint8	 cur = buf[i];
		auto it = _character_map.find(cur);
		if (it == _character_map.end())
			continue;

		bmfont::CharData *data = it->second;

		float cx = cursor_x + data->xoffset;
		float cy = cursor_y - data->yoffset;

		// 1, 2
		// 0, 3
		*vtx++ = PosTex(cx, cy - data->height, 1, data->x / w, (data->y + data->height) / h);
		*vtx++ = PosTex(cx, cy, 1, data->x / w, data->y / h);
		*vtx++ = PosTex(cx + data->width, cy, 1, (data->x + data->width)/ w, data->y / h);
		*vtx++ = PosTex(cx + data->width, cy - data->height, 1, (data->x + data->width)/ w, (data->y + data->height) / h);

		*idx++ = i*4+0; *idx++ = i*4+1; *idx++ = i*4+2;
		*idx++ = i*4+0; *idx++ = i*4+2; *idx++ = i*4+3;

		cursor_x += data->xadvance;
	}

	int verts = _vb->unmap(vtx);
	int indices = _ib->unmap(idx);


	XMMATRIX mtx_proj = XMMatrixOrthographicOffCenterLH(0, 640, 0, 480, 1, 100);

	ID3D11Device *device = _device;
	ID3D11DeviceContext *context = _context;

	ID3D11Buffer *bufs[] = { _vb->get() };
	UINT strides[] = { sizeof(PosTex) };
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
	context->PSSetSamplers(0, 1, &(_sampler.p));
	context->PSSetShaderResources(0, 1, &(_texture->srv.p));

	context->DrawIndexed(indices, 0, 0);

	_graphics->present();


}