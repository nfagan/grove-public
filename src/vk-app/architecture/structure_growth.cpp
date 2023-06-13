#include "structure_growth.hpp"
#include "common.hpp"
#include "grove/math/constants.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace arch;

void reset(FitBoundsToPointsContext* context) {
  clear(&context->alloc);
  context->num_fit = 0;
}

void set_target_length(FitBoundsToPointsContext* context, float length) {
  context->fit_params.target_length = length;
}

arch::FitLinesToPointsParams begin_line_fit(const Vec2f& p0, LinearAllocator* alloc,
                                            float target_length, int max_num_fit) {
  arch::FitLinesToPointsParams result{};
  result.target_length = target_length;
  result.max_num_fit = max_num_fit;
  result.p0_ind = 0;
  result.query_p = p0;
  result.f = 0.5f;
  result.last_theta = 0.0f;
  result.result_entries = alloc;
  return result;
}

OBB3f make_extruded_bounds(const Vec3f& size, const Vec3f& p0,
                           float dtheta, const OBB3f* parent_bounds) {
  OBB3f bounds;
  if (!parent_bounds) {
    bounds = arch::make_obb_xz(p0, dtheta, size);
    bounds.position += bounds.i * bounds.half_size.x;
  } else {
    bounds = arch::extrude_obb_xz(*parent_bounds, dtheta, size);
  }
  return bounds;
}

struct NewStructureSegmentResult {
  float dtheta;
  bool success;
};

struct NewStructureSegmentParams {
  Vec2f* line_v;
  Vec2f* line_p;
  const arch::TryEncirclePointParams* encircle_point_params;
  const Vec2f* line_target;
  std::vector<Vec2f>* encircle_ps;
  arch::FitLinesToPointsParams* fit_params;
  int* num_fit;
};

NewStructureSegmentParams make_new_segment_params(FitBoundsToPointsContext* context) {
  NewStructureSegmentParams result{};
  result.fit_params = &context->fit_params;
  result.encircle_point_params = &context->encircle_point_params;
  result.line_v = &context->line_v;
  result.line_p = &context->line_p;
  result.line_target = &context->line_target;
  result.encircle_ps = &context->line_ps;
  result.num_fit = &context->num_fit;
  return result;
}

NewStructureSegmentResult compute_next_structure_segment(const NewStructureSegmentParams& params) {
  int attempt{};
  int curr_num_fit{};
  const int tot_num_fit = *params.num_fit;

  while (tot_num_fit < params.fit_params->max_num_fit && attempt < 128) {
    std::vector<Vec2f>* encircle_ps = params.encircle_ps;
    if (params.fit_params->p0_ind + 1 < uint32_t(encircle_ps->size())) {
      curr_num_fit += arch::fit_line_to_points(
        encircle_ps->data(), uint32_t(encircle_ps->size()), params.fit_params);
      if (curr_num_fit > 0) {
        break;
      }
    }
    arch::try_encircle_point(
      *params.line_target, *params.encircle_point_params, params.line_p, params.line_v);
    encircle_ps->push_back(*params.line_p);
    attempt++;
  }

  NewStructureSegmentResult result{};
  if (curr_num_fit > 0) {
    result.success = true;
    *params.num_fit += curr_num_fit;

    arch::FitLineToPointsEntry entry{};
    read_ith(&entry, params.fit_params->result_entries->begin, tot_num_fit);
    result.dtheta = entry.dtheta;
  }
  return result;
}

float remap_theta_for_bounds_extrusion(float th) {
  const auto pi2 = float(two_pi());
  while (th >= pi2) {
    th -= pi2;
  }
  if (th > pif()) {
    th = -(pif() - (th - pif()));
  }
  return th;
}

} //  anon

void arch::try_encircle_point(const Vec2f& target, const TryEncirclePointParams& params,
                              Vec2f* p, Vec2f* v) {
  auto to_target = target - *p;
  auto d_to_target = to_target.length();
  if (d_to_target == 0.0f) {
    to_target = Vec2f{1.0f, 0.0f};
    d_to_target = 1.0f;
  }

  auto to_target_v = to_target / d_to_target;
  auto n_to_target_v = Vec2f{-to_target_v.y, to_target_v.x};

  const float d_attract_until = params.dist_attract_until;
  const float d_begin_propel = params.dist_begin_propel;

  float clamp_attrac_dist = std::max(d_to_target, d_attract_until) - d_attract_until;
  float attract = 1.0f - std::exp(params.attract_dist_falloff * -clamp_attrac_dist);

  float clamp_propel_dist = std::min(d_begin_propel, d_to_target);
  float propel = (d_begin_propel - clamp_propel_dist) / d_begin_propel;

  auto f_to_cent = to_target_v * attract;
  auto f_away_cent = n_to_target_v * propel;
  auto f = f_to_cent * params.attract_force_scale + f_away_cent * params.propel_force_scale;
  auto new_p = *p + *v * params.dt + f * (params.dt * params.dt);
  *v = *p - new_p;

  if (params.constant_speed) {
    auto lv = v->length();
    *v = *params.constant_speed * (lv == 0.0f ? Vec2f{1.0f, 0.0f} : *v / lv);
    new_p = *p - *v;
  }

  *p = new_p;
}

