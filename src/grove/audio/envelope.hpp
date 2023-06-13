#pragma once

#include "types.hpp"

namespace grove {

namespace audio {

template <typename Float>
struct ExpInterpolated {
  ExpInterpolated() = default;
  explicit ExpInterpolated(Float target) : target{target} {
    //
  }

  Float tick(Float sample_rate) {
    auto t = std::exp(-Float(1) / sample_rate / time_constant);
    current = t * current + (Float(1) - t) * target;
    return current;
  }

  void set_time_constant95(Float seconds_to_95) {
    time_constant = seconds_to_95 / Float(3.0);
  }

  void set_target(Float t) {
    target = t;
  }

  void reach_target_if(Float eps) {
    if (std::abs(target - current) < eps) {
      current = target;
    }
  }

  bool reached_target() const {
    return current == target;
  }

  Float target{};
  Float current{};
  Float time_constant{Float(1)};
};

} //  audio

class Envelope {
  friend struct EnvelopeEpoch;
protected:
  enum class Epoch : uint8_t {
    Attack,
    Decay,
    Sustain,
    Release,
    Elapsed
  };

public:
  struct Params {
    static Params default_exp(bool inf_sustain = true);

    double attack_time = 0.005;
    double decay_time = 0.05;
    double sustain_time = 0.05;
    double release_time = 0.025;
    double peak_amp = 1.0;
    double sustain_amp = 0.25;
    bool infinite_sustain = false;
  };
};

namespace env {
  class ADSR;
}

struct EnvelopeEpoch {
  struct Min {
    static double evaluate(double current_gain, double target_gain);
  };
  struct Max {
    static double evaluate(double current_gain, double target_gain);
  };

  template <typename Bounder>
  static double tick(env::ADSR& envelope, int num_frames,
                     Envelope::Epoch target_epoch, double target_gain);
};

namespace env {

/*
 * ADSR
 */

class ADSR : public Envelope {
  template <typename Bounder>
  friend double EnvelopeEpoch::tick(env::ADSR& envelope,
                                    int num_frames,
                                    Envelope::Epoch target_epoch,
                                    double target_gain);
public:
  ADSR();
  ADSR(double sample_rate, const Params& params);

  void set_sample_rate(double to);

  void note_on();
  void note_off();
  void reset();

  double tick();
  bool elapsed() const;
  double get_sample_rate() const;

  double get_current_amplitude() const {
    return db_to_amplitude(current_gain);
  }
  double get_current_gain() const {
    return current_gain;
  }

  void configure(const Params& params);

  static void default_configure1(ADSR& envelope);

private:
  double attack();
  double decay();
  double sustain();
  double release();

  double transition(Epoch to, double set_current_gain);

  int attack_frames() const;
  int decay_frames() const;
  int release_frames() const;
  int sustain_frames() const;

private:
  double sample_rate;

  int current_frame;
  Epoch epoch;
  double current_gain;

private:
  double initial_gain;
  double attack_time;
  double peak_gain;
  double decay_time;
  double sustain_gain;
  double sustain_time;
  double release_time;
};

/*
 * ADSRExp
 */

template <typename Float>
class ADSRExp : public Envelope {
  struct Segment {
    Float tick(double sr);

    Float last{};
    Float target;
    Float tau;
    Float duration;
  };

public:
  Float tick(double sr);

  void configure(const Params& params);

  void set_attack_time(Float t) {
    attack_time = t;
  }
  void set_decay_time(Float t) {
    decay_time = t;
  }
  void set_sustain_time(Float t) {
    sustain_time = t;
  }
  void set_release_time(Float t) {
    release_time = t;
  }

  void note_on();
  void note_off();

  bool elapsed() const {
    return epoch == Epoch::Elapsed;
  }

  Float get_current_amplitude() const {
    return current_segment.last;
  }

private:
  static Float time_to_tau(Float t) {
    return t / Float(3.0);
  }

private:
  Float epoch_elapsed_time{Float(0.0)};
  Float attack_time{Float(1.0)};
  Float decay_time{Float(1.0)};
  Float sustain_time{Float(1.0)};
  Float release_time{Float(1.0)};

