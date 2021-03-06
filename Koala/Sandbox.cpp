#include <Koala.h>

#include <fstream>
#include <iostream>
#include <istream>
#include <vector>

class TriangleRender : public Layer {
public:
	void on_attach() override {}

	void on_detach() override {}

	void on_update() override {}

	void on_render() override {}

	bool on_event(Event& e) override { return true; }
};

class Sandbox : public Application {
public:
	Sandbox(ApplicationProperties& props) :
		Application(props)
	{
		push_layer<TriangleRender>();
	}
};

std::unique_ptr<Application> create_application() {
	ApplicationProperties props{};
	props.app_name = "Koala App";
	props.app_version = 1;

	return std::make_unique<Sandbox>(props);
}