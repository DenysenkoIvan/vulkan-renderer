#pragma once

#include <Event/Event.h>

class Layer {
public:
	virtual ~Layer() {}

	virtual void on_attach() = 0;
	virtual void on_detach() = 0;
	virtual void on_update() = 0;
	virtual bool on_event(Event& e) = 0;
	virtual void on_render() = 0;
};