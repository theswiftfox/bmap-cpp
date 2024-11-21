#include <iostream>

#define BMAP_COPY_DEBUG_PRINT
#include "bmap.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "Usage: " << std::string(argv[0])
                  << " /tmp/input.wic /dev/sdX" << std::endl;
        std::exit(1);
    }

    const auto wicFilePath = std::string(argv[1]);
    const auto targetDevice = std::string(argv[2]);

    try {
        bmap::copy(wicFilePath, targetDevice);
    } catch (const std::runtime_error &err) {
        std::cerr << "Error during bmap copy: " << err.what() << std::endl;
        std::exit(2);
    }

    return 0;
}