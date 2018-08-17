#pragma once

#include "ABC_Model.hpp"
#include "ABC_H0.hpp"

namespace Models
{

class SIAM_Square : public ABC_Model_2D<IO::IOSIAM, ABC_H0<Nx1, Nx1>>
{
  using IOModel_t = IO::IOSIAM;
  using H0_t = ABC_H0<Nx1, Nx1>;

public:
  SIAM_Square(const Json &jj) : ABC_Model_2D<IOModel_t, H0_t>(jj){};
};
} // namespace Models