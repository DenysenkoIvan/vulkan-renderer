#pragma once

#include <functional>

#include "ApplicationEvent.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"

enum class EventType : uint32_t {
	Application,
	Keyboard,
	Mouse
};

struct Event {
	EventType type;
	union {
		ApplicationEvent application;
		KeyboardEvent keyboard;
		MouseEvent mouse;
	};
};