// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// Collection view item to represent and configure an MDCCollectionViewTextCell.
@interface CollectionViewTextItem : CollectionViewItem

// The accessory type for the represented cell.
@property(nonatomic) MDCCollectionViewCellAccessoryType accessoryType;

// The main text to display.
@property(nonatomic, nullable, copy) NSString* text;

// The secondary text to display.
@property(nonatomic, nullable, copy) NSString* detailText;

// The image to show.
@property(nonatomic, nullable, strong) UIImage* image;

// The font of the main text. The Default value is the medium Roboto font of
// size 14.
@property(nonatomic, null_resettable, copy) UIFont* textFont;

// The color of the main text. The Default value is the 900 tint color of the
// grey palette.
@property(nonatomic, null_resettable, copy) UIColor* textColor;

// The font of the secondary text. The Default value is the regular Roboto font
// of size 14.
@property(nonatomic, null_resettable, copy) UIFont* detailTextFont;

// The color of the secondary text. The default value is the 500 tint color of
// the grey palette.
@property(nonatomic, null_resettable, copy) UIColor* detailTextColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_TEXT_ITEM_H_