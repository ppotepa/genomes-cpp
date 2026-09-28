#include <genomes/foundation/BuildInfo.hpp>

#include <iostream>

int main() {
    const genomes::foundation::BuildInfo info = genomes::foundation::buildInfo();
    if (info.projectName != "Genomes") {
        std::cerr << "unexpected native project name\n";
        return 1;
    }
    if (info.nativeBootstrapVersion != "0.1") {
        std::cerr << "unexpected native bootstrap version\n";
        return 1;
    }
    return 0;
}
