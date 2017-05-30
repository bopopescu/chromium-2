// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_H_
#define CC_TREES_LAYER_TREE_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/debug/micro_benchmark_controller.h"
#include "cc/input/browser_controls_state.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/input_handler.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/output/swap_promise.h"
#include "cc/resources/resource_format.h"
#include "cc/surfaces/surface_reference_owner.h"
#include "cc/surfaces/surface_sequence_generator.h"
#include "cc/trees/compositor_mode.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/proxy.h"
#include "cc/trees/swap_promise_manager.h"
#include "cc/trees/target_property.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class HeadsUpDisplayLayer;
class Layer;
class LayerTreeHostClient;
class LayerTreeHostImpl;
class LayerTreeHostImplClient;
class LayerTreeHostSingleThreadClient;
class LayerTreeMutator;
class MutatorEvents;
class MutatorHost;
struct PendingPageScaleAnimation;
class RenderingStatsInstrumentation;
class TaskGraphRunner;
class UIResourceManager;
struct RenderingStats;
struct ScrollAndScaleSet;

class CC_EXPORT LayerTreeHost : public NON_EXPORTED_BASE(SurfaceReferenceOwner),
                                public NON_EXPORTED_BASE(MutatorHostClient) {
 public:
  // TODO(sad): InitParams should be a movable type so that it can be
  // std::move()d to the Create* functions.
  struct CC_EXPORT InitParams {
    LayerTreeHostClient* client = nullptr;
    TaskGraphRunner* task_graph_runner = nullptr;
    LayerTreeSettings const* settings = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner;
    MutatorHost* mutator_host = nullptr;

    // The image worker task runner is used to schedule image decodes. The
    // compositor thread may make sync calls to this thread, analogous to the
    // raster worker threads.
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner;

    InitParams();
    ~InitParams();
  };

  static std::unique_ptr<LayerTreeHost> CreateThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      InitParams* params);

  static std::unique_ptr<LayerTreeHost> CreateSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      InitParams* params);

  ~LayerTreeHost() override;

  // Returns the process global unique identifier for this LayerTreeHost.
  int GetId() const;

  // The current source frame number. This is incremented for each main frame
  // update(commit) pushed to the compositor thread.
  int SourceFrameNumber() const;

  // Returns the UIResourceManager used to create UIResources for
  // UIResourceLayers pushed to the LayerTree.
  UIResourceManager* GetUIResourceManager() const;

  // Returns the TaskRunnerProvider used to access the main and compositor
  // thread task runners.
  TaskRunnerProvider* GetTaskRunnerProvider() const;

  // Returns the settings used by this host.
  const LayerTreeSettings& GetSettings() const;

  // Sets the client id used to generate the SurfaceId that uniquely identifies
  // the Surfaces produced by this compositor.
  void SetFrameSinkId(const FrameSinkId& frame_sink_id);

  // Sets the LayerTreeMutator interface used to directly mutate the compositor
  // state on the compositor thread. (Compositor-Worker)
  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator);

  // Call this function when you expect there to be a swap buffer.
  // See swap_promise.h for how to use SwapPromise.
  void QueueSwapPromise(std::unique_ptr<SwapPromise> swap_promise);

  // Returns the SwapPromiseManager used to create SwapPromiseMonitors for this
  // host.
  SwapPromiseManager* GetSwapPromiseManager();

  // Sets whether the content is suitable to use Gpu Rasterization.
  void SetHasGpuRasterizationTrigger(bool has_trigger);

  // Visibility and CompositorFrameSink -------------------------------

  void SetVisible(bool visible);
  bool IsVisible() const;

  // Called in response to an CompositorFrameSink request made to the client
  // using LayerTreeHostClient::RequestNewCompositorFrameSink. The client will
  // be informed of the CompositorFrameSink initialization status using
  // DidInitializaCompositorFrameSink or DidFailToInitializeCompositorFrameSink.
  // The request is completed when the host successfully initializes an
  // CompositorFrameSink.
  void SetCompositorFrameSink(
      std::unique_ptr<CompositorFrameSink> compositor_frame_sink);

  // Forces the host to immediately release all references to the
  // CompositorFrameSink, if any. Can be safely called any time.
  std::unique_ptr<CompositorFrameSink> ReleaseCompositorFrameSink();

  // Frame Scheduling (main and compositor frames) requests -------

  // Requests a main frame update even if no content has changed. This is used,
  // for instance in the case of RequestAnimationFrame from blink to ensure the
  // main frame update is run on the next tick without pre-emptively forcing a
  // full commit synchronization or layer updates.
  void SetNeedsAnimate();

  // Requests a main frame update and also ensure that the host pulls layer
  // updates from the client, even if no content might have changed, without
  // forcing a full commit synchronization.
  virtual void SetNeedsUpdateLayers();

  // Requests that the next main frame update performs a full commit
  // synchronization.
  virtual void SetNeedsCommit();

  // Requests that the next frame re-chooses crisp raster scales for all layers.
  void SetNeedsRecalculateRasterScales();

  // Returns true if a main frame with commit synchronization has been
  // requested.
  bool CommitRequested() const;

  // Enables/disables the compositor from requesting main frame updates from the
  // client.
  void SetDeferCommits(bool defer_commits);

  // Synchronously performs a main frame update and layer updates. Used only in
  // single threaded mode when the compositor's internal scheduling is disabled.
  void LayoutAndUpdateLayers();

  // Synchronously performs a complete main frame update, commit and compositor
  // frame. Used only in single threaded mode when the compositor's internal
  // scheduling is disabled.
  void Composite(base::TimeTicks frame_begin_time);

  // Requests a redraw (compositor frame) for the given rect.
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect);

  // Requests a main frame (including layer updates) and ensures that this main
  // frame results in a redraw for the complete viewport when producing the
  // CompositorFrame.
  void SetNextCommitForcesRedraw();

  // Input Handling ---------------------------------------------

  // Notifies the compositor that input from the browser is being throttled till
  // the next commit. The compositor will prioritize activation of the pending
  // tree so a commit can be performed.
  void NotifyInputThrottledUntilCommit();

  // Sets the state of the browser controls. (Used for URL bar animations on
  // android).
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate);

  // Returns a reference to the InputHandler used to respond to input events on
  // the compositor thread.
  const base::WeakPtr<InputHandler>& GetInputHandler() const;

  // Informs the compositor that an active fling gesture being processed on the
  // main thread has been finished.
  void DidStopFlinging();

  // Debugging and benchmarks ---------------------------------
  void SetDebugState(const LayerTreeDebugState& debug_state);
  const LayerTreeDebugState& GetDebugState() const;

  // Returns the id of the benchmark on success, 0 otherwise.
  int ScheduleMicroBenchmark(const std::string& benchmark_name,
                             std::unique_ptr<base::Value> value,
                             const MicroBenchmark::DoneCallback& callback);

  // Returns true if the message was successfully delivered and handled.
  bool SendMessageToMicroBenchmark(int id, std::unique_ptr<base::Value> value);

  // When the main thread informs the impl thread that it is ready to commit,
  // generally it would remain blocked till the main thread state is copied to
  // the pending tree. Calling this would ensure that the main thread remains
  // blocked till the pending tree is activated.
  void SetNextCommitWaitsForActivation();

  // The LayerTreeHost tracks whether the content is suitable for Gpu raster.
  // Calling this will reset it back to not suitable state.
  void ResetGpuRasterizationTracking();

  void SetRootLayer(scoped_refptr<Layer> root_layer);
  Layer* root_layer() { return root_layer_.get(); }
  const Layer* root_layer() const { return root_layer_.get(); }

  void RegisterViewportLayers(scoped_refptr<Layer> overscroll_elasticity_layer,
                              scoped_refptr<Layer> page_scale_layer,
                              scoped_refptr<Layer> inner_viewport_scroll_layer,
                              scoped_refptr<Layer> outer_viewport_scroll_layer);

  Layer* overscroll_elasticity_layer() const {
    return overscroll_elasticity_layer_.get();
  }
  Layer* page_scale_layer() const { return page_scale_layer_.get(); }
  Layer* inner_viewport_scroll_layer() const {
    return inner_viewport_scroll_layer_.get();
  }
  Layer* outer_viewport_scroll_layer() const {
    return outer_viewport_scroll_layer_.get();
  }

  void RegisterSelection(const LayerSelection& selection);
  const LayerSelection& selection() const { return selection_; }

  void SetHaveScrollEventHandlers(bool have_event_handlers);
  bool have_scroll_event_handlers() const {
    return have_scroll_event_handlers_;
  }

  void SetEventListenerProperties(EventListenerClass event_class,
                                  EventListenerProperties event_properties);
  EventListenerProperties event_listener_properties(
      EventListenerClass event_class) const {
    return event_listener_properties_[static_cast<size_t>(event_class)];
  }

  void SetViewportSize(const gfx::Size& device_viewport_size);
  gfx::Size device_viewport_size() const { return device_viewport_size_; }

  void SetBrowserControlsHeight(float height, bool shrink);
  void SetBrowserControlsShownRatio(float ratio);
  void SetBottomControlsHeight(float height);

  void SetPageScaleFactorAndLimits(float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor);
  float page_scale_factor() const { return page_scale_factor_; }
  float min_page_scale_factor() const { return min_page_scale_factor_; }
  float max_page_scale_factor() const { return max_page_scale_factor_; }

  void set_background_color(SkColor color) { background_color_ = color; }
  SkColor background_color() const { return background_color_; }

  void set_has_transparent_background(bool transparent) {
    has_transparent_background_ = transparent;
  }
  bool has_transparent_background() const {
    return has_transparent_background_;
  }

  void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                               bool use_anchor,
                               float scale,
                               base::TimeDelta duration);
  bool HasPendingPageScaleAnimation() const;

  void SetDeviceScaleFactor(float device_scale_factor);
  float device_scale_factor() const { return device_scale_factor_; }

  void SetPaintedDeviceScaleFactor(float painted_device_scale_factor);
  float painted_device_scale_factor() const {
    return painted_device_scale_factor_;
  }

  void SetContentSourceId(uint32_t);
  uint32_t content_source_id() const { return content_source_id_; }

  void SetDeviceColorSpace(const gfx::ColorSpace& device_color_space);
  const gfx::ColorSpace& device_color_space() const {
    return device_color_space_;
  }

  // Used externally by blink for setting the PropertyTrees when
  // |settings_.use_layer_lists| is true. This is a SPV2 setting.
  PropertyTrees* property_trees() { return &property_trees_; }

  void SetNeedsDisplayOnAllLayers();

  void RegisterLayer(Layer* layer);
  void UnregisterLayer(Layer* layer);
  Layer* LayerById(int id) const;

  size_t NumLayers() const;

  bool in_update_property_trees() const { return in_update_property_trees_; }
  bool PaintContent(const LayerList& update_layer_list,
                    bool* content_is_suitable_for_gpu);
  bool in_paint_layer_contents() const { return in_paint_layer_contents_; }

  void AddLayerShouldPushProperties(Layer* layer);
  void RemoveLayerShouldPushProperties(Layer* layer);
  std::unordered_set<Layer*>& LayersThatShouldPushProperties();
  bool LayerNeedsPushPropertiesForTesting(Layer* layer) const;

  virtual void SetNeedsMetaInfoRecomputation(
      bool needs_meta_info_recomputation);
  bool needs_meta_info_recomputation() const {
    return needs_meta_info_recomputation_;
  }

  void SetPageScaleFromImplSide(float page_scale);
  void SetElasticOverscrollFromImplSide(gfx::Vector2dF elastic_overscroll);
  gfx::Vector2dF elastic_overscroll() const { return elastic_overscroll_; }

  void UpdateHudLayer(bool show_hud_info);
  HeadsUpDisplayLayer* hud_layer() const { return hud_layer_.get(); }

  virtual void SetNeedsFullTreeSync();
  bool needs_full_tree_sync() const { return needs_full_tree_sync_; }

  void SetPropertyTreesNeedRebuild();

  void PushPropertiesTo(LayerTreeImpl* tree_impl);

  MutatorHost* mutator_host() const { return mutator_host_; }

  Layer* LayerByElementId(ElementId element_id) const;
  void RegisterElement(ElementId element_id,
                       ElementListType list_type,
                       Layer* layer);
  void UnregisterElement(ElementId element_id,
                         ElementListType list_type,
                         Layer* layer);
  void SetElementIdsForTesting();

  void BuildPropertyTreesForTesting();

  // Layer iterators.
  LayerListIterator<Layer> begin() const;
  LayerListIterator<Layer> end() const;
  LayerListReverseIterator<Layer> rbegin();
  LayerListReverseIterator<Layer> rend();

  // LayerTreeHostInProcess interface to Proxy.
  void WillBeginMainFrame();
  void DidBeginMainFrame();
  void BeginMainFrame(const BeginFrameArgs& args);
  void BeginMainFrameNotExpectedSoon();
  void AnimateLayers(base::TimeTicks monotonic_frame_begin_time);
  void RequestMainFrameUpdate();
  void FinishCommitOnImplThread(LayerTreeHostImpl* host_impl);
  void WillCommit();
  void CommitComplete();
  void RequestNewCompositorFrameSink();
  void DidInitializeCompositorFrameSink();
  void DidFailToInitializeCompositorFrameSink();
  virtual std::unique_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* client);
  void DidLoseCompositorFrameSink();
  void DidCommitAndDrawFrame() { client_->DidCommitAndDrawFrame(); }
  void DidReceiveCompositorFrameAck() {
    client_->DidReceiveCompositorFrameAck();
  }
  bool UpdateLayers();
  // Called when the compositor completed page scale animation.
  void DidCompletePageScaleAnimation();
  void ApplyScrollAndScale(ScrollAndScaleSet* info);

  LayerTreeHostClient* client() { return client_; }

  bool gpu_rasterization_histogram_recorded() const {
    return gpu_rasterization_histogram_recorded_;
  }

  void CollectRenderingStats(RenderingStats* stats) const;

  RenderingStatsInstrumentation* rendering_stats_instrumentation() const {
    return rendering_stats_instrumentation_.get();
  }

  void SetAnimationEvents(std::unique_ptr<MutatorEvents> events);

  bool has_gpu_rasterization_trigger() const {
    return has_gpu_rasterization_trigger_;
  }

  Proxy* proxy() const { return proxy_.get(); }

  bool IsSingleThreaded() const;
  bool IsThreaded() const;

  // SurfaceReferenceOwner implementation.
  SurfaceSequenceGenerator* GetSurfaceSequenceGenerator() override;

  // MutatorHostClient implementation.
  bool IsElementInList(ElementId element_id,
                       ElementListType list_type) const override;
  void SetMutatorsNeedCommit() override;
  void SetMutatorsNeedRebuildPropertyTrees() override;
  void SetElementFilterMutated(ElementId element_id,
                               ElementListType list_type,
                               const FilterOperations& filters) override;
  void SetElementOpacityMutated(ElementId element_id,
                                ElementListType list_type,
                                float opacity) override;
  void SetElementTransformMutated(ElementId element_id,
                                  ElementListType list_type,
                                  const gfx::Transform& transform) override;
  void SetElementScrollOffsetMutated(
      ElementId element_id,
      ElementListType list_type,
      const gfx::ScrollOffset& scroll_offset) override;

  void ElementIsAnimatingChanged(ElementId element_id,
                                 ElementListType list_type,
                                 const PropertyAnimationState& mask,
                                 const PropertyAnimationState& state) override;

  void ScrollOffsetAnimationFinished() override {}
  gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const override;

 protected:
  LayerTreeHost(InitParams* params, CompositorMode mode);

  void InitializeThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  void InitializeSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  void InitializeForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider,
      std::unique_ptr<Proxy> proxy_for_testing);
  void SetTaskRunnerProviderForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider);
  void SetUIResourceManagerForTesting(
      std::unique_ptr<UIResourceManager> ui_resource_manager);

  // task_graph_runner() returns a valid value only until the LayerTreeHostImpl
  // is created in CreateLayerTreeHostImpl().
  TaskGraphRunner* task_graph_runner() const { return task_graph_runner_; }

  void OnCommitForSwapPromises();

  void RecordGpuRasterizationHistogram(const LayerTreeHostImpl* host_impl);

  MicroBenchmarkController micro_benchmark_controller_;

  base::WeakPtr<InputHandler> input_handler_weak_ptr_;

  scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner_;

 private:
  friend class LayerTreeHostSerializationTest;

  // This is the number of consecutive frames in which we want the content to be
  // suitable for GPU rasterization before re-enabling it.
  enum { kNumFramesToConsiderBeforeGpuRasterization = 60 };

  void ApplyViewportDeltas(ScrollAndScaleSet* info);
  void ApplyPageScaleDeltaFromImplSide(float page_scale_delta);
  void InitializeProxy(std::unique_ptr<Proxy> proxy);

  bool DoUpdateLayers(Layer* root_layer);
  void UpdateHudLayer();

  bool AnimateLayersRecursive(Layer* current, base::TimeTicks time);

  void CalculateLCDTextMetricsCallback(Layer* layer);

  const CompositorMode compositor_mode_;

  std::unique_ptr<UIResourceManager> ui_resource_manager_;

  LayerTreeHostClient* client_;
  std::unique_ptr<Proxy> proxy_;
  std::unique_ptr<TaskRunnerProvider> task_runner_provider_;

  int source_frame_number_ = 0U;
  std::unique_ptr<RenderingStatsInstrumentation>
      rendering_stats_instrumentation_;

  SwapPromiseManager swap_promise_manager_;

  // |current_compositor_frame_sink_| can't be updated until we've successfully
  // initialized a new CompositorFrameSink. |new_compositor_frame_sink_|
  // contains the new CompositorFrameSink that is currently being initialized.
  // If initialization is successful then |new_compositor_frame_sink_| replaces
  // |current_compositor_frame_sink_|.
  std::unique_ptr<CompositorFrameSink> new_compositor_frame_sink_;
  std::unique_ptr<CompositorFrameSink> current_compositor_frame_sink_;

  const LayerTreeSettings settings_;
  LayerTreeDebugState debug_state_;

  bool visible_ = false;

  bool has_gpu_rasterization_trigger_ = false;
  bool content_is_suitable_for_gpu_rasterization_ = true;
  bool gpu_rasterization_histogram_recorded_ = false;

  // If set, then page scale animation has completed, but the client hasn't been
  // notified about it yet.
  bool did_complete_scale_animation_ = false;

  int id_;
  bool next_commit_forces_redraw_ = false;
  bool next_commit_forces_recalculate_raster_scales_ = false;
  // Track when we're inside a main frame to see if compositor is being
  // destroyed midway which causes a crash. crbug.com/654672
  bool inside_main_frame_ = false;

  TaskGraphRunner* task_graph_runner_;

  SurfaceSequenceGenerator surface_sequence_generator_;
  uint32_t num_consecutive_frames_suitable_for_gpu_ = 0;

  scoped_refptr<Layer> root_layer_;

  scoped_refptr<Layer> overscroll_elasticity_layer_;
  scoped_refptr<Layer> page_scale_layer_;
  scoped_refptr<Layer> inner_viewport_scroll_layer_;
  scoped_refptr<Layer> outer_viewport_scroll_layer_;

  float top_controls_height_ = 0.f;
  float top_controls_shown_ratio_ = 0.f;
  bool browser_controls_shrink_blink_size_ = false;

  float bottom_controls_height_ = 0.f;

  float device_scale_factor_ = 1.f;
  float painted_device_scale_factor_ = 1.f;
  float page_scale_factor_ = 1.f;
  float min_page_scale_factor_ = 1.f;
  float max_page_scale_factor_ = 1.f;
  gfx::ColorSpace device_color_space_;

  uint32_t content_source_id_;

  SkColor background_color_ = SK_ColorWHITE;
  bool has_transparent_background_ = false;

  LayerSelection selection_;

  gfx::Size device_viewport_size_;

  bool have_scroll_event_handlers_ = false;
  EventListenerProperties event_listener_properties_[static_cast<size_t>(
      EventListenerClass::kNumClasses)];

  std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation_;

  PropertyTrees property_trees_;

  bool needs_full_tree_sync_ = true;
  bool needs_meta_info_recomputation_ = true;

  gfx::Vector2dF elastic_overscroll_;

  scoped_refptr<HeadsUpDisplayLayer> hud_layer_;

  // Set of layers that need to push properties.
  std::unordered_set<Layer*> layers_that_should_push_properties_;

  // Layer id to Layer map.
  std::unordered_map<int, Layer*> layer_id_map_;

  std::unordered_map<ElementId, Layer*, ElementIdHash> element_layers_map_;

  bool in_paint_layer_contents_ = false;
  bool in_update_property_trees_ = false;

  MutatorHost* mutator_host_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHost);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_H_