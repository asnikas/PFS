#pragma once
#include <cstdint>
#include <vector>

struct Encryption{
    std::vector <uint8_t> generate_key();

    std::vector <uint8_t> gamma(std::vector <uint8_t> data, std::vector <uint8_t> key);
};
