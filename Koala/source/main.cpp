#include "Core/Application.h"
#include <Profile.h>

int main() {
	MY_PROFILE_START("profiling.json");

	ApplicationProperties props{};
	props.app_name = "Koala App";
	props.app_version = 1;
	
	Application app(props);
	
	try {
		app.run();
	} catch (std::exception& e) {
		std::cout << e.what() << '\n';
	}

	MY_PROFILE_END();
}