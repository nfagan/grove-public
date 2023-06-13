#pragma once

namespace grove {

void init_fdft();
void fdft(float* out, const float* in, int n);
void fdft(double* out, const double* in, int n);

}