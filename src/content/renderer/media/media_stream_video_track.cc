// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_track.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/media_stream_constraints_util_video_device.h"

namespace content {

namespace {
void ResetCallback(std::unique_ptr<VideoCaptureDeliverFrameCB> callback) {
  // |callback| will be deleted when this exits.
}

// Empty method used for keeping a reference to the original media::VideoFrame.
// The reference to |frame| is kept in the closure that calls this method.
void ReleaseOriginalFrame(const scoped_refptr<media::VideoFrame>& frame) {
}

}  // namespace

// MediaStreamVideoTrack::FrameDeliverer is a helper class used for registering
// VideoCaptureDeliverFrameCB on the main render thread to receive video frames
// on the IO-thread.
// Frames are only delivered to the sinks if the track is enabled. If the track
// is disabled, a black frame is instead forwarded to the sinks at the same
// frame rate.
class MediaStreamVideoTrack::FrameDeliverer
    : public base::RefCountedThreadSafe<FrameDeliverer> {
 public:
  typedef MediaStreamVideoSink* VideoSinkId;

  FrameDeliverer(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                 bool enabled);

  void SetEnabled(bool enabled);

  // Add |callback| to receive video frames on the IO-thread.
  // Must be called on the main render thread.
  void AddCallback(VideoSinkId id, const VideoCaptureDeliverFrameCB& callback);

  // Removes |callback| associated with |id| from receiving video frames if |id|
  // has been added. It is ok to call RemoveCallback even if the |id| has not
  // been added. Note that the added callback will be reset on the main thread.
  // Must be called on the main render thread.
  void RemoveCallback(VideoSinkId id);

  // Triggers all registered callbacks with |frame|, |format| and
  // |estimated_capture_time| as parameters. Must be called on the IO-thread.
  void DeliverFrameOnIO(const scoped_refptr<media::VideoFrame>& frame,
                        base::TimeTicks estimated_capture_time);

 private:
  friend class base::RefCountedThreadSafe<FrameDeliverer>;
  virtual ~FrameDeliverer();
  void AddCallbackOnIO(VideoSinkId id,
                       const VideoCaptureDeliverFrameCB& callback);
  void RemoveCallbackOnIO(
      VideoSinkId id,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  void SetEnabledOnIO(bool enabled);
  // Returns a black frame where the size and time stamp is set to the same as
  // as in |reference_frame|.
  scoped_refptr<media::VideoFrame> GetBlackFrame(
      const scoped_refptr<media::VideoFrame>& reference_frame);

  // Used to DCHECK that AddCallback and RemoveCallback are called on the main
  // Render Thread.
  base::ThreadChecker main_render_thread_checker_;
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  bool enabled_;
  scoped_refptr<media::VideoFrame> black_frame_;

  typedef std::pair<VideoSinkId, VideoCaptureDeliverFrameCB>
      VideoIdCallbackPair;
  std::vector<VideoIdCallbackPair> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FrameDeliverer);
};

MediaStreamVideoTrack::FrameDeliverer::FrameDeliverer(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    bool enabled)
    : io_task_runner_(io_task_runner), enabled_(enabled) {
  DCHECK(io_task_runner_.get());
}

MediaStreamVideoTrack::FrameDeliverer::~FrameDeliverer() {
  DCHECK(callbacks_.empty());
}

void MediaStreamVideoTrack::FrameDeliverer::AddCallback(
    VideoSinkId id,
    const VideoCaptureDeliverFrameCB& callback) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FrameDeliverer::AddCallbackOnIO, this, id, callback));
}

void MediaStreamVideoTrack::FrameDeliverer::AddCallbackOnIO(
    VideoSinkId id,
    const VideoCaptureDeliverFrameCB& callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  callbacks_.push_back(std::make_pair(id, callback));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveCallback(VideoSinkId id) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&FrameDeliverer::RemoveCallbackOnIO, this, id,
                            base::ThreadTaskRunnerHandle::Get()));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveCallbackOnIO(
    VideoSinkId id,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  std::vector<VideoIdCallbackPair>::iterator it = callbacks_.begin();
  for (; it != callbacks_.end(); ++it) {
    if (it->first == id) {
      // Callback is copied to heap and then deleted on the target thread.
      std::unique_ptr<VideoCaptureDeliverFrameCB> callback;
      callback.reset(new VideoCaptureDeliverFrameCB(it->second));
      callbacks_.erase(it);
      task_runner->PostTask(
          FROM_HERE, base::Bind(&ResetCallback, base::Passed(&callback)));
      return;
    }
  }
}

void MediaStreamVideoTrack::FrameDeliverer::SetEnabled(bool enabled) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&FrameDeliverer::SetEnabledOnIO, this, enabled));
}

void MediaStreamVideoTrack::FrameDeliverer::SetEnabledOnIO(bool enabled) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  enabled_ = enabled;
  if (enabled_)
    black_frame_ = NULL;
}

void MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO(
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  const scoped_refptr<media::VideoFrame>& video_frame =
      enabled_ ? frame : GetBlackFrame(frame);
  for (const auto& entry : callbacks_)
    entry.second.Run(video_frame, estimated_capture_time);
}

