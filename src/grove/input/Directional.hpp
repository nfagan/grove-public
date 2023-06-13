#pragma once

#include <atomic>

namespace grove {
  namespace input {
    class Directional;
  }
}

class grove::input::Directional {
public:
  Directional();
  
  void set_x(double amount);
  void set_z(double amount);
  
  void add_x(double amount);
  void add_z(double amount);
  
  void clear();
  
  double get_x() const;
  double get_z() const;
  
private:
  std::atomic<double> x;
  std::atomic<double> z;
};