int arch::fit_line_to_points(const Vec2f* ps, uint32_t num_ps,
                             FitLinesToPointsParams* params) {
  int num_fit{};
  int max_num_fit = params->max_num_fit;
  while (params->p0_ind + 1 < num_ps && num_fit < max_num_fit) {
    uint32_t p0_ind = params->p0_ind;
    uint32_t p1_ind = p0_ind + 1;
    const auto& p0 = ps[p0_ind];
    const auto& p1 = ps[p1_ind];
    auto query_to_p1 = p1 - params->query_p;
    auto p0_to_p1 = p1 - p0;

    if (params->target_length >= query_to_p1.length()) {
      params->p0_ind++;
      params->f = 0.5f;
    } else {
      while (num_fit < max_num_fit) {
        Vec2f eval_p;
        float step = 1.0f - params->f;
        for (int i = 0; i < 22; i++) {
          eval_p = p0_to_p1 * params->f + p0;
          auto eval_dist = (eval_p - params->query_p).length();
          if (eval_dist < params->target_length) {
            params->f += step * 0.5f;
            step *= 0.5f;
          } else if (eval_dist > params->target_length) {
            params->f -= step * 0.5f;
            step *= 0.5f;
          } else {
            break;
          }
        }

        Vec2f dir = eval_p - params->query_p;
        float theta = atan2(dir.y, dir.x);

        FitLineToPointsEntry entry{};
        entry.dtheta = theta - params->last_theta;
        entry.p0 = params->query_p;
        push(params->result_entries, &entry, 1);
        num_fit++;

        params->last_theta = theta;
        params->query_p = eval_p;
        query_to_p1 = p1 - params->query_p;

        if (params->target_length >= query_to_p1.length() || params->f >= 1.0f) {
          params->p0_ind++;
          params->f = 0.5f;
          break;
        }
      }
    }
  }
  return num_fit;
}

void arch::initialize_fit_bounds_to_points_context(FitBoundsToPointsContext* context,
                                                   const Vec3f& p0,
                                                   const Vec2f& line_target,
                                                   const TryEncirclePointParams& encircle_point_params,
                                                   int max_num_entries) {
  *context = {};
  context->p0 = p0;
  context->line_target = line_target;

  const auto heap_size = sizeof(arch::FitLineToPointsEntry) * max_num_entries;
  context->heap_data = std::make_unique<unsigned char[]>(heap_size);
  context->alloc = make_linear_allocator(
    context->heap_data.get(), context->heap_data.get() + heap_size);

  const Vec2f p0_xz{p0.x, p0.z};
  context->fit_params = begin_line_fit(p0_xz, &context->alloc, 0.0f, max_num_entries);
  context->encircle_point_params = encircle_point_params;
  context->line_ps.push_back(p0_xz);
  context->line_p = p0_xz;
}

void arch::initialize_fit_bounds_to_points_context_default(FitBoundsToPointsContext* context,
                                                           const Vec3f& struct_ori,
                                                           const Vec2f& line_target) {
  arch::initialize_fit_bounds_to_points_context(
    context, struct_ori, line_target, arch::TryEncirclePointParams::make_default1(nullptr), 1);
}

void arch::set_line_target(FitBoundsToPointsContext* context, const Vec2f& line_target) {
  context->line_target = line_target;
}

Optional<OBB3f> arch::extrude_bounds(FitBoundsToPointsContext* context, const Vec3f& size,
                                     const OBB3f* parent_bounds) {
  set_target_length(context, size.x);
  reset(context);
  auto new_seg_params = make_new_segment_params(context);
  auto res = compute_next_structure_segment(new_seg_params);
  if (res.success) {
    const float dtheta = remap_theta_for_bounds_extrusion(res.dtheta);
    return Optional<OBB3f>(make_extruded_bounds(size, context->p0, dtheta, parent_bounds));
  } else {
    return NullOpt{};
  }
}

TryEncirclePointParams arch::TryEncirclePointParams::make_default1(const float* piece_length) {
  TryEncirclePointParams result{};
  result.dist_attract_until = 16.0f;
  result.dist_begin_propel = 64.0f;
  result.dt = 0.5f;
  result.constant_speed = piece_length;
  result.attract_force_scale = 10.0f;
  result.propel_force_scale = 100.0f;
  result.attract_dist_falloff = 0.05f;
  return result;
}

GROVE_NAMESPACE_END