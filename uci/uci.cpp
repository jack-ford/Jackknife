#include "uci.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

#include "engine_info.hpp"
#include "eval/evaluation.hpp"
#include "search/controller.hpp"
#include "utils/move_utility.hpp"

namespace uci {
namespace {

constexpr std::string_view EngineName = chess::EngineInfo::FullName;
constexpr std::string_view EngineAuthor = chess::EngineInfo::Author;

std::string lower(std::string_view value) {
    std::string result(value);

    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    return result;
}

std::string_view trim(std::string_view value) {
    std::size_t first = value.find_first_not_of(" \t\r\n");

    if (first == std::string_view::npos)
        return {};

    std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

template <typename Integer>
bool parse_integer(std::string_view text, Integer& value) {
    if (text.empty())
        return false;

    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    return error == std::errc {} && end == text.data() + text.size();
}

bool parse_check(std::string_view text, bool& value) {
    std::string normalized = lower(trim(text));
    if (normalized == "true" || normalized == "1" || normalized == "on") {
        value = true;
        return true;
    }
    
    if (normalized == "false" || normalized == "0" || normalized == "off") {
        value = false;
        return true;
    }

    return false;
}

std::vector<std::string> words(std::string_view text) {
    std::istringstream input {std::string(text)};
    std::vector<std::string> result;

    std::string word;
    while (input >> word)
        result.push_back(std::move(word));

    return result;
}

bool is_go_keyword(std::string_view word) {
    static const std::unordered_set<std::string> keywords {
        "searchmoves", "ponder", "wtime", "btime", "winc", "binc", "movestogo",
        "depth", "nodes", "mate", "movetime", "infinite"
    };

    return keywords.contains(lower(word));
}

std::string score_text(int score) {
    if (std::abs(score) >= search::MateThreshold) {
        int plies = search::MateScore - std::abs(score);
        int moves = (plies + 1) / 2;

        if (score < 0)
            moves = -moves;

        return "mate " + std::to_string(moves);
    }

    return "cp " + std::to_string(score);
}

std::string option_value(std::string_view value) {
    value = trim(value);
    return value == "<empty>" ? std::string {} : std::string(value);
}

}

Application::Application(std::string defaultBookFile):
    m_defaultBookFile(std::move(defaultBookFile)), m_board(chess::FenUtility::StartPositionFen) {
    m_settings.bookFile = m_defaultBookFile;
    rebuild_search();
}

Application::~Application() {
    stop_search(false);
}

int Application::run(std::istream& input, std::ostream& output) {
    m_output = &output;

    std::string line;
    while (std::getline(input, line)) {
        try {
            handle_command(line);
        }
        catch (const std::exception& error) {
            send_error(error.what());
        }

        if (lower(trim(line)) == "quit")
            break;
    }

    stop_search(false);

    return 0;
}

void Application::handle_command(std::string_view line) {
    line = trim(line);

    std::size_t separator = line.find_first_of(" \t");
    std::string command = lower(line.substr(0, separator));
    std::string_view arguments = separator == std::string_view::npos ? std::string_view {} : trim(line.substr(separator + 1));

    if (command.empty())
        return;

    if (command == "uci")
        print_identification();
    else if (command == "isready")
        send("readyok");
    else if (command == "debug")
        m_debug = lower(arguments) == "on";
    else if (command == "setoption")
        handle_setoption(arguments);
    else if (command == "register")
        send("registration ok");
    else if (command == "ucinewgame") {
        stop_search(false);
        m_controller->clear();
    }
    else if (command == "position")
        handle_position(arguments);
    else if (command == "go")
        handle_go(arguments);
    else if (command == "stop")
        stop_search(true);
    else if (command == "ponderhit") {
        if (m_pondering) {
            stop_search(false);
            start_search(m_ponderHitLimits, m_ponderHitClock, false);
        }
    }
    else if (command == "quit")
        stop_search(false);
    else if (command == "d") {
        std::ostringstream output;
        output << m_board << "Fen: " << m_board.current_fen();
        send(output.str());
    }
    else if (command == "eval")
        send("info string evaluation " + std::to_string(eval::evaluate(m_board)) + " cp");
    else if (command == "perft")
        handle_perft(arguments);
    else if (command == "bench")
        handle_bench(arguments);
    else if (command == "help")
        send("info string custom commands: d, eval, perft <depth>, bench [depth], help");
    else if (m_debug)
        send_error("unknown command: " + command);
}

void Application::print_identification() {
    send("id name " + std::string(EngineName));
    send("id author " + std::string(EngineAuthor));
    send("option name Hash type spin default 16 min 1 max 1024");
    send("option name Clear Hash type button");
    send("option name Threads type spin default 1 min 1 max 1");
    send("option name Ponder type check default false");
    send("option name MultiPV type spin default 1 min 1 max 256");
    send("option name Move Overhead type spin default 20 min 0 max 5000");
    send("option name UCI_AnalyseMode type check default false");
    send("option name OwnBook type check default false");
    send("option name Book File type string default " + (m_defaultBookFile.empty() ? std::string("<empty>") : m_defaultBookFile));
    send("option name SyzygyPath type string default <empty>");
    send("option name Aspiration Windows type check default true");
    send("option name Principal Variation Search type check default true");
    send("option name Null Move Pruning type check default true");
    send("option name Reverse Futility Pruning type check default true");
    send("option name Futility Pruning type check default true");
    send("option name Late Move Reductions type check default true");
    send("option name SEE Pruning type check default true");
    send("option name Continuation History type check default true");
    send("option name Counter Moves type check default false");
    send("option name Capture History type check default false");
    send("option name Pawn History type check default false");
    send("option name Singular Extensions type check default true");
    send("option name ProbCut type check default false");
    send("option name Razoring type check default true");
    send("option name Correction History type check default true");
    send("uciok");
}

void Application::handle_position(std::string_view arguments) {
    stop_search(false);
    std::vector<std::string> tokens = words(arguments);

    if (tokens.empty())
        throw std::invalid_argument("position requires startpos or fen");

    chess::Board board;
    std::size_t index = 0;
    if (lower(tokens[index]) == "startpos") {
        board.load_start_position();
        index++;
    }
    else if (lower(tokens[index]) == "fen") {
        if (tokens.size() < 7)
            throw std::invalid_argument("position fen requires six FEN fields");

        std::string fen = tokens[1];

        for (std::size_t field = 2; field <= 6; field++)
            fen += " " + tokens[field];

        board.load_position(fen);
        index = 7;
    }
    else {
        throw std::invalid_argument("position requires startpos or fen");
    }

    if (index < tokens.size()) {
        if (lower(tokens[index]) != "moves")
            throw std::invalid_argument("unexpected position argument");

        index++;

        for (; index < tokens.size(); index++) {
            auto move = chess::parse_coordinate_move(board, lower(tokens[index]));
            if (!move)
                throw std::invalid_argument("illegal position move: " + tokens[index]);

            board.make_move(*move);
        }
    }
    m_board = std::move(board);
}

void Application::handle_setoption(std::string_view arguments) {
    stop_search(false);
    std::vector<std::string> tokens = words(arguments);

    if (tokens.empty() || lower(tokens[0]) != "name")
        throw std::invalid_argument("setoption requires name");

    std::size_t valueIndex = tokens.size();
    for (std::size_t index = 1; index < tokens.size(); index++)
        if (lower(tokens[index]) == "value") {
            valueIndex = index;
            break;
        }

    std::string name;
    for (std::size_t index = 1; index < valueIndex; index++)
        name += (name.empty() ? "" : " ") + lower(tokens[index]);

    std::string value;
    for (std::size_t index = valueIndex + (valueIndex < tokens.size()); index < tokens.size(); index++)
        value += (value.empty() ? "" : " ") + tokens[index];

    bool rebuild = false;

    auto setCheck = [&](bool& setting) {
        bool parsed;
        if (!parse_check(value, parsed))
            throw std::invalid_argument("invalid check value for " + name);

        if (setting != parsed) {
            setting = parsed;
            rebuild = true;
        }
    };

    if (name == "clear hash")
        m_controller->clear();
    else if (name == "hash") {
        int parsed = 0;
        if (!parse_integer(value, parsed) || parsed < 1 || parsed > static_cast<int>(search::MaxHashMegabytes))
            throw std::invalid_argument("Hash must be between 1 and 1024");

        if (m_settings.hashMegabytes != static_cast<std::size_t>(parsed)) {
            m_settings.hashMegabytes = parsed;
            rebuild = true;
        }
    }
    else if (name == "threads") {
        int parsed = 0;
        if (!parse_integer(value, parsed) || parsed != 1)
            throw std::invalid_argument("this engine currently supports one search thread");
    }
    else if (name == "multipv") {
        int parsed = 0;
        if (!parse_integer(value, parsed) || parsed < 1 || parsed > chess::MaxMoves)
            throw std::invalid_argument("MultiPV must be between 1 and 256");

        m_settings.multiPv = parsed;
    }
    else if (name == "move overhead") {
        int parsed = 0;
        if (!parse_integer(value, parsed) || parsed < 0 || parsed > 5000)
            throw std::invalid_argument("Move Overhead must be between 0 and 5000");

        m_settings.moveOverhead = std::chrono::milliseconds {parsed};
    }
    else if (name == "ponder")
        setCheck(m_settings.ponder);
    else if (name == "uci_analysemode")
        setCheck(m_settings.analyseMode);
    else if (name == "ownbook") {
        bool parsed;
        if (!parse_check(value, parsed))
            throw std::invalid_argument("invalid check value for OwnBook");

        m_settings.ownBook = parsed;
        configure_book();
    }
    else if (name == "book file") {
        m_settings.bookFile = option_value(value);
        configure_book();
    }
    else if (name == "syzygypath") {
        m_settings.syzygyPath = option_value(value);
        configure_syzygy();
    }
    else if (name == "aspiration windows") setCheck(m_settings.searchOptions.aspirationWindows);
    else if (name == "principal variation search") setCheck(m_settings.searchOptions.principalVariationSearch);
    else if (name == "null move pruning") setCheck(m_settings.searchOptions.nullMovePruning);
    else if (name == "reverse futility pruning") setCheck(m_settings.searchOptions.reverseFutilityPruning);
    else if (name == "futility pruning") setCheck(m_settings.searchOptions.futilityPruning);
    else if (name == "late move reductions") setCheck(m_settings.searchOptions.lateMoveReductions);
    else if (name == "see pruning") setCheck(m_settings.searchOptions.seePruning);
    else if (name == "continuation history") setCheck(m_settings.searchOptions.continuationHistory);
    else if (name == "counter moves") setCheck(m_settings.searchOptions.counterMoves);
    else if (name == "capture history") setCheck(m_settings.searchOptions.captureHistory);
    else if (name == "pawn history") setCheck(m_settings.searchOptions.pawnHistory);
    else if (name == "singular extensions") setCheck(m_settings.searchOptions.singularExtensions);
    else if (name == "probcut") setCheck(m_settings.searchOptions.probCut);
    else if (name == "razoring") setCheck(m_settings.searchOptions.razoring);
    else if (name == "correction history") setCheck(m_settings.searchOptions.correctionHistory);
    else
        throw std::invalid_argument("unknown option: " + name);

    if (rebuild)
        rebuild_search();
}

void Application::handle_go(std::string_view arguments) {
    stop_search(false);
    std::vector<std::string> tokens = words(arguments);
    
    GoCommand command;
    command.limits.depth = search::MaxPly - 1;
    command.limits.multiPv = m_settings.multiPv;
    command.limits.time = std::chrono::milliseconds {0};
    command.limits.softTime = std::chrono::milliseconds {0};

    std::chrono::milliseconds whiteTime {0};
    std::chrono::milliseconds blackTime {0};
    std::chrono::milliseconds whiteIncrement {0};
    std::chrono::milliseconds blackIncrement {0};
    std::chrono::milliseconds moveTime {0};
    
    int movesToGo = 0;
    int mate = 0;
    bool hasClock = false;
    bool hasMoveTime = false;
    bool infinite = false;

    auto nextInteger = [&](std::size_t& index, auto& destination) {
        index++;
        if (index >= tokens.size() || !parse_integer(tokens[index], destination))
            throw std::invalid_argument("invalid go value");
    };

    for (std::size_t index = 0; index < tokens.size(); index++) {
        std::string keyword = lower(tokens[index]);
        if (keyword == "ponder")
            command.ponder = true;
        else if (keyword == "infinite")
            infinite = true;
        else if (keyword == "wtime") { long long value; nextInteger(index, value); whiteTime = std::chrono::milliseconds {value}; hasClock = true; }
        else if (keyword == "btime") { long long value; nextInteger(index, value); blackTime = std::chrono::milliseconds {value}; hasClock = true; }
        else if (keyword == "winc") { long long value; nextInteger(index, value); whiteIncrement = std::chrono::milliseconds {value}; }
        else if (keyword == "binc") { long long value; nextInteger(index, value); blackIncrement = std::chrono::milliseconds {value}; }
        else if (keyword == "movestogo") nextInteger(index, movesToGo);
        else if (keyword == "depth") nextInteger(index, command.limits.depth);
        else if (keyword == "nodes") nextInteger(index, command.limits.nodes);
        else if (keyword == "mate") nextInteger(index, mate);
        else if (keyword == "movetime") {
            long long value;
            nextInteger(index, value);
            moveTime = std::chrono::milliseconds {value};
            hasMoveTime = true;
        }
        else if (keyword == "searchmoves") {
            while (index + 1 < tokens.size() && !is_go_keyword(tokens[index + 1])) {
                auto move = chess::parse_coordinate_move(m_board, lower(tokens[index++]));
                if (!move)
                    throw std::invalid_argument("illegal searchmove: " + tokens[index]);

                if (std::find(command.limits.searchMoves.begin(), command.limits.searchMoves.end(), *move) == command.limits.searchMoves.end())
                    command.limits.searchMoves.push_back(*move);
            }
            if (command.limits.searchMoves.empty())
                throw std::invalid_argument("searchmoves requires at least one legal move");
        }
        else
            throw std::invalid_argument("unknown go argument: " + tokens[index]);
    }

    if (whiteTime.count() < 0 || blackTime.count() < 0 || whiteIncrement.count() < 0 ||
        blackIncrement.count() < 0 || moveTime.count() < 0 || movesToGo < 0)
        throw std::invalid_argument("go times must not be negative");

    if (command.limits.depth < 1 || command.limits.depth >= search::MaxPly || mate < 0)
        throw std::invalid_argument("go depth or mate limit is out of range");

    if (mate > 0) {
        int mateDepth = mate >= search::MaxPly / 2 ? search::MaxPly - 1 : mate * 2;
        command.limits.depth = std::min(command.limits.depth, mateDepth);
    }

    if (hasMoveTime) {
        command.limits.time = std::max(std::chrono::milliseconds {1}, moveTime);
        command.limits.softTime = command.limits.time;
    }
    else if (hasClock && !infinite) {
        search::Clock clock;
        clock.remaining = m_board.is_white_to_move() ? whiteTime : blackTime;
        clock.increment = m_board.is_white_to_move() ? whiteIncrement : blackIncrement;
        clock.overhead = m_settings.moveOverhead;
        clock.movesToGo = movesToGo;
        command.clock = clock;
    }

    if (infinite) {
        command.limits.time = std::chrono::milliseconds {0};
        command.limits.softTime = std::chrono::milliseconds {0};
    }

    command.ponderHitLimits = command.limits;
    command.ponderHitClock = command.clock;
    if (command.ponder) {
        command.limits.time = std::chrono::milliseconds {0};
        command.limits.softTime = std::chrono::milliseconds {0};
        command.clock.reset();
    }

    m_ponderHitLimits = command.ponderHitLimits;
    m_ponderHitClock = command.ponderHitClock;
    start_search(command.limits, command.clock, command.ponder);
}

void Application::start_search(const search::Limits& limits, const std::optional<search::Clock>& clock, bool ponder) {
    m_stopSource = std::stop_source {};
    m_emitBestMove = !ponder;
    m_pondering = ponder;

    chess::Board position = m_board;
    m_worker = std::jthread([this, position = std::move(position), limits, clock]() mutable {
        try {
            auto onIteration = [this](const search::Result& iteration) { print_info(iteration); };
            search::Result result = clock ?
                m_controller->search(std::move(position), *clock, limits, m_stopSource.get_token(), onIteration) :
                m_controller->search(std::move(position), limits, m_stopSource.get_token(), onIteration);

            if (m_emitBestMove.load())
                print_result(result);
        }
        catch (const std::exception& error) {
            send_error(error.what());

            if (m_emitBestMove.load())
                send("bestmove 0000");
        }
    });
}

void Application::stop_search(bool emitBestMove) {
    if (!m_worker.joinable()) {
        m_pondering = false;
        return;
    }

    m_emitBestMove = emitBestMove;
    m_stopSource.request_stop();
    m_worker.join();
    m_pondering = false;
}

void Application::print_info(const search::Result& result) {
    uint64_t elapsed = std::max<int64_t>(0, result.elapsed.count());
    uint64_t nps = elapsed == 0 ? result.nodes * 1000 : result.nodes * 1000 / elapsed;

    for (std::size_t index = 0; index < result.variations.size(); index++) {
        const auto& variation = result.variations[index];
        std::ostringstream line;

        line << "info depth " << result.depth << " seldepth " << result.selectiveDepth;

        if (result.variations.size() > 1)
            line << " multipv " << index + 1;
        line << " score " << score_text(variation.score) << " nodes " << result.nodes
            << " nps " << nps << " time " << elapsed << " pv";

        for (chess::move_t move : variation.moves)
            line << ' ' << chess::coordinate_move(move);

        send(line.str());
    }
}

void Application::print_result(const search::Result& result) {
    std::string line = "bestmove ";
    line += result.bestMove == 0 ? "0000" : chess::coordinate_move(result.bestMove);

    if (result.ponderMove != 0)
        line += " ponder " + chess::coordinate_move(result.ponderMove);

    send(line);
}

void Application::rebuild_search() {
    stop_search(false);
    m_controller = std::make_unique<search::Controller>(m_settings.hashMegabytes, m_settings.searchOptions);
    refresh_integrations();
}

void Application::refresh_integrations() {
    search::Integrations integrations;
    if (m_syzygy)
        integrations = m_syzygy->integrations();

    if (m_book)
        integrations.probeBook = [book = m_book, random = std::mt19937_64(std::random_device {}())](
            const chess::Board& board) mutable {
            return book->probe(board, random());
        };

    m_controller->set_integrations(std::move(integrations));
}

void Application::configure_book() {
    if (!m_settings.ownBook || m_settings.bookFile.empty()) {
        m_book.reset();
        refresh_integrations();
        return;
    }

    auto book = std::make_shared<search::OpeningBook>();
    book->load(m_settings.bookFile);
    m_book = std::move(book);
    refresh_integrations();

    send("info string opening book loaded: " + std::to_string(m_book->size()) + " positions");
}

void Application::configure_syzygy() {
    m_controller->set_integrations({});
    m_syzygy.reset();

    if (!m_settings.syzygyPath.empty()) {
        m_syzygy = std::make_shared<search::Syzygy>(m_settings.syzygyPath);
        send("info string Syzygy loaded: up to " + std::to_string(m_syzygy->max_pieces()) + " pieces");
    }

    refresh_integrations();
}

uint64_t Application::perft(chess::Board& board, int depth) {
    if (depth == 0)
        return 1;

    chess::MoveGenerator generator;
    chess::MoveList moves = generator.generate_moves(board);

    uint64_t nodes = 0;
    for (chess::move_t move : moves) {
        board.make_move(move, true);
        nodes += perft(board, depth - 1);
        board.unmake_move(move, true);
    }

    return nodes;
}

void Application::handle_perft(std::string_view arguments) {
    stop_search(false);
    int depth = 0;

    if (!parse_integer(trim(arguments), depth) || depth < 0 || depth > 16)
        throw std::invalid_argument("perft depth must be between 0 and 16");

    chess::Board board = m_board;
    auto start = std::chrono::steady_clock::now();
    if (depth == 0) {
        send("Nodes searched: 1");
        send("Time: 0 ms");
        return;
    }

    uint64_t total = 0;
    chess::MoveGenerator generator;
    for (chess::move_t move : generator.generate_moves(board)) {
        board.make_move(move, true);
        uint64_t nodes = perft(board, depth - 1);
        board.unmake_move(move, true);
        total += nodes;

        send(chess::coordinate_move(move) + ": " + std::to_string(nodes));
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    send("Nodes searched: " + std::to_string(total));
    send("Time: " + std::to_string(elapsed.count()) + " ms");
}

void Application::handle_bench(std::string_view arguments) {
    stop_search(false);
    int depth = 6;

    if (!trim(arguments).empty() && (!parse_integer(trim(arguments), depth) || depth < 1 || depth >= search::MaxPly))
        throw std::invalid_argument("bench depth is out of range");

    search::Limits limits;
    limits.depth = depth;
    limits.time = std::chrono::milliseconds {0};
    limits.multiPv = 1;

    auto start = std::chrono::steady_clock::now();
    search::Result result = m_controller->search(m_board, limits);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    send("info string bench depth " + std::to_string(result.depth) + " nodes " + std::to_string(result.nodes) +
        " time " + std::to_string(elapsed.count()) + " bestmove " +
        (result.bestMove == 0 ? std::string("0000") : chess::coordinate_move(result.bestMove)));
}

void Application::send(std::string_view line) {
    std::lock_guard lock(m_outputMutex);
    if (m_output != nullptr)
        *m_output << line << std::endl;
}

void Application::send_error(std::string_view message) {
    send("info string error: " + std::string(message));
}

}
