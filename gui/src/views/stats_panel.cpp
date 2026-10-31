#include "views/stats_panel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#include "search/constants.hpp"
#include "ui/draw.hpp"
#include "ui/theme.hpp"

namespace GUI::StatsPanel {

namespace {

constexpr float Left = 1020.0f;
constexpr float Right = 1260.0f;
constexpr float Width = 240.0f;
constexpr SDL_FRect EvalBar {962.0f, 48.0f, 10.0f, 624.0f};
constexpr float MainRowY = 124.0f;
constexpr float MainRowSpacing = 34.0f;
constexpr float CounterRowY = 322.0f;
constexpr float CounterRowSpacing = 22.0f;
constexpr float LineRowY = 626.0f;
constexpr float LineRowSpacing = 24.0f;
constexpr std::array<float, 3> DividerY {106.0f, 306.0f, 582.0f};
constexpr std::size_t PreviewMoves = 5;
constexpr double EvalBarScale = 400.0;
constexpr double CentipawnsPerPawn = 100.0;
constexpr std::uint64_t MillisecondsPerSecond = 1000;

bool is_mate(int score) {
    return std::abs(score) >= search::MateThreshold;
}

int white_score(const Game& game, int score) {
    return game.result_color() == chess::PieceColor::White ? score : -score;
}

std::string score_text(int score) {
    if (is_mate(score)) {
        int moves = (search::MateScore - std::abs(score) + 1) / 2;
        return std::string(score < 0 ? "-M" : "M") + std::to_string(moves);
    }

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%+.2f", score / CentipawnsPerPawn);
    return buffer;
}

std::string move_text(chess::move_t move) {
    chess::square_t from = chess::Move::from(move);
    chess::square_t to = chess::Move::to(move);

    if (chess::Move::type(move) == chess::Move::Type::Castling) {
        chess::PieceColor color = chess::Square::rank(from) == 0 ? chess::PieceColor::White : chess::PieceColor::Black;
        to = chess::Square::castling_king_square(to > from, color);
    }

    std::string text = chess::Square::to_string(from) + chess::Square::to_string(to);

    if (chess::Move::type(move) == chess::Move::Type::Promotion)
        text += chess::Piece::symbol(chess::Piece::create(chess::Move::promotion_piece(move), chess::PieceColor::Black));

    return text;
}

std::string count_text(std::uint64_t value) {
    std::string digits = std::to_string(value);
    std::string text;

    for (std::size_t index = 0; index < digits.size(); index++) {
        if (index > 0 && (digits.size() - index) % 3 == 0)
            text += ',';

        text += digits[index];
    }

    return text;
}

void render_eval_bar(SDL_Renderer* renderer, const Game& game, bool flipped) {
    const search::Result& result = game.result();
    double whiteShare = 0.5;

    if (!result.variations.empty()) {
        int score = white_score(game, result.variations.front().score);
        whiteShare = is_mate(score) ? (score > 0 ? 1.0 : 0.0) : 0.5 + 0.5 * std::tanh(score / EvalBarScale);
    }

    float whiteHeight = static_cast<float>(whiteShare) * EvalBar.h;
    float whiteTop = flipped ? EvalBar.y : EvalBar.y + EvalBar.h - whiteHeight;

    Draw::fill_rect(renderer, EvalBar, Theme::Button);
    Draw::fill_rect(renderer, {EvalBar.x, whiteTop, EvalBar.w, whiteHeight}, Theme::Text);
}

}

void render(SDL_Renderer* renderer, Text& text, const Game& game, bool flipped) {
    render_eval_bar(renderer, game, flipped);

    const search::Result& result = game.result();
    const search::Statistics& stats = result.statistics;
    bool hasResult = !result.variations.empty();
    bool bookMove = hasResult && result.depth == 0 && result.nodes == 0;

    std::string evaluation = "-";

    if (bookMove)
        evaluation = "Book";
    else if (hasResult)
        evaluation = score_text(white_score(game, result.variations.front().score));

    text.draw(Font::Heading, evaluation, Left, 20.0f, Theme::Accent);
    text.draw(Font::Small, game.thinking() ? "Evaluation (white), thinking" : "Evaluation (white)", Left + 1.0f, 70.0f, Theme::DimText);

    for (float y : DividerY)
        Draw::fill_rect(renderer, {Left, y, Width, 1.0f}, Theme::Line);

    long long elapsed = static_cast<long long>(result.elapsed.count());
    std::uint64_t nps = elapsed > 0 ? result.nodes * MillisecondsPerSecond / static_cast<std::uint64_t>(elapsed) : 0;

    const std::array<std::pair<const char*, std::string>, 5> main {{
        {"Best Move", result.bestMove != 0 ? move_text(result.bestMove) : "-"},
        {"Depth", std::to_string(result.depth) + " / " + std::to_string(result.selectiveDepth)},
        {"Nodes", count_text(result.nodes)},
        {"NPS", count_text(nps)},
        {"Time", std::to_string(elapsed) + " ms"}
    }};

    for (std::size_t row = 0; row < main.size(); row++) {
        float y = MainRowY + MainRowSpacing * static_cast<float>(row);
        text.draw(Font::Text, main[row].first, Left, y, Theme::DimText);
        text.draw_right(Font::Text, main[row].second, Right, y, Theme::Text);
    }

    const std::array<std::pair<const char*, std::uint64_t>, 11> counters {{
        {"Hash Hits", stats.hashHits},
        {"Hash Cutoffs", stats.hashCutoffs},
        {"Null Cutoffs", stats.nullCutoffs},
        {"Late Move Reductions", stats.lateReductions},
        {"LMR Re-searches", stats.lateResearches},
        {"PVS Re-searches", stats.pvsResearches},
        {"Aspiration Re-searches", stats.aspirationResearches},
        {"Futility Prunes", stats.futilityPrunes},
        {"SEE Prunes", stats.seePrunes},
        {"Singular Extensions", stats.singularExtensions},
        {"Tablebase Hits", stats.tablebaseHits}
    }};

    for (std::size_t row = 0; row < counters.size(); row++) {
        float y = CounterRowY + CounterRowSpacing * static_cast<float>(row);
        text.draw(Font::Small, counters[row].first, Left, y, Theme::DimText);
        text.draw_right(Font::Small, count_text(counters[row].second), Right, y, Theme::Text);
    }

    text.draw(Font::Small, "Lines", Left, 598.0f, Theme::DimText);

    for (std::size_t index = 0; index < result.variations.size(); index++) {
        const search::Variation& variation = result.variations[index];
        std::string line = score_text(white_score(game, variation.score)) + "  ";

        for (std::size_t ply = 0; ply < std::min(PreviewMoves, variation.moves.size()); ply++)
            line += move_text(variation.moves[ply]) + " ";

        text.draw(Font::Small, line, Left, LineRowY + LineRowSpacing * static_cast<float>(index), Theme::Text);
    }
}

}
