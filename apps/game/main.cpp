#include "GameApplication.hpp"

#include <iostream>

#ifndef GENOMES_BUILD_GIT_SHA
#define GENOMES_BUILD_GIT_SHA "unknown"
#endif
#ifndef GENOMES_BUILD_STATE
#define GENOMES_BUILD_STATE "unknown"
#endif
#ifndef GENOMES_BUILD_RENDER_PROFILE
#define GENOMES_BUILD_RENDER_PROFILE "unknown"
#endif

int main(int argc, char** argv) {
    std::cout << "Genomes build sha=" << GENOMES_BUILD_GIT_SHA
              << " state=" << GENOMES_BUILD_STATE
              << " renderer=" << GENOMES_BUILD_RENDER_PROFILE << '\n';
    auto application = genomes::game::GameApplication::create();
    if (!application) {
        std::cerr << "Game startup failed: " << application.error().message << '\n';
        return 1;
    }
    return application.value()->run(argc, argv);
}
