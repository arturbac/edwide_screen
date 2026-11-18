// edwide_screen: where a side monitor's pixel takes its colour from in the game's flat picture - the arithmetic of
// shaders/wings.hlsl on the CPU (the plugin's constants, the test's expectations); keep the two the same.
#pragma once

#include <cmath>
#include <optional>

namespace edwide
  {
  ///\brief the layout in pixels of the picture and the game's projection
  struct wings_layout_t
    {
    float width{}, height{};
    float half_middle{};  ///< half the middle monitor's width
    float bezel{};        ///< the pixels hidden behind the bezels between the middle and a side
    float plane{};        ///< the game's picture plane's distance from the eye, pixels: (height / 2) / tan(vertical fov / 2)
    float angle_deg{};    ///< the sides turned towards the eye about their hinge with the middle
    float curve{};        ///< 0 flat monitors .. 1 a cylinder about the eye
    float zoom{1.f};
    float offset{};       ///< vertical, pixels
    };

  struct wings_point_t
    {
    float x, y;
    };

  ///\brief the picture plane's distance for a vertical field of view (degrees) and the picture's height
  [[nodiscard]]
  inline auto wings_plane(float vertical_fov_deg, float height) noexcept -> float
    {
    return 0.5f * height / std::tan(0.5f * vertical_fov_deg * 0.017453293f);
    }

  ///\brief where the output pixel (its centre, x, y from the top left) takes its colour from in the game's picture;
  /// none behind a bezel, past the eye's side or outside the picture. The middle monitor's part is itself
  [[nodiscard]]
  inline auto wings_source(wings_layout_t const & l, float x, float y) noexcept -> std::optional<wings_point_t>
    {
    float const u{x - 0.5f * l.width};
    float const v{y - 0.5f * l.height};
    float const side{u < 0.f ? -1.f : 1.f};
    float const along{std::fabs(u) - l.half_middle};
    if(along < 0.f)
      return wings_point_t{x, y};
    if(along < l.bezel)
      return std::nullopt;
    float const a{l.angle_deg * 0.017453293f};
    float const fx{l.half_middle + along * std::cos(a)}, fz{l.plane - along * std::sin(a)};
    float const radius{std::sqrt(l.half_middle * l.half_middle + l.plane * l.plane)};
    float const turn{std::atan2(l.half_middle, l.plane) + along / radius};
    float const rx{radius * std::sin(turn)}, rz{radius * std::cos(turn)};
    float const px{fx + (rx - fx) * l.curve}, pz{fz + (rz - fz) * l.curve};
    if(pz <= 1.f)
      return std::nullopt;
    float const sx{l.half_middle + (px * l.plane / pz - l.half_middle) / l.zoom};
    float const sy{(v * l.plane / pz) / l.zoom + l.offset};
    wings_point_t const at{0.5f * l.width + side * sx, 0.5f * l.height + sy};
    if(at.x < 0.f or at.y < 0.f or at.x >= l.width or at.y >= l.height)
      return std::nullopt;
    return at;
    }
  }  // namespace edwide
