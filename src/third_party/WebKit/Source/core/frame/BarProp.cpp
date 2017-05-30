/*
 * Copyright (C) 2007, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/frame/BarProp.h"

#include "core/frame/LocalFrame.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"

namespace blink {

BarProp::BarProp(LocalFrame* frame, Type type)
    : DOMWindowClient(frame), m_type(type) {}

DEFINE_TRACE(BarProp) {
  DOMWindowClient::trace(visitor);
}

bool BarProp::visible() const {
  if (!frame())
    return false;
  DCHECK(frame()->page());

  switch (m_type) {
    case Locationbar:
    case Personalbar:
    case Toolbar:
      return frame()->page()->chromeClient().toolbarsVisible();
    case Menubar:
      return frame()->page()->chromeClient().menubarVisible();
    case Scrollbars:
      return frame()->page()->chromeClient().scrollbarsVisible();
    case Statusbar:
      return frame()->page()->chromeClient().statusbarVisible();
  }

  NOTREACHED();
  return false;
}

}  // namespace blink