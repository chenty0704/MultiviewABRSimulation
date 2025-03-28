module;

#include <System/Macros.h>

export module MultiviewABRSimulation.MultiviewABRControllers.MultiviewABRControllerFactory;

import System.Base;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;
import MultiviewABRSimulation.MultiviewABRControllers.ModelPredictiveController;
import MultiviewABRSimulation.MultiviewABRControllers.ThroughputBasedController;

using namespace std;

#define TRY_CREATE(T) \
    if (const auto *const _options = dynamic_cast<const CONCAT(T, Options) *>(&options)) \
        return make_unique<T>(streamingConfig, *_options);

/// Represents a factory for multiview adaptive bitrate controllers.
export class MultiviewABRControllerFactory {
public:
    /// Creates a multiview adaptive bitrate controller with the specified configuration and options.
    /// @param streamingConfig The adaptive bitrate streaming configuration.
    /// @param options The options for the multiview adaptive bitrate controller.
    /// @returns A multiview adaptive bitrate controller with the specified configuration and options.
    [[nodiscard]] static unique_ptr<IMultiviewABRController> Create(const StreamingConfig &streamingConfig,
                                                                    const BaseMultiviewABRControllerOptions &options) {
        FOR_EACH(TRY_CREATE, (
                     ModelPredictiveController,
                     ThroughputBasedController
                 ))
        throw runtime_error("Unknown multiview adaptive bitrate controller options.");
    }
};
