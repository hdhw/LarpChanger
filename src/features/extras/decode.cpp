#include "features/extras/decode.h"

#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mmreg.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

namespace oc::extras {

namespace {

template <class T> struct Released {
  T *Ptr = nullptr;
  ~Released() {
    if (Ptr != nullptr) {
      Ptr->Release();
    }
  }
  T **operator&() { return &Ptr; }
  T *operator->() const { return Ptr; }
};

void append(std::vector<std::byte> &Out, const void *Data, std::size_t Size) {
  const auto *const Bytes = static_cast<const std::byte *>(Data);
  Out.insert(Out.end(), Bytes, Bytes + Size);
}

void append32(std::vector<std::byte> &Out, std::uint32_t Value) {
  append(Out, &Value, sizeof(Value));
}

} // namespace

std::vector<std::byte> decodeToWav(const std::filesystem::path &File,
                                   int Volume) {
  Released<IMFSourceReader> Reader;
  if (FAILED(MFCreateSourceReaderFromURL(File.c_str(), nullptr, &Reader))) {
    return {};
  }
  const auto Stream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
  Released<IMFMediaType> Wanted;
  if (FAILED(MFCreateMediaType(&Wanted)) ||
      FAILED(Wanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
      FAILED(Wanted->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
      FAILED(Wanted->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)) ||
      FAILED(Reader->SetCurrentMediaType(Stream, nullptr, Wanted.Ptr))) {
    return {};
  }
  Released<IMFMediaType> Actual;
  WAVEFORMATEX *Format = nullptr;
  UINT32 FormatSize = 0;
  if (FAILED(Reader->GetCurrentMediaType(Stream, &Actual)) ||
      FAILED(MFCreateWaveFormatExFromMFMediaType(Actual.Ptr, &Format,
                                                 &FormatSize))) {
    return {};
  }
  WAVEFORMATEX Header = *Format;
  CoTaskMemFree(Format);
  Header.wFormatTag = WAVE_FORMAT_PCM;
  Header.cbSize = 0;

  std::vector<std::int16_t> Samples;
  for (;;) {
    DWORD Flags = 0;
    Released<IMFSample> Sample;
    if (FAILED(Reader->ReadSample(Stream, 0, nullptr, &Flags, nullptr,
                                  &Sample)) ||
        (Flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
      break;
    }
    if (Sample.Ptr == nullptr) {
      continue;
    }
    Released<IMFMediaBuffer> Buffer;
    BYTE *Data = nullptr;
    DWORD Length = 0;
    if (FAILED(Sample->ConvertToContiguousBuffer(&Buffer)) ||
        FAILED(Buffer->Lock(&Data, nullptr, &Length))) {
      break;
    }
    const std::size_t Count = Length / sizeof(std::int16_t);
    const std::size_t At = Samples.size();
    Samples.resize(At + Count);
    std::memcpy(Samples.data() + At, Data, Count * sizeof(std::int16_t));
    Buffer->Unlock();
  }
  if (Samples.empty()) {
    return {};
  }
  const int Scale = std::clamp(Volume, 0, 100);
  for (std::int16_t &Each : Samples) {
    Each = static_cast<std::int16_t>(Each * Scale / 100);
  }

  const auto DataSize =
      static_cast<std::uint32_t>(Samples.size() * sizeof(std::int16_t));
  std::vector<std::byte> Wav;
  Wav.reserve(44 + DataSize);
  append(Wav, "RIFF", 4);
  append32(Wav, 36 + DataSize);
  append(Wav, "WAVEfmt ", 8);
  append32(Wav, 16);
  append(Wav, &Header, 16);
  append(Wav, "data", 4);
  append32(Wav, DataSize);
  append(Wav, Samples.data(), DataSize);
  return Wav;
}

} // namespace oc::extras
