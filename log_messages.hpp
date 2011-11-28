#pragma once
#include <stdint.h>

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

#pragma pack(pop)
}
