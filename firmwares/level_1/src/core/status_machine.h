#pragma once

#include <cstddef>
#include <cstdint>
#include <math.h>

namespace fastrobox {

enum class HeatStatus : uint8_t { Safe, Warning, Danger };

struct Readings {
  float tempC;
  float rh;
  bool qualityGood;
};

class StatusMachine {
 public:
  HeatStatus update(const Readings& readings, uint32_t nowMs) {
    if (!readings.qualityGood || !isfinite(readings.tempC) || !isfinite(readings.rh) ||
        readings.tempC < 0.0f ||
        readings.tempC > 50.0f || readings.rh < 20.0f ||
        readings.rh > 90.0f) {
      candidateActive_ = false;
      return status_;
    }

    HeatStatus target = status_;
    switch (status_) {
      case HeatStatus::Safe:
        if (readings.tempC >= 40.0f) {
          target = HeatStatus::Danger;
        } else if (readings.tempC >= 32.0f || readings.rh >= 85.0f) {
          target = HeatStatus::Warning;
        }
        break;
      case HeatStatus::Warning:
        if (readings.tempC >= 40.0f) {
          target = HeatStatus::Danger;
        } else if (readings.tempC <= 30.0f && readings.rh <= 80.0f) {
          target = HeatStatus::Safe;
        }
        break;
      case HeatStatus::Danger:
        if (readings.tempC <= 38.0f) target = HeatStatus::Warning;
        break;
    }

    if (target == status_) {
      candidateActive_ = false;
      return status_;
    }
    if (!candidateActive_ || candidate_ != target) {
      candidate_ = target;
      candidateSinceMs_ = nowMs;
      candidateActive_ = true;
      return status_;
    }
    if (static_cast<uint32_t>(nowMs - candidateSinceMs_) >= 3000U) {
      status_ = target;
      candidateActive_ = false;
    }
    return status_;
  }

  HeatStatus status() const { return status_; }

 private:
  HeatStatus status_ = HeatStatus::Safe;
  HeatStatus candidate_ = HeatStatus::Safe;
  uint32_t candidateSinceMs_ = 0;
  bool candidateActive_ = false;
};

inline const char* statusName(HeatStatus status) {
  switch (status) {
    case HeatStatus::Warning: return "WARNING";
    case HeatStatus::Danger: return "DANGER";
    case HeatStatus::Safe:
    default: return "SAFE";
  }
}

enum class Trend : uint8_t { Rising, Stable, Falling };

class TrendWindow {
 public:
  static constexpr size_t kCapacity = 10;

  void add(float value) {
    values_[next_] = value;
    next_ = (next_ + 1) % kCapacity;
    if (count_ < kCapacity) ++count_;
  }

  Trend trend() const {
    if (count_ < 2) return Trend::Stable;
    const size_t oldest = count_ == kCapacity ? next_ : 0;
    const float change = values_[(oldest + count_ - 1) % kCapacity] - values_[oldest];
    if (change > 0.3f) return Trend::Rising;
    if (change < -0.3f) return Trend::Falling;
    return Trend::Stable;
  }

  size_t size() const { return count_; }

 private:
  float values_[kCapacity] = {};
  size_t count_ = 0;
  size_t next_ = 0;
};

inline const char* trendName(Trend trend) {
  switch (trend) {
    case Trend::Rising: return "RISING";
    case Trend::Falling: return "FALLING";
    case Trend::Stable:
    default: return "STABLE";
  }
}

}  // namespace fastrobox
