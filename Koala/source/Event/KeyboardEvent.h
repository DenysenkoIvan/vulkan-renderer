#pragma once

//#include "Event.h"

#include <sstream>

enum class KeyboardEventType : uint32_t {
	KeyPressed,
	KeyReleased
};

struct KeyPressedEvent {
	int key_code;
	int repeat_count;
};

struct KeyReleasedEvent {
	int key_code;
};

struct KeyboardEvent {
	KeyboardEventType type;
	union {
		KeyPressedEvent key_pressed;
		KeyReleasedEvent key_released;
	};
};