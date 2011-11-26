#pragma once
#include <cassert>
#include "dx_utils.hpp"

template<typename T>
class DynamicBase {
public:
	enum { stride = sizeof(T) };

	DynamicBase(ID3D11Device *device, ID3D11DeviceContext *context)
		: _device(device)
		, _context(context)
		, _org(nullptr)
		, _count(0)
		, _capacity(0)
	{
	}

	virtual bool create(int capacity) = 0;

	T *map() {
		D3D11_MAPPED_SUBRESOURCE r;
		return FAILED(_context->Map(_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &r)) ? NULL : _org = (T*)r.pData;
	}

	int unmap(T *final = NULL) {
		assert(_org);
		_context->Unmap(_buffer, 0);

		// calc # elems inserted
		int num_elems = _count = (final != NULL ? final - _org : -1);
		_org = nullptr;
		return num_elems;
	}

	void fill(const std::vector<T> &v) {
		if ((int)v.size() > capacity())
			create(v.size() * 2);

		T *ptr = map();
		memcpy(ptr, &v[0], v.size() * sizeof(T));
		unmap(ptr + v.size());
	}

	ID3D11Buffer *get() { return _buffer; }
	int count() const { return _count; }
	int capacity() const { return _capacity; }

protected:
	T *_org;
	int _count, _capacity;
	CComPtr<ID3D11Buffer> _buffer;
	ID3D11Device *_device;
	ID3D11DeviceContext *_context;
};

// Helper classes for dynamic vertex and index buffers
template<typename Vtx>
class DynamicVb : public DynamicBase<Vtx> {
public:
	DynamicVb(ID3D11Device *device, ID3D11DeviceContext *context) : DynamicBase(device, context) {}

	bool create(int capacity) {
		_buffer = NULL;
		_capacity = capacity;
		return SUCCEEDED(create_dynamic_vertex_buffer(_device, capacity, sizeof(Vtx), &_buffer));
	}
};


template<typename Idx>
class DynamicIb : public DynamicBase<Idx> {
public:

	DynamicIb(ID3D11Device *device, ID3D11DeviceContext *context) : DynamicBase(device, context) {}

	bool create(int capacity) {
		_capacity = capacity;
		_buffer = nullptr;
		return SUCCEEDED(create_dynamic_index_buffer(_device, capacity, sizeof(Idx), &_buffer));
	}
};
