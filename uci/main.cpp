#include <iostream>

#include "uci.hpp"

int main() {
    uci::Application application;
    return application.run(std::cin, std::cout);
}
