#pragma once

#include "SkyGradient.hpp"

namespace grove::weather {

SkyGradient::Params sunny_to_overcast_gradient_params(float frac_cloudy);
SkyGradient::Params overcast_to_sunny_gradient_params(float frac_cloudy);

Vec3f sunny_to_overcast_sun_color(float frac_sunny);
Vec3f overcast_to_sunny_sun_color(float frac_sunny);

}