#pragma once
#include <stdint.h>
#include <zmq.hpp>

namespace log_msg {


#pragma pack(push, 1)

	enum Cmd {
		kCmdSetupWindow,
		kCmdBeginFrame,
		kCmdClear,
		kCmdQuad,
		kCmdText,
		kCmdCircle,
		kCmdLine,
		kCmdEndFrame,
	};

	struct Quad {
		Quad() {}
		Quad(int x, int y, int width, int height, uint32_t color) : x(x), y(y), width(width), height(height), fill_color(color) {}
		int x, y, width, height;
		uint32_t fill_color;  // RGBA
	};

	struct Base {
		Base(Cmd cmd) : cmd(cmd) {}
		Cmd cmd;
	};

	struct SetupWindow : public Base {
		SetupWindow(int width, int height) : Base(kCmdSetupWindow), width(width), height(height) {}
		int width, height;
	};

	struct BeginFrame : public Base {
		BeginFrame() : Base(kCmdBeginFrame) {}
	};

	struct Clear : public Base {
		Clear() : Base(kCmdClear) {}
	};

	struct DrawQuads : public Base {
		DrawQuads(int count) : Base(kCmdQuad), count(count) {}
		uint32_t count;
#pragma warning(suppress: 4200)
		Quad quads[0];
	};

	struct EndFrame : public Base {
		EndFrame() : Base(kCmdEndFrame) {}

	};

	template <class T>
	void send_msg(zmq::socket_t *socket, const T &t) {

		zmq::message_t msg(sizeof(T));
		T *var = new(msg.data())T(t);
		socket->send(msg);
	}

	inline void send_quads(zmq::socket_t *socket, const std::vector<log_msg::Quad> &quads) {
		int count = quads.size();
		int size = count * sizeof(log_msg::Quad);
		zmq::message_t msg(sizeof(log_msg::DrawQuads) + size);
		new(msg.data())log_msg::DrawQuads(count);

		void *dst = (void *)((uintptr_t)msg.data() + offsetof(log_msg::DrawQuads, quads));
		memcpy(dst, &quads[0], size);
		socket->send(msg);
	}


#pragma pack(pop)
}
