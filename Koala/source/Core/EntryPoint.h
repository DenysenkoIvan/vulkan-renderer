#pragma once

#include <Core/Application.h>

// TODO: Delete this line
#include <iostream>

extern std::unique_ptr<Application> create_application();

int main(int argc, char** argv) {
	try {
		auto app = create_application();
		app->run();
	} catch (std::exception& e) {
		std::cout << e.what() << '\n';
	}
}