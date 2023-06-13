#pragma once

namespace grove::ls {

struct DeriveResult;
struct DeriveContext;
struct DerivingString;

DeriveResult derive_branched(DeriveContext* ctx, const DerivingString* str);

}