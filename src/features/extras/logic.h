#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace oc::extras {

enum class TagMode : std::uint8_t { Static, Scroll, Type, Blink };

constexpr std::size_t streakSound(std::size_t Kill, std::size_t Count) {
  if (Count <= 1 || Kill <= 1) {
    return 0;
  }
  return (Kill < Count ? Kill : Count) - 1;
}

constexpr std::string tagFrame(std::string_view Text, TagMode Mode,
                               std::uint64_t Step) {
  const std::size_t Size = Text.size();
  if (Size == 0) {
    return {};
  }
  switch (Mode) {
  case TagMode::Scroll: {
    const std::size_t Shift = Step % Size;
    return std::string(Text.substr(Shift)) +
           std::string(Text.substr(0, Shift));
  }
  case TagMode::Type: {
    const std::size_t At = Step % (2 * Size);
    return std::string(Text.substr(0, At < Size ? At + 1 : Size));
  }
  case TagMode::Blink:
    return Step % 2 == 0 ? std::string(Text) : std::string();
  case TagMode::Static:
    break;
  }
  return std::string(Text);
}

static_assert(streakSound(1, 5) == 0);
static_assert(streakSound(5, 5) == 4);
static_assert(streakSound(9, 5) == 4);
static_assert(streakSound(3, 1) == 0);
static_assert(streakSound(0, 5) == 0);

static_assert(tagFrame("LARP", TagMode::Static, 7) == "LARP");
static_assert(tagFrame("LARP", TagMode::Scroll, 0) == "LARP");
static_assert(tagFrame("LARP", TagMode::Scroll, 1) == "ARPL");
static_assert(tagFrame("LARP", TagMode::Scroll, 4) == "LARP");
static_assert(tagFrame("LARP", TagMode::Type, 0) == "L");
static_assert(tagFrame("LARP", TagMode::Type, 3) == "LARP");
static_assert(tagFrame("LARP", TagMode::Type, 7) == "LARP");
static_assert(tagFrame("LARP", TagMode::Type, 8) == "L");
static_assert(tagFrame("LARP", TagMode::Blink, 0) == "LARP");
static_assert(tagFrame("LARP", TagMode::Blink, 1) == "");
static_assert(tagFrame("", TagMode::Static, 3) == "");
static_assert(tagFrame("", TagMode::Scroll, 3) == "");
static_assert(tagFrame("", TagMode::Type, 3) == "");
static_assert(tagFrame("", TagMode::Blink, 0) == "");

} // namespace oc::extras
