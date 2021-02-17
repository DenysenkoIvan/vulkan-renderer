#pragma once

#include <string>
#include <sstream>

class MouseEvent : public Event {
public:
	virtual ~MouseEvent() {}

	EventCategory category() const override { return (EventCategory)(EventCategoryMouse | EventCategoryInput); }
};

class MouseMovedEvent final : public MouseEvent {
public:
	virtual ~MouseMovedEvent() {}

	MouseMovedEvent(double xpos, double ypos) :
		m_xpos(xpos), m_ypos(ypos) {}

	EventType type() const override { return EventType::MouseMoved; }
	std::string stringify() const override {
		std::stringstream ss;
		ss << "MouseMovedEvent: (" << m_xpos << ", " << m_ypos << ')';
		return ss.str();
	}

	double x() const { return m_xpos; }
	double y() const { return m_ypos; }

private:
	double m_xpos, m_ypos;
};

class MouseButtonEvent : public MouseEvent {
public:
	int button() const { return m_button; }

protected:
	MouseButtonEvent(int button) :
		m_button(button) {}

private:
	int m_button;
};

class MouseButtonPressedEvent final : public MouseButtonEvent {
public:
	MouseButtonPressedEvent(int button) :
		MouseButtonEvent(button) {}

	EventType type() const override { return EventType::MouseButtonPressed; }
	std::string stringify() const override {
		std::stringstream ss;
		ss << "MouseButtonPressedEvent: " << button();
		return ss.str();
	}
};

class MouseButtonReleasedEvent final : public MouseButtonEvent {
public:
	MouseButtonReleasedEvent(int button) :
		MouseButtonEvent(button) {}

	EventType type() const override { return EventType::MouseButtonReleased; }
	std::string stringify() const override {
		std::stringstream ss;
		ss << "MouseButtonReleasedEvent: " << button();
		return ss.str();
	}
};