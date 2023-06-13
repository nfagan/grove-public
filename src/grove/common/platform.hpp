#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  #define GROVE_WIN
#elif __APPLE__
  #define GROVE_MACOS
  #define GROVE_UNIX
#elif __linux__
  #define MT_LINUX
  #define GROVE_UNIX
#elif __unix__
  #define GROVE_LINUX
  #define GROVE_UNIX
#endif

#ifdef _MSC_VER
#define GROVE_IS_MSVC (1)
#else
#define GROVE_IS_MSVC (0)
#endif