/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MacSparkle.h"

#import <Foundation/Foundation.h>
#import <Sparkle/Sparkle.h>

// This does not follow the new procedure at https://sparkle-project.org/documentation/programmatic-setup/ yet

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

class SparkleAutoUpdate::Impl
{
public:
  SUUpdater *updater;
};

SparkleAutoUpdate::SparkleAutoUpdate()
{
  d = new SparkleAutoUpdate::Impl;
  d->updater = [[SUUpdater sharedUpdater] retain];
  [d->updater setAutomaticallyChecksForUpdates: YES];
  [d->updater setUpdateCheckInterval: 3600];
}

SparkleAutoUpdate::~SparkleAutoUpdate()
{
  [d->updater release];
  delete d;
}

void SparkleAutoUpdate::checkForUpdates()
{
  [d->updater checkForUpdates : nil];
}

#pragma clang diagnostic pop
