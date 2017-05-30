// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/offline_url_utils.h"

#include "base/md5.h"
#include "base/strings/stringprintf.h"

namespace {
const char kOfflineDirectory[] = "Offline";
const char kMainPageFileName[] = "page.html";
const char kPDFFileName[] = "file.pdf";
}  // namespace

namespace reading_list {

base::FilePath OfflineRootDirectoryPath(const base::FilePath& profile_path) {
  return profile_path.Append(FILE_PATH_LITERAL(kOfflineDirectory));
}

std::string OfflineURLDirectoryID(const GURL& url) {
  return base::MD5String(url.spec());
}

base::FilePath OfflineURLDirectoryAbsolutePath(
    const base::FilePath& profile_path,
    const GURL& url) {
  return OfflineURLAbsolutePathFromRelativePath(
      profile_path, base::FilePath(OfflineURLDirectoryID(url)));
}

base::FilePath OfflinePagePath(const GURL& url, OfflineFileType type) {
  base::FilePath directory(OfflineURLDirectoryID(url));
  switch (type) {
    case OFFLINE_TYPE_HTML:
      return directory.Append(FILE_PATH_LITERAL(kMainPageFileName));
    case OFFLINE_TYPE_PDF:
      return directory.Append(FILE_PATH_LITERAL(kPDFFileName));
  }
}

base::FilePath OfflineURLAbsolutePathFromRelativePath(
    const base::FilePath& profile_path,
    const base::FilePath& relative_path) {
  return OfflineRootDirectoryPath(profile_path).Append(relative_path);
}
}