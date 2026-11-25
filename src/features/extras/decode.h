#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

namespace oc::extras {

// A complete 16-bit PCM RIFF/WAVE file, ready for PlaySound(SND_MEMORY).
std::vector<std::byte> decodeToWav(const std::filesystem::path &File,
                                   int Volume);

} // namespace oc::extras
