#pragma once

#include <filesystem>

namespace eval::nnue {

// Configure once, before constructing boards or accessing network().
// The architecture and exact byte count must match the bundled network.
void load_network(const std::filesystem::path& path);

}
