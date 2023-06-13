#pragma once

#include "grove/math/vector.hpp"

namespace grove::foliage {

enum class OrnamentalFoliageWindType {
  Null,
  OnPlantStem,
  OnBranchAxis,
};

enum class OrnamentalFoliageGeometryType {
  Null,
  CurvedPlane,
  FlatPlane,
};

enum class OrnamentalFoliageMaterialType {
  Null,
  Material1,
  Material2
};

struct OrnamentalFoliageWindDataDescriptor {
  struct OnPlantStem {
    float tip_y_fraction;
    Vec2f world_origin_xz;
  };

  struct OnBranchAxis {
    Vec4<uint32_t> info0;
    Vec4<uint32_t> info1;
    Vec4<uint32_t> info2;
  };

  union {
    OnPlantStem on_plant_stem;
    OnBranchAxis on_branch_axis;
  };
};

struct OrnamentalFoliageMaterial1Descriptor {
  uint32_t texture_layer_index;
  Vec3<uint8_t> color0;
  Vec3<uint8_t> color1;
  Vec3<uint8_t> color2;
  Vec3<uint8_t> color3;
};

struct OrnamentalFoliageMaterial2Descriptor {
  uint32_t texture_layer_index;
  Vec3<uint8_t> color0;
  Vec3<uint8_t> color1;
  Vec3<uint8_t> color2;
  Vec3<uint8_t> color3;
};

struct CurvedPlaneGeometryDescriptor {
  float min_radius;
  float radius;
  float radius_power;
  float curl_scale;
};

struct FlatPlaneGeometryDescriptor {
  float aspect;
  float scale;
  float y_rotation_theta;
};

struct OrnamentalFoliageMaterialDescriptor {
  union {
    OrnamentalFoliageMaterial1Descriptor material1;
    OrnamentalFoliageMaterial2Descriptor material2;
  };
};

struct OrnamentalFoliageGeometryDescriptor {
  union {
    CurvedPlaneGeometryDescriptor curved_plane;
    FlatPlaneGeometryDescriptor flat_plane;
  };
};

struct OrnamentalFoliageInstanceGroupDescriptor {
  OrnamentalFoliageGeometryType geometry_type;
  OrnamentalFoliageMaterialType material_type;
  OrnamentalFoliageWindType wind_type;
  Vec3f aggregate_aabb_p0;
  Vec3f aggregate_aabb_p1;
};

struct OrnamentalFoliageInstanceDescriptor {
  Vec3f translation;
  Vec3f orientation;
  OrnamentalFoliageMaterialDescriptor material;
  OrnamentalFoliageGeometryDescriptor geometry_descriptor;
  OrnamentalFoliageWindDataDescriptor wind_data;
};

}