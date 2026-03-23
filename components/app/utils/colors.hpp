#ifndef _COLORS_HPP_
#define _COLORS_HPP_


#include <cstdint>

namespace color {

    // RGB888 format
    constexpr inline uint32_t RED          = 0xFF0000;
    constexpr inline uint32_t ORANGE       = 0xFF8000;
    constexpr inline uint32_t YELLOW       = 0xFFFF00;
    constexpr inline uint32_t GREEN        = 0x00FF00;
    constexpr inline uint32_t BLUE         = 0x0000FF;
    constexpr inline uint32_t INDIGO       = 0x4B0082;
    constexpr inline uint32_t VIOLET       = 0x8A2BE2;
    constexpr inline uint32_t PURPLE       = 0x800080;
    constexpr inline uint32_t CYAN         = 0x00FFFF;
    constexpr inline uint32_t DARK_BLUE    = 0x00008B;
    constexpr inline uint32_t WHITE        = 0xFFFFFF;
    constexpr inline uint32_t BLACK        = 0x000000;
    constexpr inline uint32_t EERIE_BLACK  = 0x1A1A1A;
    constexpr inline uint32_t GREY         = 0x7F7F7F;
    constexpr inline uint32_t DARK_GREY    = 0x404040;
    constexpr inline uint32_t OFF_WHITE    = 0xF0F0F0;

} // namespace color


#endif // _COLORS_HPP_