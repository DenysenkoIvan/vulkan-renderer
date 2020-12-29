#pragma once

#include <functional>

#define BIT(x) (1 << x)

enum class EventType {
	None = 0,
	WindowResize, WindowClose,
	KeyPressed, KeyReleased,
	MouseMoved, MouseButtonPressed, MouseButtonReleased
};

enum EventCategory {
	None						= 0,
	EventCategoryApplication	= BIT(0),
	EventCategoryInput			= BIT(1),
	EventCategoryKeyboard		= BIT(2),
	EventCategoryMouse			= BIT(3)
};

class Event {
public:
	using event_handler_fn = std::function<void(Event&)>;

	virtual ~Event() {}

	virtual EventType type() const = 0;
	virtual EventCategory category() const = 0;
	virtual std::string stringify() const = 0;
};

class EventDispatcher {
public:
	EventDispatcher(Event& e) :
		m_event(e) {}

	template<typename T, typename F>
	EventDispatcher& dispatch(const F& handler) {
		if (typeid(m_event) == typeid(T) && !m_is_dispatched) {
			handler(static_cast<T&>(m_event));
			m_is_dispatched = true;
		}
	
		return *this;
	}

	bool is_dispatched() const { return m_is_dispatched; }

private:
	Event& m_event;
	bool m_is_dispatched = false;
};