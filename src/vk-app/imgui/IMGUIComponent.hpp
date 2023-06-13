#pragma once

#include "ProceduralTreeGUI.hpp"
#include "ProceduralTreeRootsGUI.hpp"
#include "ProceduralFlowerGUI.hpp"
#include "ProfileComponentGUI.hpp"
#include "GraphicsGUI.hpp"
#include "WeatherGUI.hpp"
#include "EditorGUI.hpp"
#include "InputGUI.hpp"
#include "SoilGUI.hpp"
#include "FogGUI.hpp"
#include "AudioGUI.hpp"
#include "ArchGUI.hpp"
#include "SystemsGUI.hpp"
#include "SkyGUI.hpp"
#include "TerrainGUI.hpp"
#include "SeasonGUI.hpp"
#include "ParticleGUI.hpp"

namespace grove {

struct IMGUIComponent {
  void render();

  bool enabled{};
  ProceduralTreeGUI procedural_tree_gui;
  bool procedural_tree_gui_enabled{};

  ProceduralTreeRootsGUI procedural_tree_roots_gui;
  bool procedural_tree_roots_gui_enabled{};

  ProceduralFlowerGUI procedural_flower_gui;
  bool procedural_flower_gui_enabled{};

  ProfileComponentGUI profile_component_gui;
  bool profile_component_gui_enabled{};

  GraphicsGUI graphics_gui;
  bool graphics_gui_enabled{};

  AudioGUI audio_gui;
  bool audio_gui_enabled{};

  WeatherGUI weather_gui;
  bool weather_gui_enabled{};

  editor::EditorGUI editor_gui;
  bool editor_gui_enabled{};

  InputGUI input_gui;
  bool input_gui_enabled{};

  SoilGUI soil_gui;
  bool soil_gui_enabled{};

  FogGUI fog_gui;
  bool fog_gui_enabled{};

  ArchGUI arch_gui;
  bool arch_gui_enabled{};

  SystemsGUI systems_gui;
  bool systems_gui_enabled{};

  SkyGUI sky_gui;
  bool sky_gui_enabled{};

  TerrainGUI terrain_gui;
  bool terrain_gui_enabled{};

  SeasonGUI season_gui;
  bool season_gui_enabled{};

  ParticleGUI particle_gui;
  bool particle_gui_enabled{};
};

}