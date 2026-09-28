#include "GameApplication.hpp"

#include <iostream>

int main(int argc, char** argv) {
    auto application = genomes::game::GameApplication::create();
    if (!application) {
        std::cerr << "Game startup failed: " << application.error().message << '\n';
        return 1;
    }
    return application.value()->run(argc, argv);
}