  Float initial_amp{Float(0.0)};
  Float peak_amp{Float(1.0)};
  Float sustain_amp{Float(0.5)};

  Segment current_segment{};
  Epoch epoch{Epoch::Elapsed};
};

/*
 * Impl
 */

template <typename Float>
Float ADSRExp<Float>::Segment::tick(double sr) {
  auto t = 1.0 / sr;
  auto a = std::exp(-t / tau);
  auto v = Float(a * last + (1.0 - a) * target);
  last = v;
  return v;
}

template <typename Float>
void ADSRExp<Float>::configure(const Params& params) {
  const auto inf = std::numeric_limits<Float>::infinity();

  attack_time = Float(params.attack_time);
  decay_time = Float(params.decay_time);
  release_time = Float(params.release_time);
  sustain_time = params.infinite_sustain ? inf : Float(params.sustain_time);

  peak_amp = Float(params.peak_amp);
  sustain_amp = Float(params.sustain_amp);
}

template <typename Float>
void ADSRExp<Float>::note_on() {
  epoch = Epoch::Attack;

  current_segment.target = peak_amp;
  current_segment.tau = time_to_tau(attack_time);
  current_segment.duration = attack_time;

  epoch_elapsed_time = Float(0);
}

template <typename Float>
void ADSRExp<Float>::note_off() {
  if (epoch == Epoch::Elapsed) {
    return;
  }

  epoch = Epoch::Release;
  current_segment.target = initial_amp;
  current_segment.tau = time_to_tau(release_time);
  current_segment.duration = release_time;

  epoch_elapsed_time = Float(0);
}

template <typename Float>
Float ADSRExp<Float>::tick(double sr) {
  if (epoch == Epoch::Elapsed) {
    return Float(0.0);
  }

  auto res =
    epoch == Epoch::Sustain ? current_segment.last : current_segment.tick(sr);

  epoch_elapsed_time += Float(1.0 / sr);

  if (epoch_elapsed_time >= current_segment.duration) {
    //  transition
    epoch_elapsed_time = Float(0.0);

    switch (epoch) {
      case Epoch::Attack: {
        current_segment.target = sustain_amp;
        current_segment.tau = time_to_tau(decay_time);
        current_segment.duration = decay_time;
        epoch = Epoch::Decay;
        break;
      }
      case Epoch::Decay: {
        epoch = Epoch::Sustain;
        current_segment.duration = sustain_time;
        break;
      }
      case Epoch::Sustain: {
        current_segment.target = initial_amp;
        current_segment.tau = time_to_tau(release_time);
        current_segment.duration = release_time;
        epoch = Epoch::Release;
        break;
      }
      case Epoch::Release: {
        epoch = Epoch::Elapsed;
        break;
      }
      default:
        break;
    }
  }

  return res;
}

/*
 * ADLin
 */

template <typename Float>
class ADLin : public Envelope {
public:
  Float tick(double sr);

  void configure(const Params& params);

  void note_on();
  void note_off();

  bool elapsed() const {
    return epoch == Epoch::Elapsed;
  }

  Float get_current_amplitude() const {
    return current;
  }

private:
  Float attack_time{Float(1.0)};
  Float decay_time{Float(1.0)};

  Float initial_amp{Float(0.0)};
  Float peak_amp{Float(1.0)};

  Float current{};
  Float target{};
  Float incr{};

  Epoch epoch{Epoch::Elapsed};
};

/*
 * impl
 */

template <typename Float>
void ADLin<Float>::configure(const Params& params) {
  attack_time = std::max(0.001, params.attack_time);
  decay_time = std::max(0.001, params.decay_time);
  //  ensure peak amp is not less than initial amp
  peak_amp = std::max(Float(initial_amp), Float(params.peak_amp));
}

template <typename Float>
void ADLin<Float>::note_on() {
  epoch = Epoch::Attack;
  target = peak_amp;
  incr = (peak_amp - initial_amp) / attack_time;
}

template <typename Float>
void ADLin<Float>::note_off() {
  if (epoch == Epoch::Elapsed) {
    return;
  }

  epoch = Epoch::Decay;
  target = initial_amp;
  incr = (initial_amp - peak_amp) / decay_time;
}

template <typename Float>
Float ADLin<Float>::tick(double sr) {
  if (epoch == Epoch::Elapsed) {
    return Float(0.0);
  }

  auto v = current;
  current += incr / sr;

  if (epoch == Epoch::Attack && current >= peak_amp) {
    current = peak_amp;
    target = initial_amp;
    incr = (initial_amp - peak_amp) / decay_time;
    epoch = Epoch::Decay;

  } else if (epoch == Epoch::Decay && current <= initial_amp) {
    current = initial_amp;
    epoch = Epoch::Elapsed;
  }

  return v;
}

/*
 * ADSRLin
 */

template <typename Float>
class ADSRLin : public Envelope {
public:
  Float tick(Float sr);

