# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with generate_compiled_resources_gyp.py, please do not edit.
{
  'targets': [
    {
      'target_name': 'iron-flex-layout-extracted',
      'dependencies': [
        'iron-shadow-flex-layout-extracted',
      ],
      'includes': ['../../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'iron-shadow-flex-layout-extracted',
      'includes': ['../../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}