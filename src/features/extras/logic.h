#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace oc::extras {

enum class TagMode : std::uint8_t { Static, Scroll, Type, Blink };

struct Settings {
  std::string Pack;
  int Volume = 80;
  std::string Tag;
  TagMode Mode = TagMode::Static;
  int Speed = 50;
  bool TagOn = false;
};

constexpr std::size_t MaxTag = 12;

constexpr std::vector<std::string_view> charsOf(std::string_view Text) {
  std::vector<std::string_view> Chars;
  std::size_t Begin = 0;
  for (std::size_t At = 1; At <= Text.size(); ++At) {
    if (At == Text.size() ||
        (static_cast<unsigned char>(Text[At]) & 0xC0U) != 0x80U) {
      Chars.push_back(Text.substr(Begin, At - Begin));
      Begin = At;
    }
  }
  return Chars;
}

constexpr std::string joined(const std::vector<std::string_view> &Chars,
                             std::size_t From, std::size_t To) {
  std::string Out;
  for (std::size_t At = From; At < To; ++At) {
    Out += Chars[At];
  }
  return Out;
}

constexpr std::string truncateChars(std::string_view Text, std::size_t Max) {
  const auto Chars = charsOf(Text);
  return joined(Chars, 0, Chars.size() < Max ? Chars.size() : Max);
}

// Only short ASCII-digit stems count as numbered; anything else sorts as 0.
constexpr int packOrder(std::wstring_view Stem) {
  if (Stem.empty() || Stem.size() > 6) {
    return 0;
  }
  int Value = 0;
  for (const wchar_t Digit : Stem) {
    if (Digit < L'0' || Digit > L'9') {
      return 0;
    }
    Value = Value * 10 + (Digit - L'0');
  }
  return Value;
}

constexpr int clamped(int Value, int Low, int High) {
  return Value < Low ? Low : (Value > High ? High : Value);
}

constexpr Settings sanitized(Settings Value) {
  Value.Volume = clamped(Value.Volume, 0, 100);
  Value.Speed = clamped(Value.Speed, 1, 100);
  if (static_cast<std::uint8_t>(Value.Mode) >
      static_cast<std::uint8_t>(TagMode::Blink)) {
    Value.Mode = TagMode::Static;
  }
  Value.Tag = truncateChars(Value.Tag, MaxTag);
  return Value;
}

constexpr std::size_t streakSound(std::size_t Kill, std::size_t Count) {
  if (Count <= 1 || Kill <= 1) {
    return 0;
  }
  return (Kill - 1) % Count;
}

constexpr std::string tagFrame(std::string_view Text, TagMode Mode,
                               std::uint64_t Step) {
  const auto Chars = charsOf(Text);
  const std::size_t Size = Chars.size();
  if (Size == 0) {
    return {};
  }
  switch (Mode) {
  case TagMode::Scroll: {
    const std::size_t Shift = Step % Size;
    return joined(Chars, Shift, Size) + joined(Chars, 0, Shift);
  }
  case TagMode::Type: {
    const std::size_t At = Step % (2 * Size);
    return joined(Chars, 0, At < Size ? At + 1 : Size);
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
static_assert(streakSound(6, 5) == 0);
static_assert(streakSound(9, 5) == 3);
static_assert(streakSound(10, 5) == 4);
static_assert(streakSound(11, 5) == 0);
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

static_assert(tagFrame("АБ", TagMode::Scroll, 1) ==
              "БА");
static_assert(tagFrame("АБ", TagMode::Type, 0) == "А");
static_assert(tagFrame("a💀", TagMode::Type, 1) == "a💀");

static_assert(truncateChars("LARPLARPLARPLARP", 12) == "LARPLARPLARP");
static_assert(truncateChars("АБВ", 2) == "АБ");
static_assert(truncateChars("ab", 12) == "ab");

static_assert(packOrder(L"1") == 1);
static_assert(packOrder(L"05") == 5);
static_assert(packOrder(L"intro") == 0);
static_assert(packOrder(L"") == 0);
static_assert(packOrder(L"20241124153000") == 0);
static_assert(packOrder(L"١") == 0);

static_assert(sanitized({.Speed = 0}).Speed == 1);
static_assert(sanitized({.Speed = 500}).Speed == 100);
static_assert(sanitized({.Volume = -3}).Volume == 0);
static_assert(sanitized({.Mode = static_cast<TagMode>(7)}).Mode ==
              TagMode::Static);
static_assert(sanitized({.Tag = "LARPLARPLARPLARP"}).Tag == "LARPLARPLARP");

} // namespace oc::extras
