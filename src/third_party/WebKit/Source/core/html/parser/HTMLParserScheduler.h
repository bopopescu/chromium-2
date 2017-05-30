/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLParserScheduler_h
#define HTMLParserScheduler_h

#include "core/html/parser/NestingLevelIncrementer.h"
#include "platform/WebTaskRunner.h"
#include "wtf/Allocator.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class HTMLDocumentParser;
class WebTaskRunner;

class PumpSession : public NestingLevelIncrementer {
  STACK_ALLOCATED();

 public:
  PumpSession(unsigned& nestingLevel);
  ~PumpSession();
};

class SpeculationsPumpSession : public NestingLevelIncrementer {
  STACK_ALLOCATED();

 public:
  SpeculationsPumpSession(unsigned& nestingLevel);
  ~SpeculationsPumpSession();

  double elapsedTime() const;
  void addedElementTokens(size_t count);
  size_t processedElementTokens() const { return m_processedElementTokens; }

 private:
  double m_startTime;
  size_t m_processedElementTokens;
};

class HTMLParserScheduler final
    : public GarbageCollectedFinalized<HTMLParserScheduler> {
  WTF_MAKE_NONCOPYABLE(HTMLParserScheduler);

 public:
  static HTMLParserScheduler* create(HTMLDocumentParser* parser,
                                     RefPtr<WebTaskRunner> loadingTaskRunner) {
    return new HTMLParserScheduler(parser, std::move(loadingTaskRunner));
  }
  ~HTMLParserScheduler();

  bool isScheduledForResume() const;
  void scheduleForResume();
  bool yieldIfNeeded(const SpeculationsPumpSession&, bool startingScript);

  /**
     * Can only be called if this scheduler is suspended. If this is called,
     * then after the scheduler is resumed by calling resume(), this call
     * ensures that HTMLDocumentParser::resumeAfterYield will be called. Used to
     * signal this scheduler that the background html parser sent chunks to
     * HTMLDocumentParser while it was suspended.
     */
  void forceResumeAfterYield();

  void suspend();
  void resume();

  void detach();  // Clear active tasks if any.

  DECLARE_TRACE();

 private:
  HTMLParserScheduler(HTMLDocumentParser*, RefPtr<WebTaskRunner>);

  bool shouldYield(const SpeculationsPumpSession&, bool startingScript) const;
  void continueParsing();

  Member<HTMLDocumentParser> m_parser;
  RefPtr<WebTaskRunner> m_loadingTaskRunner;

  TaskHandle m_cancellableContinueParseTaskHandle;
  bool m_isSuspendedWithActiveTimer;
};

}  // namespace blink

#endif