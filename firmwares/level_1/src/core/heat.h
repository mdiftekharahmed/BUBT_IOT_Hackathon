#pragma once

#include <math.h>

namespace fastrobox {

inline float heatIndexC(float tempC, float relativeHumidity) {
  const float tempF = tempC * 9.0f / 5.0f + 32.0f;
  if (tempF < 80.0f) return tempC;

  const float rh = relativeHumidity;
  float heatIndexF =
      -42.379f + 2.04901523f * tempF + 10.14333127f * rh -
      0.22475541f * tempF * rh - 0.00683783f * tempF * tempF -
      0.05481717f * rh * rh + 0.00122874f * tempF * tempF * rh +
      0.00085282f * tempF * rh * rh -
      0.00000199f * tempF * tempF * rh * rh;

  if (rh < 13.0f && tempF >= 80.0f && tempF <= 112.0f) {
    const float adjustment =
        ((13.0f - rh) / 4.0f) *
        sqrtf((17.0f - fabsf(tempF - 95.0f)) / 17.0f);
    heatIndexF -= adjustment;
  } else if (rh > 85.0f && tempF >= 80.0f && tempF <= 87.0f) {
    const float adjustment = ((rh - 85.0f) / 10.0f) * ((87.0f - tempF) / 5.0f);
    heatIndexF += adjustment;
  }
  return (heatIndexF - 32.0f) * 5.0f / 9.0f;
}

}  // namespace fastrobox
