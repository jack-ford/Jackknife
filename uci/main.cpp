#include <filesystem>
#include <iostream>
#include <system_error>

#include "uci.hpp"

int main(int argc, char** argv) {
    std::string defaultBook;
    if (argc > 0) {
        std::error_code error;
        auto book = std::filesystem::absolute(argv[0], error).parent_path() / search::DefaultBookPath;
        if (!error && std::filesystem::is_regular_file(book, error)) defaultBook = book.string();
    }

    uci::Application application(std::move(defaultBook));
    return application.run(std::cin, std::cout);
}
