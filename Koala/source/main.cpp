#include "Core/Application.h"

int main() {
	ApplicationProperties props{};
	props.app_name = "Koala App";
	props.app_version = 1;
	
	Application app(props);
	
	try {
		app.run();
	} catch (std::exception& e) {
		std::cout << e.what() << '\n';
	}
}