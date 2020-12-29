#pragma once

#include "Event.h"

#include <sstream>

class KeyboardEvent : public Event {
public:
	virtual ~KeyboardEvent() {}

	EventCategory category() const override { return (EventCategory)(EventCategoryInput | EventCategoryKeyboard); }
	
	int key_code() const { return m_key_code; }

protected:
	KeyboardEvent(int key_code) :
		m_key_code(key_code) {}

private:
	int m_key_code;
};

class KeyPressedEvent final : public KeyboardEvent {
public:
	KeyPressedEvent(int key_code, int repeat) :
		KeyboardEvent(key_code), m_repeat_count(repeat) {}

	EventType type() const override { return EventType::KeyPressed; }
	std::string stringify() const override {
		std::stringstream ss;
		ss << "KeyPressedEvent: " << key_code() << " (" << repeat_count() << " repeats)";
		return ss.str();
	}

	int repeat_count() const { return m_repeat_count; }

private:
	int m_repeat_count = 0;
};

class KeyReleasedEvent final : public KeyboardEvent {
public:
	KeyReleasedEvent(int key_code) :
		KeyboardEvent(key_code) {}

	EventType type() const override { return EventType::KeyReleased; }
	std::string stringify() const override {
		std::stringstream ss;
		ss << "KeyReleasedEvent: " << key_code();
		return ss.str();
	}
};