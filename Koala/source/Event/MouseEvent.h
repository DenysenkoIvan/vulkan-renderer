#pragma once

#include <string>
#include <sstream>

enum class MouseEventType : uint32_t {
	MouseMoved,
	MouseButtonPressed,
	MouseButtonReleased
};

struct MouseMovedEvent {
	int x;
	int y;
};

struct MouseButtonPressedEvent {
	int button;
};

struct MouseButtonReleasedEvent {
	int button;
};

struct MouseEvent {
	MouseEventType type;
	union {
		MouseMovedEvent mouse_moved;
		MouseButtonPressedEvent mouse_button_pressed;
		MouseButtonReleasedEvent mouse_button_released;
	};
};