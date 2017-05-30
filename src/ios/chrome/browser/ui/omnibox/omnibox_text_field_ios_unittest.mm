// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A category for making existing methods visible for use in these tests.
@interface OmniboxTextFieldIOS (VisibleForTesting)
- (CGRect)rectForDrawTextInRect:(CGRect)rect;
@end

namespace {

class OmniboxTextFieldIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    // This rect is fairly arbitrary. The text field just needs a non-zero width
    // so that the pre-edit label's text alignment can be tested.
    CGRect rect = CGRectMake(0, 0, 100, 20);
    textfield_ = [[OmniboxTextFieldIOS alloc] initWithFrame:rect];
    [[[UIApplication sharedApplication] keyWindow] addSubview:textfield_];
  };

  void TearDown() override { [textfield_ removeFromSuperview]; }

  BOOL IsCopyUrlInMenu() {
    UIMenuController* menuController = [UIMenuController sharedMenuController];
    NSString* const kTitle = l10n_util::GetNSString(IDS_IOS_COPY_URL);
    for (UIMenuItem* item in menuController.menuItems) {
      if ([item.title isEqual:kTitle])
        return YES;
    }
    return NO;
  };

  void ExpectRectEqual(CGRect expectedRect, CGRect actualRect) {
    EXPECT_EQ(expectedRect.origin.x, actualRect.origin.x);
    EXPECT_EQ(expectedRect.origin.y, actualRect.origin.y);
    EXPECT_EQ(expectedRect.size.width, actualRect.size.width);
    EXPECT_EQ(expectedRect.size.height, actualRect.size.height);
  }

  // Verifies that the |selectedNSRange| function properly converts from opaque
  // UITextRanges to NSRanges.  This function selects blocks of text in the text
  // field and compares the field's actual selected text to the converted
  // NSRange.
  void VerifySelectedNSRanges(NSString* text) {
    // The NSRange conversion mechanism only works when the field is first
    // responder.
    [textfield_ setText:text];
    [textfield_ becomeFirstResponder];
    ASSERT_TRUE([textfield_ isFirstResponder]);

    // |i| and |j| hold the start and end offsets of the range that is currently
    // being tested.  This function iterates through all possible combinations
    // of |i| and |j|.
    NSInteger i = 0;
    NSInteger j = i + 1;
    UITextPosition* beginning = [textfield_ beginningOfDocument];
    UITextPosition* start =
        [textfield_ positionFromPosition:[textfield_ beginningOfDocument]
                                  offset:i];

    // In order to avoid making any assumptions about the length of the text in
    // the field, this test operates by incrementing the |i| and |j| offsets and
    // converting them to opaque UITextPositions.  If either |i| or |j| are
    // invalid offsets for the current field text,
    // |positionFromPosition:offset:| is documented to return nil.  This is used
    // as a signal to stop incrementing that offset and reset (or end the test).
    while (start) {
      UITextPosition* end =
          [textfield_ positionFromPosition:beginning offset:j];
      while (end) {
        [textfield_
            setSelectedTextRange:[textfield_ textRangeFromPosition:start
                                                        toPosition:end]];

        // There are two ways to get the selected text:
        // 1) Ask the field for it directly.
        // 2) Compute the selected NSRange and use that to extract a substring
        //    from the field's text.
        // This block of code ensures that the two methods give identical text.
        NSRange nsrange = [textfield_ selectedNSRange];
        NSString* nstext = [[textfield_ text] substringWithRange:nsrange];
        UITextRange* uirange = [textfield_ selectedTextRange];
        NSString* uitext = [textfield_ textInRange:uirange];
        EXPECT_NSEQ(nstext, uitext);

        // Increment |j| and |end| for the next iteration of the inner while
        // loop.
        ++j;
        end = [textfield_ positionFromPosition:beginning offset:j];
      }

      // Increment |i| and |start| for the next iteration of the outer while
      // loop.  This also requires |j| to be reset.
      ++i;
      j = i + 1;
      start = [textfield_ positionFromPosition:beginning offset:i];
    }

    [textfield_ resignFirstResponder];
  }

  OmniboxTextFieldIOS* textfield_;
};