  void configure(const Params& params);

  void note_on();
  void note_off();

  bool elapsed() const {
    return epoch == Epoch::Elapsed;
  }

  Float get_current_amplitude() const {
    return current;
  }

private:
  Float attack_time{Float(1.0)};
  Float decay_time{Float(1.0)};
  Float sustain_time{Float(1.0)};
  Float release_time{Float(1.0)};

  Float initial_amp{Float(0.0)};
  Float peak_amp{Float(1.0)};
  Float sustain_amp{Float(1.0)};

  Float current{};
  Float target{};
  Float incr{};

  Epoch epoch{Epoch::Elapsed};
  Float epoch_elapsed_time{Float(0)};
};

/*
* impl
*/

template <typename Float>
void ADSRLin<Float>::configure(const Params& params) {
  attack_time = Float(std::max(0.001, params.attack_time));
  decay_time = Float(std::max(0.001, params.decay_time));
  release_time = Float(std::max(0.001, params.release_time));

  if (params.infinite_sustain) {
    sustain_time = Float(-1.0);
  } else {
    sustain_time = Float(params.sustain_time);
  }

  //  ensure peak amp is not less than initial amp
  peak_amp = std::max(Float(initial_amp), Float(params.peak_amp));
  //  ensure sustain amp is not greater than peak amp.
  sustain_amp = std::min(peak_amp, Float(params.sustain_amp));
}

template <typename Float>
void ADSRLin<Float>::note_on() {
  epoch = Epoch::Attack;
  target = peak_amp;
  incr = (peak_amp - initial_amp) / attack_time;
  epoch_elapsed_time = Float(0);
}

template <typename Float>
void ADSRLin<Float>::note_off() {
  if (epoch == Epoch::Elapsed) {
    return;
  }

  epoch = Epoch::Release;
  target = Float(0);
  incr = (target - sustain_amp) / release_time;
  epoch_elapsed_time = Float(0);
}

template <typename Float>
Float ADSRLin<Float>::tick(Float sr) {
  if (epoch == Epoch::Elapsed) {
    return Float(0);
  }

  auto v = current;
  current += incr / sr;
  epoch_elapsed_time += Float(1) / sr;

  if (epoch == Epoch::Attack && current >= peak_amp) {
    epoch_elapsed_time = Float(0);
    current = peak_amp;
    target = sustain_amp;
    incr = (sustain_amp - current) / decay_time;
    epoch = Epoch::Decay;

  } else if (epoch == Epoch::Decay && current <= sustain_amp) {
    epoch_elapsed_time = Float(0);
    current = sustain_amp;

    if (sustain_time == Float(0)) {
      //  Directly to release.
      target = Float(0);
      incr = (Float(0) - current) / release_time;
      epoch = Epoch::Release;
    } else {
      incr = Float(0);
      epoch = Epoch::Sustain;
    }

  } else if (epoch == Epoch::Sustain) {
    if (sustain_time >= Float(0) && epoch_elapsed_time >= sustain_time) {
      epoch_elapsed_time = Float(0);
      target = Float(0);
      incr = (Float(0) - sustain_amp) / release_time;
      epoch = Epoch::Release;
    }

  } else if (epoch == Epoch::Release && current <= Float(0)) {
    current = Float(0);
    epoch_elapsed_time = Float(0);
    incr = Float(0);
    epoch = Epoch::Elapsed;
  }

  return v;
}

}

}