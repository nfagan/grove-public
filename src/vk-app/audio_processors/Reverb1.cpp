#include "Reverb1.hpp"
#include "grove/common/common.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/matrix.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

template <typename T>
void configure_fdn_filters(T&& filters) {
  const double b0s[3] = {0.025176114554401, 0.050352229108803, 0.025176114554401};
  const double a0s[3] = {1.0, -1.503695341299222, 0.604399799516827};

  const double b1s[3] = {0.057200372524856, 0.114400745049711, 0.057200372524856};
  const double a1s[3] = {1.0, -1.218879336445587, 0.447680826545010};

  const double b2s[3] = {0.083159869929952, 0.166319739859905, 0.083159869929952};
  const double a2s[3] = {1.0, -1.035171209738942, 0.367810689458751};

  const double b3s[3] = {0.112055205606069, 0.224110411212137, 0.112055205606069};
  const double a3s[3] = {1.0, -0.855989502672595, 0.304210325096870};

  const double* bs[4] = {b0s, b1s, b2s, b3s};
  const double* as[4] = {a0s, a1s, a2s, a3s};

  for (int i = 0; i < filters.size(); i++) {
    const auto coeff_ind = i % 4;
    filters[i].set_b(bs[coeff_ind], 3);
    filters[i].set_a(as[coeff_ind], 3);
  }
}

} //  anon

Reverb1::Reverb1() {
  const double lb[11] = {
    0.003905006730547, 0.0, -0.019525033652734, 0.0, 0.039050067305468,
    0.0, -0.039050067305468, 0.0, 0.019525033652734, 0.0, -0.003905006730547};

  const double la[11] = {
    1.0, -7.248243592455253, 23.751570524604634, -46.504229312238074,
    60.431958590925959, -54.567530718966829, 34.696940461494691, -15.337949025420988,
    4.509781050300187, -0.796340692543437, 0.064042723049371,
  };

  lp0.set_b(lb, 11);
  lp0.set_a(la, 11);
  lp1.set_b(lb, 11);
  lp1.set_a(la, 11);

  const int fdn_delays[4] = {1331, 2197, 4913, 6859};
  for (const auto& del : fdn_delays) {
    fdn_filters0.emplace_back();
    fdn_filters1.emplace_back();
    fdn_delays0.emplace_back(del);
    fdn_delays1.emplace_back(del + 33);
  }

  configure_fdn_filters(fdn_filters0);
  configure_fdn_filters(fdn_filters1);
}

GROVE_NAMESPACE_END