#pragma once

#include <objbase.h>
#include <wrl/client.h>


namespace uboot {
namespace util {

using Microsoft::WRL::ComPtr;

// RAII wrapper for CoInitialize/CoUninitialize
class ComInit {
public:
  ComInit(DWORD dwCoInit = COINIT_MULTITHREADED) {
    HRESULT hr = CoInitializeEx(nullptr, dwCoInit);
    if (SUCCEEDED(hr)) {
      initialized_ = true;
    }
  }

  ~ComInit() {
    if (initialized_) {
      CoUninitialize();
    }
  }

  bool IsInitialized() const { return initialized_; }

  // Prevent copying
  ComInit(const ComInit &) = delete;
  ComInit &operator=(const ComInit &) = delete;

private:
  bool initialized_ = false;
};

} // namespace util
} // namespace uboot
