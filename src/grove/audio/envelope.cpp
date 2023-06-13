#include "envelope.hpp"
#include "grove/math/util.hpp"

namespace grove {

/*
 * Params
 */

Envelope::Params Envelope::Params::default_exp(bool inf_sustain) {
  Params params{};
  params.attack_time = 0.05;
  params.decay_time = 0.5;
  params.sustain_time = 0.0;
  params.sustain_amp = 1.0;
  params.release_time = 0.5;
  params.infinite_sustain = inf_sustain;
  return params;
}

/*
 * EnvelopeEpoch
 */

double EnvelopeEpoch::Min::evaluate(double current_gain, double target_gain) {
  return std::min(current_gain, target_gain);
}

double EnvelopeEpoch::Max::evaluate(double current_gain, double target_gain) {
  return std::max(current_gain, target_gain);
}

template <typename Bounder>
double EnvelopeEpoch::tick(env::ADSR& envelope,
                           int num_frames,
                           Envelope::Epoch target_epoch,
                           double target_gain) {
  const auto curr_frame = envelope.current_frame;
  envelope.current_frame++;

  if (curr_frame >= num_frames) {
    return envelope.transition(target_epoch, target_gain);

  } else {
    const double delta = target_gain - envelope.current_gain;
    const auto remaining_frames = double(num_frames - curr_frame);
    const double incr = delta / remaining_frames;
    envelope.current_gain += incr;
    envelope.current_gain = Bounder::evaluate(envelope.current_gain, target_gain);

    return db_to_amplitude(envelope.current_gain);
  }
}

/*
 * ADSRExp
 */

env::ADSR::ADSR() : ADSR(default_sample_rate(), Params{}) {
  //
}

env::ADSR::ADSR(double sample_rate, const Params& params) :
  sample_rate(sample_rate),
  current_frame(0),
  epoch(Epoch::Elapsed),
  current_gain(minimum_finite_gain()),
  initial_gain(minimum_finite_gain()),
  attack_time(params.attack_time),
  peak_gain(amplitude_to_db(params.peak_amp)),
  decay_time(params.decay_time),
  sustain_gain(amplitude_to_db(params.sustain_amp)),
  sustain_time(params.infinite_sustain ? -1.0 : params.sustain_time),
  release_time(params.release_time) {
  //
}

void env::ADSR::set_sample_rate(double to) {
  sample_rate = to;
}

void env::ADSR::note_on() {
  current_frame = 0;
  epoch = Epoch::Attack;
}

void env::ADSR::note_off() {
  current_frame = 0;
  epoch = Epoch::Release;
}

void env::ADSR::reset() {
  current_frame = 0;
  epoch = Epoch::Elapsed;
  current_gain = initial_gain;
}

double env::ADSR::transition(Epoch to, double set_current_gain) {
  epoch = to;
  current_gain = set_current_gain;
  current_frame = 0;
  return tick();
}

double env::ADSR::tick() {
  switch (epoch) {
    case Epoch::Elapsed:
      return 0.0;
    case Epoch::Attack:
      return attack();
    case Epoch::Decay:
      return decay();
    case Epoch::Sustain:
      return sustain();
    case Epoch::Release:
      return release();
    default:
      return 0.0;
  }
}

double env::ADSR::attack() {
  using Min = EnvelopeEpoch::Min;
  return EnvelopeEpoch::tick<Min>(*this, attack_frames(), Epoch::Decay, peak_gain);
}

double env::ADSR::decay() {
  using Max = EnvelopeEpoch::Max;
  return EnvelopeEpoch::tick<Max>(*this, decay_frames(), Epoch::Sustain, sustain_gain);
}

double env::ADSR::sustain() {
  using Max = EnvelopeEpoch::Max;

  if (sustain_time < 0.0) {
    current_frame++;
    return db_to_amplitude(sustain_gain);

  } else {
    return EnvelopeEpoch::tick<Max>(*this, sustain_frames(), Epoch::Release, sustain_gain);
  }
}

double env::ADSR::release() {
  using Max = EnvelopeEpoch::Max;
  auto target_gain = initial_gain;

  if (target_gain == -grove::infinity()) {
    target_gain = minimum_finite_gain();
  }

  return EnvelopeEpoch::tick<Max>(*this, release_frames(), Epoch::Elapsed, target_gain);
}

int env::ADSR::attack_frames() const {
  return int(attack_time * sample_rate);
}

int env::ADSR::decay_frames() const {
  return int(decay_time * sample_rate);
}

int env::ADSR::release_frames() const {
  return int(release_time * sample_rate);
}

int env::ADSR::sustain_frames() const {
  return int(sustain_time * sample_rate);
}

bool env::ADSR::elapsed() const {
  return epoch == Epoch::Elapsed;
}

double env::ADSR::get_sample_rate() const {
  return sample_rate;
}

void env::ADSR::default_configure1(ADSR& envelope) {
  envelope.attack_time = 0.005;
  envelope.peak_gain = amplitude_to_db(1.0);
  envelope.decay_time = 0.2;
  envelope.sustain_gain = amplitude_to_db(0.5);
  envelope.sustain_time = -1.0;
  envelope.release_time = 0.5;
}

void env::ADSR::configure(const Params& params) {
  attack_time = params.attack_time;
  peak_gain = amplitude_to_db(params.peak_amp);

  decay_time = params.decay_time;
  sustain_gain = amplitude_to_db(params.sustain_amp);
  sustain_time = params.infinite_sustain ? -1 : params.sustain_time;
  release_time = params.release_time;
}

}
