#pragma once

#include "Event.h"

#include <iostream>
#include <sstream>

class WindowResizeEvent final : public Event {
public:
	WindowResizeEvent(uint32_t width, uint32_t height) :
		m_width(width), m_height(height) {}

	EventType type() const override { return EventType::WindowResize; }
	EventCategory category() const override { return EventCategoryApplication; }
	std::string stringify() const override {
		std::stringstream ss;
		ss << "WindowResizeEvent: (" << m_width << ", " << m_height << ')';
		return ss.str();
	}

	uint32_t width() const { return m_width; }
	uint32_t height() const { return m_height; }

private:
	uint32_t m_width, m_height;
};

class WindowCloseEvent final : public Event {
public:
	WindowCloseEvent() = default;

	EventType type() const override { return EventType::WindowClose; }
	EventCategory category() const override { return EventCategoryApplication; }
	std::string stringify() const override { return "WindowCloseEvent"; }
};