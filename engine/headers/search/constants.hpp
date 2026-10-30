#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace search {

inline constexpr int MaxPly = 128;
inline constexpr int MateScore = 30000;
inline constexpr int MateThreshold = MateScore - MaxPly;
inline constexpr int Infinity = 32000;
inline constexpr int MaxExtensions = 8;
inline constexpr int HistoryLimit = 16384;
inline constexpr std::size_t HistoryTableSize = 16384;
inline constexpr int PieceSquareCount = 14 * 64;
inline constexpr std::size_t ContinuationHistorySize = PieceSquareCount * PieceSquareCount;
inline constexpr int CorrectionScale = 32;
inline constexpr int CorrectionMaxWeight = 16;
inline constexpr int RazorDepth = 2;
inline constexpr int RazorMargin = 250;
inline constexpr int ProbCutDepth = 5;
inline constexpr int ProbCutMargin = 180;
inline constexpr int ProbCutReduction = 4;
inline constexpr int SingularDepth = 6;
inline constexpr int SingularDepthMargin = 3;
inline constexpr int SingularMargin = 2;
inline constexpr int MaxScoreVolatility = 200;
inline constexpr int EagerMovePieceLimit = 5;
inline constexpr int HistoryBonusMax = 1600;
inline constexpr int HistoryBonusScale = 32;
inline constexpr int HistoryReductionScale = 4096;
inline constexpr int HashMovePriority = 2000000;
inline constexpr int TacticalPriority = 1000000;
inline constexpr int FirstKillerPriority = 900000;
inline constexpr int SecondKillerPriority = 800000;
inline constexpr int VictimValueScale = 16;
inline constexpr int TacticalScoreDivisor = 16;
inline constexpr int AspirationDepth = 4;
inline constexpr int AspirationWindow = 25;
inline constexpr int ReverseFutilityDepth = 4;
inline constexpr int ReverseFutilityMargin = 80;
inline constexpr int FutilityDepth = 5;
inline constexpr int FutilityMargin = 150;
inline constexpr int NullMoveDepth = 2;
inline constexpr int NullVerificationDepth = MaxPly;
inline constexpr int NullReductionBase = 2;
inline constexpr int NullReductionDivisor = 3;
inline constexpr int InternalReductionDepth = 4;
inline constexpr int LateReductionDepth = 2;
inline constexpr int LateReductionMove = 1;
inline constexpr double LateReductionBase = 0.75;
inline constexpr double LateReductionDivisor = 2.0;
inline constexpr int RecaptureDepth = MaxPly;
inline constexpr uint64_t ClockCheckMask = 255;
inline constexpr int AspirationRetries = 4;
inline constexpr int DeltaMargin = 90;
inline constexpr int LosingCapturePriority = -100000;
inline constexpr int MaxLateReduction = 4;
inline constexpr int FutilityCaptureMargin = 75;
inline constexpr int ExchangeReductionScale = 300;
inline constexpr int EvaluationReductionScale = 200;
inline constexpr int TablebaseScore = MateThreshold - 1;
inline constexpr std::size_t DefaultHashMegabytes = 16;
inline constexpr std::size_t MaxHashMegabytes = 1024;
inline constexpr std::size_t BytesPerMegabyte = 1024 * 1024;
inline constexpr std::size_t HashClusterSize = 4;
inline constexpr std::size_t CacheLineSize = 64;
inline constexpr int HashAgePenalty = 8;
inline constexpr int HashExactBonus = 4;
inline constexpr uint64_t MixMultiplierOne = 0xbf58476d1ce4e5b9ULL;
inline constexpr uint64_t MixMultiplierTwo = 0x94d049bb133111ebULL;
inline constexpr uint64_t ExcludedMoveSeed = 0x9e3779b97f4a7c15ULL;
inline constexpr int HashCutoffFiftyLimit = 90;

}
