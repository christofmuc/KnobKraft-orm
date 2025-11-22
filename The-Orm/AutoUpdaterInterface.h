/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

class AutoUpdaterInterface
{
public:
  virtual ~AutoUpdaterInterface() = default;
  virtual void checkForUpdates() = 0;
};
