#pragma once

#include <Core/Layer.h>

#include <memory>
#include <vector>

class LayerStack {
public:
	using iterator = std::vector<std::unique_ptr<Layer>>::iterator;
	using reverse_iterator = std::vector<std::unique_ptr<Layer>>::reverse_iterator;
	
	~LayerStack() {
		for (size_t i = 0; i < m_layers.size(); i++)
			m_layers[i]->on_detach();
	}

	template<typename T>
	void push_layer(T layer) {
		layer->on_attach();
		m_layers.push_back(std::move(layer));
	}

	iterator begin() { return m_layers.begin(); }
	iterator end() { return m_layers.end(); }
	reverse_iterator rbegin() { return m_layers.rbegin(); }
	reverse_iterator rend() { return m_layers.rend(); }

private:
	std::vector<std::unique_ptr<Layer>> m_layers;
};