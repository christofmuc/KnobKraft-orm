/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "AutoUpdaterInterface.h"

class SparkleAutoUpdate : public AutoUpdaterInterface
{
public:
  SparkleAutoUpdate ();
  virtual ~SparkleAutoUpdate ();
  virtual void checkForUpdates();

private:
  class Impl;
  Impl *d;
};
