#pragma once

class LogServer {
public:
	bool init();
	void close();
private:
	static DWORD WINAPI server_thread(LPVOID data);

	HANDLE _server_thread;
	void *_context;
};