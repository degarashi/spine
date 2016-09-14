// frea: 1b6d5e89ed91eb796bc3604d225a0c1f8742f515 からのコピー
#pragma once
#include <cstdint>
#include <type_traits>

#define ENABLE_IF_I(exp) typename std::enable_if_t<exp, std::nullptr_t>
#define ENABLE_IF(exp) ENABLE_IF_I(exp) = nullptr