TEST_F(OmniboxTextFieldIOSTest, BecomeFirstResponderAddsCopyURLMenuItem) {
  // The 'Copy URL' menu item should not be present before this test runs.
  EXPECT_FALSE(IsCopyUrlInMenu());

  // Call |becomeFirstResponder| and verify the Copy URL menu item was added.
  UIMenuController* menuController = [UIMenuController sharedMenuController];
  NSUInteger expectedItems = [menuController.menuItems count] + 1;
  [textfield_ becomeFirstResponder];
  EXPECT_EQ(expectedItems, [menuController.menuItems count]);
  EXPECT_TRUE(IsCopyUrlInMenu());

  // Call |becomeFirstResponder| again and verify the Copy URL menu item is not
  // added again.
  [textfield_ becomeFirstResponder];
  EXPECT_EQ(expectedItems, [menuController.menuItems count]);
  EXPECT_TRUE(IsCopyUrlInMenu());
}

TEST_F(OmniboxTextFieldIOSTest, ResignFirstResponderRemovesCopyURLMenuItem) {
  // Call |becomeFirstResponder| to add the 'Copy URL' menu item so this test
  // can remove it.
  [textfield_ becomeFirstResponder];

  UIMenuController* menuController = [UIMenuController sharedMenuController];
  NSUInteger expectedItems = [menuController.menuItems count] - 1;
  [textfield_ resignFirstResponder];
  EXPECT_EQ(expectedItems, [menuController.menuItems count]);
  EXPECT_FALSE(IsCopyUrlInMenu());
}

TEST_F(OmniboxTextFieldIOSTest, enterPreEditState_preEditTextAlignment_short) {
  [textfield_ setText:@"s"];
  [textfield_ becomeFirstResponder];
  [textfield_ enterPreEditState];
  UILabel* preEditLabel = [textfield_ preEditStaticLabel];
  EXPECT_EQ(NSTextAlignmentLeft, preEditLabel.textAlignment);
  [textfield_ resignFirstResponder];
}

TEST_F(OmniboxTextFieldIOSTest, enterPreEditState_preEditTextAlignment_long) {
  [textfield_ setText:@"some really long text that is wider than the omnibox"];
  [textfield_ becomeFirstResponder];
  [textfield_ enterPreEditState];
  UILabel* preEditLabel = [textfield_ preEditStaticLabel];
  EXPECT_EQ(NSTextAlignmentRight, preEditLabel.textAlignment);
  [textfield_ resignFirstResponder];
}

TEST_F(OmniboxTextFieldIOSTest, enterPreEditState_preEditTextAlignment_change) {
  [textfield_ setText:@"s"];
  [textfield_ becomeFirstResponder];
  [textfield_ enterPreEditState];
  // Simulate changing the omnibox text while in pre-edit state.
  [textfield_ setText:@"some really long text that is wider than the omnibox"];
  [textfield_ layoutSubviews];
  UILabel* preEditLabel = [textfield_ preEditStaticLabel];
  EXPECT_EQ(NSTextAlignmentLeft, preEditLabel.textAlignment);
  [textfield_ resignFirstResponder];
}

TEST_F(OmniboxTextFieldIOSTest, rectForDrawTextInRect_entireURLFits) {
  NSString* text = @"http://www.google.com";
  [textfield_ setText:text];
  CGSize textSize = [[textfield_ attributedText] size];
  CGFloat widthForEntireURL = ceil(textSize.width) + 10;

  CGRect inputRect = CGRectMake(0, 0, widthForEntireURL, textSize.height);
  CGRect actualRect = [textfield_ rectForDrawTextInRect:inputRect];
  ExpectRectEqual(inputRect, actualRect);
}