scoped_refptr<media::VideoFrame>
MediaStreamVideoTrack::FrameDeliverer::GetBlackFrame(
    const scoped_refptr<media::VideoFrame>& reference_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!black_frame_.get() ||
      black_frame_->natural_size() != reference_frame->natural_size()) {
    black_frame_ =
        media::VideoFrame::CreateBlackFrame(reference_frame->natural_size());
  }

  // Wrap |black_frame_| so we get a fresh timestamp we can modify. Frames
  // returned from this function may still be in use.
  scoped_refptr<media::VideoFrame> wrapped_black_frame =
      media::VideoFrame::WrapVideoFrame(black_frame_, black_frame_->format(),
                                        black_frame_->visible_rect(),
                                        black_frame_->natural_size());
  if (!wrapped_black_frame)
    return nullptr;
  wrapped_black_frame->AddDestructionObserver(
      base::Bind(&ReleaseOriginalFrame, black_frame_));

  wrapped_black_frame->set_timestamp(reference_frame->timestamp());
  base::TimeTicks reference_time;
  if (reference_frame->metadata()->GetTimeTicks(
          media::VideoFrameMetadata::REFERENCE_TIME, &reference_time)) {
    wrapped_black_frame->metadata()->SetTimeTicks(
        media::VideoFrameMetadata::REFERENCE_TIME, reference_time);
  }

  return wrapped_black_frame;
}

// static
blink::WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled) {
  blink::WebMediaStreamTrack track;
  track.initialize(source->owner());
  track.setTrackData(
      new MediaStreamVideoTrack(source, constraints, callback, enabled));
  return track;
}

// static
MediaStreamVideoTrack* MediaStreamVideoTrack::GetVideoTrack(
     const blink::WebMediaStreamTrack& track) {
  if (track.isNull() ||
      track.source().getType() != blink::WebMediaStreamSource::TypeVideo) {
    return nullptr;
  }
  return static_cast<MediaStreamVideoTrack*>(track.getTrackData());
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled)
    : MediaStreamTrack(true),
      frame_deliverer_(
          new MediaStreamVideoTrack::FrameDeliverer(source->io_task_runner(),
                                                    enabled)),
      constraints_(constraints),
      source_(source->GetWeakPtr()) {
  DCHECK(!constraints.isNull());
  source->AddTrack(this,
                   base::Bind(
                       &MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO,
                       frame_deliverer_),
                   constraints, callback);
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(sinks_.empty());
  Stop();
  DVLOG(3) << "~MediaStreamVideoTrack()";
}

void MediaStreamVideoTrack::AddSink(MediaStreamVideoSink* sink,
                                    const VideoCaptureDeliverFrameCB& callback,
                                    bool is_sink_secure) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end());
  sinks_.push_back(sink);
  frame_deliverer_->AddCallback(sink, callback);
  secure_tracker_.Add(sink, is_sink_secure);
  // Request source to deliver a frame because a new sink is added.
  if (!source_)
    return;
  source_->UpdateHasConsumers(this, true);
  source_->RequestRefreshFrame();
  source_->UpdateCapturingLinkSecure(this,
                                     secure_tracker_.is_capturing_secure());
}

void MediaStreamVideoTrack::RemoveSink(MediaStreamVideoSink* sink) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  std::vector<MediaStreamVideoSink*>::iterator it =
      std::find(sinks_.begin(), sinks_.end(), sink);
  DCHECK(it != sinks_.end());
  sinks_.erase(it);
  frame_deliverer_->RemoveCallback(sink);
  secure_tracker_.Remove(sink);
  if (!source_)
    return;
  if (sinks_.empty())
    source_->UpdateHasConsumers(this, false);
  source_->UpdateCapturingLinkSecure(this,
                                     secure_tracker_.is_capturing_secure());
}

void MediaStreamVideoTrack::SetEnabled(bool enabled) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  frame_deliverer_->SetEnabled(enabled);
  for (auto* sink : sinks_)
    sink->OnEnabledChanged(enabled);
}

void MediaStreamVideoTrack::SetContentHint(
    blink::WebMediaStreamTrack::ContentHintType content_hint) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  for (auto* sink : sinks_)
    sink->OnContentHintChanged(content_hint);
}

void MediaStreamVideoTrack::Stop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (source_) {
    source_->RemoveTrack(this);
    source_ = NULL;
  }
  OnReadyStateChanged(blink::WebMediaStreamSource::ReadyStateEnded);
}

void MediaStreamVideoTrack::getSettings(
    blink::WebMediaStreamTrack::Settings& settings) {
  // TODO(hta): Extract the real value.
  settings.deviceId = blink::WebString("video device ID");
  if (!source_)
    return;

  const media::VideoCaptureFormat* format = source_->GetCurrentFormat();
  if (format) {
    settings.frameRate = format->frame_rate;
    settings.width = format->frame_size.width();
    settings.height = format->frame_size.height();
    settings.videoKind = GetVideoKindForFormat(*format);
  }
  switch (source_->device_info().device.video_facing) {
    case media::MEDIA_VIDEO_FACING_NONE:
      settings.facingMode = blink::WebMediaStreamTrack::FacingMode::None;
      break;
    case media::MEDIA_VIDEO_FACING_USER:
      settings.facingMode = blink::WebMediaStreamTrack::FacingMode::User;
      break;
    case media::MEDIA_VIDEO_FACING_ENVIRONMENT:
      settings.facingMode = blink::WebMediaStreamTrack::FacingMode::Environment;
      break;
    default:
      settings.facingMode = blink::WebMediaStreamTrack::FacingMode::None;
      break;
  }
  const base::Optional<CameraCalibration> calibration =
      source_->device_info().device.camera_calibration;
  if (calibration) {
    settings.depthNear = calibration->depth_near;
    settings.depthFar = calibration->depth_far;
    settings.focalLengthX = calibration->focal_length_x;
    settings.focalLengthY = calibration->focal_length_y;
  }
}

void MediaStreamVideoTrack::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  for (auto* sink : sinks_)
    sink->OnReadyStateChanged(state);
}

}  // namespace content