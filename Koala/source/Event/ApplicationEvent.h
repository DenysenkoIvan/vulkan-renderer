#pragma once

//#include "Event.h"

#include <iostream>
#include <sstream>

enum class ApplicationEventType : uint32_t {
	WindowResizeEvent,
	WindowCloseEvent
};

struct WindowResizeEvent {
	int width;
	int height;
};

struct WindowCloseEvent {
	
};

struct ApplicationEvent {
	ApplicationEventType type;
	union {
		WindowResizeEvent window_resize;
		WindowCloseEvent window_close;
	};
};