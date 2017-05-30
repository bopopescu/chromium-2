// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_NTP_TILE_SOURCE_H_
#define COMPONENTS_NTP_TILES_NTP_TILE_SOURCE_H_

namespace ntp_tiles {

// The source of an NTP tile.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp
enum class NTPTileSource {
  // Tile comes from the personal top sites list, based on local history.
  TOP_SITES,
  // Tile comes from the suggestions service, based on synced history.
  SUGGESTIONS_SERVICE,
  // Tile is regionally popular.
  POPULAR,
  // Tile is on a custodian-managed whitelist.
  WHITELIST,

  LAST = WHITELIST
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_NTP_TILE_SOURCE_H_