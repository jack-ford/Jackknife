#include "uci.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

}

int main() {
    try {
        std::istringstream input {
            "uci\n"
            "isready\n"
            "setoption name Hash value 1\n"
            "position startpos moves e2e4 e7e5\n"
            "go depth 1 searchmoves g1f3\n"
            "stop\n"
            "position fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1 moves e1g1\n"
            "eval\n"
            "quit\n"
        };
        std::ostringstream output;
        uci::Application application;
        require(application.run(input, output) == EXIT_SUCCESS, "UCI application returned an error");
        std::string transcript = output.str();
        require(transcript.find("id name ChessEngineV2") != std::string::npos, "missing engine identification");
        require(transcript.find("option name Hash") != std::string::npos, "missing Hash option");
        require(transcript.find("uciok") != std::string::npos, "missing uciok");
        require(transcript.find("readyok") != std::string::npos, "missing readyok");
        require(transcript.find("bestmove g1f3") != std::string::npos, "searchmoves was not respected");
        require(transcript.find("info string evaluation") != std::string::npos, "custom eval command failed");
        require(transcript.find("info string error") == std::string::npos, "protocol transcript contains an error");
        std::cout << "UCI protocol tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
