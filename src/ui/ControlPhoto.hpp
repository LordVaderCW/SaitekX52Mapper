#pragma once
#include <array>
#include <string_view>

namespace x52 {
// Positions refer to the original manufacturer images, before the display crop.
// These are visual annotations only, never HID mappings. Zero radius = occluded.
struct PhotoSpot { float x{}, y{}, radius{}; };
struct ControlPhoto {
    std::string_view id;
    std::array<PhotoSpot, 5> spots; // front, angled, throttle detail, complete, side
    int preferred{};
    bool areaOnly{};
};
inline const ControlPhoto* FindControlPhoto(std::string_view id)
{
    static const ControlPhoto annotations[]{
        {"stick.x", {{{374, 232, 54}, {382, 218, 54}, {}, {489, 211, 45}, {369, 220, 48}}}},
        {"stick.y", {{{374, 232, 54}, {382, 218, 54}, {}, {489, 211, 45}, {369, 220, 48}}}},
        {"stick.twist", {{{374, 232, 54}, {382, 218, 54}, {}, {489, 211, 45}, {369, 220, 48}}}},
        {"stick.trigger1", {{{}, {426, 175, 22}, {}, {435, 153, 20}, {419, 160, 24}}}, 4},
        {"stick.trigger2", {{{}, {426, 175, 22}, {}, {435, 153, 20}, {419, 160, 24}}}, 4},
        {"stick.fire", {{{378, 77, 23}, {412, 67, 22}, {}, {459, 67, 21}, {399, 66, 20}}}},
        {"stick.a", {{{407, 76, 18}, {432, 68, 17}, {}, {485, 65, 15}, {397, 79, 17}}}},
        {"stick.b", {{{403, 107, 15}, {422, 96, 14}, {}, {487, 91, 14}, {385, 104, 14}}}},
        {"stick.c", {{{338, 117, 21}, {379, 106, 18}, {}, {442, 113, 18}, {370, 117, 16}}}},
        {"stick.pinkie", {{{}, {390, 300, 20}, {}, {466, 270, 18}, {396, 292, 22}}}, 4},
        {"stick.hat_upper", {{{341, 76, 26}, {384, 69, 23}, {}, {437, 79, 22}, {387, 82, 19}}}},
        {"stick.hat_lower", {{{379, 127, 25}, {395, 110, 24}, {}, {478, 113, 22}, {374, 118, 19}}}},
        {"stick.mode", {{{432, 77, 24}, {462, 70, 27}, {}, {499, 58, 18}, {420, 65, 26}}}, 1},
        {"stick.toggle12", {{{292, 475, 26}, {211, 430, 21}, {}, {495, 411, 20}, {}}}},
        {"stick.toggle34", {{{335, 475, 26}, {234, 439, 21}, {}, {527, 402, 20}, {}}}},
        {"stick.toggle56", {{{379, 475, 26}, {256, 448, 21}, {}, {556, 393, 20}, {}}}},
        {"throttle.main", {{{}, {}, {214, 226, 53}, {208, 314, 43}, {}}}, 2},
        {"throttle.rotary_top", {{{}, {}, {309, 45, 36}, {263, 220, 28}, {}}}, 2},
        {"throttle.rotary_side", {{{}, {}, {379, 150, 29}, {302, 281, 23}, {}}}, 2},
        {"throttle.slider", {{{}, {}, {322, 110, 25}, {270, 255, 21}, {}}}, 2},
        // D/E are the buttons in the rotary centres; I is above the slider.
        {"throttle.d", {{{}, {}, {307, 34, 18}, {263, 214, 16}, {}}}, 2},
        {"throttle.e", {{{}, {}, {390, 153, 17}, {309, 282, 15}, {}}}, 2},
        {"throttle.clutch", {{{}, {}, {355, 79, 19}, {289, 239, 17}, {}}}, 2},
        // The index-finger controls are hidden behind the handle in these photos.
        {"throttle.hat", {{{}, {}, {248, 93, 30}, {228, 247, 26}, {}}}, 2, true},
        {"throttle.mouse_x", {{{}, {}, {359, 182, 17}, {291, 299, 15}, {}}}, 2},
        {"throttle.mouse_y", {{{}, {}, {359, 182, 17}, {291, 299, 15}, {}}}, 2},
        {"throttle.mouse_button", {{{}, {}, {355, 205, 16}, {290, 314, 15}, {}}}, 2},
        {"throttle.scroll", {{{}, {}, {249, 133, 29}, {230, 269, 24}, {}}}, 2, true},
        {"throttle.mfd_function", {{{}, {}, {408, 440, 26}, {318, 448, 21}, {}}}, 2},
        {"throttle.mfd_start", {{{}, {}, {453, 429, 26}, {347, 442, 21}, {}}}, 2},
        {"throttle.mfd_reset", {{{}, {}, {498, 418, 26}, {375, 435, 21}, {}}}, 2},
    };
    for (const auto& annotation : annotations) if (annotation.id == id) return &annotation;
    return nullptr;
}
}
