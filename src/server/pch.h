
#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "log.h"

struct Ratio
{
    uint32_t numerator = 0;
    uint32_t denominator = 1;

    float asFloat() const
    {
        return numerator / static_cast<float>(denominator);
    }
};
