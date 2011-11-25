#pragma once
#include <stdint.h>

namespace log_msg {


#pragma pack(push, 1)

	enum Cmd {
		kCmdBeginFrame,
		kCmdClear,
		kCmdQuad,
		kCmdText,
		kCmdCircle,
		kCmdLine,
		kCmdEndFrame,
	};

	struct Quad {
		int x, y, width, height;
		uint32_t fill_color;
	};

	struct Base {
		Base(Cmd cmd) : cmd(cmd) {}
		Cmd cmd;
	};

	struct BeginFrame : public Base {
		BeginFrame() : Base(kCmdBeginFrame) {}

	};

	struct Clear : public Base {

	};

	struct DrawQuads : public Base {
		uint32_t count;
		Quad *quads;
	};

	struct EndFrame : public Base {

	};

#pragma pack(pop)
}