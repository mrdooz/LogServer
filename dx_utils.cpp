#include "stdafx.h"
#include "dx_utils.hpp"
#include "vertex_types.hpp"
#include "file_utils.hpp"

#pragma comment(lib, "d3dcompiler.lib")

static HRESULT create_buffer_inner(ID3D11Device* device, const D3D11_BUFFER_DESC& desc, const void* data, ID3D11Buffer** buffer)
{
	D3D11_SUBRESOURCE_DATA init_data;
	ZeroMemory(&init_data, sizeof(init_data));
	init_data.pSysMem = data;
	return device->CreateBuffer(&desc, data ? &init_data : NULL, buffer);
}

void screen_to_clip(float x, float y, float w, float h, float *ox, float *oy)
{
	*ox = (x - w / 2) / (w / 2);
	*oy = (h/2 - y) / (h/2);
}

HRESULT create_dynamic_vertex_buffer(ID3D11Device* device, uint32_t vertex_count, uint32_t vertex_size, ID3D11Buffer **vertex_buffer) {
	return create_buffer_inner(device, 
		CD3D11_BUFFER_DESC(vertex_count * vertex_size, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE), 
		NULL, vertex_buffer);
}

HRESULT create_dynamic_index_buffer(ID3D11Device* device, uint32_t index_count, uint32_t index_size, ID3D11Buffer **index_buffer) {
	return create_buffer_inner(device, 
		CD3D11_BUFFER_DESC(index_count * index_size, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE), 
		NULL, index_buffer);
}

HRESULT create_dynamic_constant_buffer(ID3D11Device* device, uint32_t size, ID3D11Buffer **constant_buffer) {
	return create_buffer_inner(device, 
		CD3D11_BUFFER_DESC(size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE), 
		NULL, constant_buffer);
}

bool create_shaders_from_file(const char *filename, D3D_FEATURE_LEVEL level, ID3D11Device *device, 
	const char *vs_entry_point, const char *ps_entry_point, 
	const D3D11_INPUT_ELEMENT_DESC *elems, int num_elems, 
	ID3D11VertexShader **vs, ID3D11PixelShader **ps, ID3D11InputLayout **layout) 
{
	void *buf;
	size_t len;
	if (!load_file(filename, &buf, &len))
		return false;

	const char *vs_level, *ps_level;

	switch (level) {
		case D3D_FEATURE_LEVEL_9_1:
			vs_level = "vs_4_0_level_9_1";
			ps_level = "ps_4_0_level_9_1";
			break;

		case D3D_FEATURE_LEVEL_9_2:
		case D3D_FEATURE_LEVEL_9_3:
			vs_level = "vs_4_0_level_9_3";
			ps_level = "ps_4_0_level_9_3";
			break;

		default:
			vs_level = "vs_4_0";
			ps_level = "ps_4_0";
			break;
	}

	// create the shaders
	ID3D10Blob *vs_blob, *ps_blob, *error_blob;
	if (vs_entry_point) {
		if (FAILED(D3DCompile(buf, len, "", NULL, NULL, vs_entry_point, vs_level, D3D10_SHADER_ENABLE_STRICTNESS, 0, &vs_blob, &error_blob))) {
			if (error_blob) {
				OutputDebugStringA((LPCSTR)error_blob->GetBufferPointer());
				OutputDebugStringA("\n");
			}
			return false;
		}

		if (FAILED(device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), NULL, vs))) {
			return false;
		}
	}

	if (ps_entry_point) {
		if (FAILED(D3DCompile(buf, len, "", NULL, NULL, ps_entry_point, ps_level, D3D10_SHADER_ENABLE_STRICTNESS, 0, &ps_blob, &error_blob))) {
			if (error_blob) {
				OutputDebugStringA((LPCSTR)error_blob->GetBufferPointer());
				OutputDebugStringA("\n");
			}
			return false;
		}

		if (FAILED(device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), NULL, ps)))
			return false;
	}

	// create the input layout
	if (FAILED(device->CreateInputLayout(elems, num_elems, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), layout)))
		return false;

	return true;
}

void make_quad_clip_space(float *x, float *y, int stride_x, int stride_y, float center_x, float center_y, float width, float height)
{
	// 0 1
	// 2 3
	uint8_t *px = (uint8_t *)x;
	uint8_t *py = (uint8_t *)y;

	*(float*)px[0*stride_x] = center_x - width;
	*(float*)py[0*stride_y] = center_y + height;

	*(float*)px[1*stride_x] = center_x + width;
	*(float*)py[1*stride_y] = center_y + height;

	*(float*)px[2*stride_x] = center_x - width;
	*(float*)py[2*stride_y] = center_y - height;

	*(float*)px[3*stride_x] = center_x + width;
	*(float*)py[3*stride_y] = center_y - height;
}

void make_quad_clip_space_unindexed(float *x, float *y, float *tx, float *ty, int stride_x, int stride_y, int stride_tx, int stride_ty, 
	float center_x, float center_y, float width, float height)
{
#define X(n) *x = v ## n.pos.x; x = (float *)((uint8_t *)x + stride_x);
#define Y(n) *y = v ## n.pos.y; y = (float *)((uint8_t *)y + stride_y);
#define U(n) if (tx) { *tx = v ## n.tex.x; tx = (float *)((uint8_t *)tx + stride_tx); }
#define V(n) if (ty) { *ty = v ## n.tex.y; ty = (float *)((uint8_t *)ty + stride_ty); }

	// 0 1
	// 2 3
	PosTex v0(center_x - width, center_y + height, 0, 0, 0);
	PosTex v1(center_x + width, center_y + height, 0, 1, 0);
	PosTex v2(center_x - width, center_y - height, 0, 0, 1);
	PosTex v3(center_x + width, center_y - height, 0, 1, 1);

	// 2, 0, 1
	X(2); Y(2); U(2); V(2);
	X(0); Y(0); U(0); V(0);
	X(1); Y(1); U(1); V(1);

	// 2, 1, 3
	X(2); Y(2); U(2); V(2);
	X(1); Y(1); U(1); V(1);
	X(3); Y(3); U(3); V(3);

#undef X
#undef Y
#undef U
#undef V

}

