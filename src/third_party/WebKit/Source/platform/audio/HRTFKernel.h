/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef HRTFKernel_h
#define HRTFKernel_h

#include "platform/audio/FFTFrame.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class AudioChannel;

// HRTF stands for Head-Related Transfer Function.
// HRTFKernel is a frequency-domain representation of an impulse-response used
// as part of the spatialized panning system.  For a given azimuth / elevation
// angle there will be one HRTFKernel for the left ear transfer function, and
// one for the right ear.  The leading delay (average group delay) for each
// impulse response is extracted:
//      m_fftFrame is the frequency-domain representation of the impulse
//      response with the delay removed
//      m_frameDelay is the leading delay of the original impulse response.
class PLATFORM_EXPORT HRTFKernel {
  USING_FAST_MALLOC(HRTFKernel);
  WTF_MAKE_NONCOPYABLE(HRTFKernel);

 public:
  // Note: this is destructive on the passed in AudioChannel.
  // The length of channel must be a power of two.
  static std::unique_ptr<HRTFKernel> create(AudioChannel* channel,
                                            size_t fftSize,
                                            float sampleRate) {
    return WTF::wrapUnique(new HRTFKernel(channel, fftSize, sampleRate));
  }

  static std::unique_ptr<HRTFKernel> create(std::unique_ptr<FFTFrame> fftFrame,
                                            float frameDelay,
                                            float sampleRate) {
    return WTF::wrapUnique(
        new HRTFKernel(std::move(fftFrame), frameDelay, sampleRate));
  }

  // Given two HRTFKernels, and an interpolation factor x: 0 -> 1, returns an
  // interpolated HRTFKernel.
  static std::unique_ptr<HRTFKernel>
  createInterpolatedKernel(HRTFKernel* kernel1, HRTFKernel* kernel2, float x);

  FFTFrame* fftFrame() { return m_fftFrame.get(); }

  size_t fftSize() const { return m_fftFrame->fftSize(); }
  float frameDelay() const { return m_frameDelay; }

  float sampleRate() const { return m_sampleRate; }
  double nyquist() const { return 0.5 * sampleRate(); }

  // Converts back into impulse-response form.
  std::unique_ptr<AudioChannel> createImpulseResponse();

 private:
  // Note: this is destructive on the passed in AudioChannel.
  HRTFKernel(AudioChannel*, size_t fftSize, float sampleRate);

  HRTFKernel(std::unique_ptr<FFTFrame> fftFrame,
             float frameDelay,
             float sampleRate)
      : m_fftFrame(std::move(fftFrame)),
        m_frameDelay(frameDelay),
        m_sampleRate(sampleRate) {}

  std::unique_ptr<FFTFrame> m_fftFrame;
  float m_frameDelay;
  float m_sampleRate;
};

typedef Vector<std::unique_ptr<HRTFKernel>> HRTFKernelList;

}  // namespace blink

#endif  // HRTFKernel_h