TEST_F(OmniboxTextFieldIOSTest, rectForDrawTextInRect_clippedPrefix) {
  NSString* text = @"http://www.google.com";
  [textfield_ setText:text];
  CGSize textSize = [[textfield_ attributedText] size];
  CGFloat clippedWidth = 10;
  CGFloat widthForPartOfHost = ceil(textSize.width) - clippedWidth;

  CGRect inputRect = CGRectMake(0, 0, widthForPartOfHost, textSize.height);
  CGRect actualRect = [textfield_ rectForDrawTextInRect:inputRect];
  CGRect expectedRect =
      CGRectMake(-1 * clippedWidth, 0, ceil(textSize.width), textSize.height);
  ExpectRectEqual(expectedRect, actualRect);
}

TEST_F(OmniboxTextFieldIOSTest, rectForDrawTextInRect_clippedSuffix) {
  NSString* text = @"http://www.google.com/somelongpath";
  [textfield_ setText:text];
  CGSize textSize = [[textfield_ attributedText] size];
  CGFloat widthForPartOfPath = ceil(textSize.width) - 10;

  CGRect inputRect = CGRectMake(0, 0, widthForPartOfPath, textSize.height);
  CGRect actualRect = [textfield_ rectForDrawTextInRect:inputRect];
  CGRect expectedRect = CGRectMake(0, 0, ceil(textSize.width), textSize.height);
  ExpectRectEqual(expectedRect, actualRect);
}

TEST_F(OmniboxTextFieldIOSTest, rectForDrawTextInRect_noScheme) {
  NSString* text = @"www.google.com";
  [textfield_ setText:text];
  CGSize textSize = [[textfield_ attributedText] size];

  CGRect inputRect = CGRectMake(0, 0, ceil(textSize.width), textSize.height);
  CGRect actualRect = [textfield_ rectForDrawTextInRect:inputRect];
  ExpectRectEqual(inputRect, actualRect);
}

// When the text doesn't contain a host the method bails early and returns
// the |rect| passed in.
TEST_F(OmniboxTextFieldIOSTest, rectForDrawTextInRect_noHost) {
  NSString* text = @"http://";
  [textfield_ setText:text];
  CGSize textSize = [[textfield_ attributedText] size];

  CGRect inputRect = CGRectMake(0, 0, ceil(textSize.width), textSize.height);
  CGRect actualRect = [textfield_ rectForDrawTextInRect:inputRect];
  ExpectRectEqual(inputRect, actualRect);
}

TEST_F(OmniboxTextFieldIOSTest, SelectedRanges) {
  base::FilePath test_data_directory;
  ASSERT_TRUE(PathService::Get(ios::DIR_TEST_DATA, &test_data_directory));
  base::FilePath test_file = test_data_directory.Append(
      FILE_PATH_LITERAL("omnibox/selected_ranges.txt"));
  ASSERT_TRUE(base::PathExists(test_file));

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file, &contents));
  std::vector<std::string> test_strings = base::SplitString(
      contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  for (size_t i = 0; i < test_strings.size(); ++i) {
    if (test_strings[i].size() > 0) {
      VerifySelectedNSRanges(base::SysUTF8ToNSString(test_strings[i]));
    }
  }
}

TEST_F(OmniboxTextFieldIOSTest, SelectExitsPreEditState) {
  [textfield_ enterPreEditState];
  EXPECT_TRUE([textfield_ isPreEditing]);
  [textfield_ select:nil];
  EXPECT_FALSE([textfield_ isPreEditing]);
}

TEST_F(OmniboxTextFieldIOSTest, SelectAllExitsPreEditState) {
  [textfield_ enterPreEditState];
  EXPECT_TRUE([textfield_ isPreEditing]);
  [textfield_ selectAll:nil];
  EXPECT_FALSE([textfield_ isPreEditing]);
}

}  // namespace