module;

#include <System/Macros.h>

export module MultiviewABRSimulation.MultiviewABRControllers.MultiviewABRControllerFactory;

import System.Base;

import MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;
import MultiviewABRSimulation.MultiviewABRControllers.ThroughputBasedController;

using namespace std;

#define TRY_CREATE(T) \
    if (const auto *const _options = dynamic_cast<const CONCAT(T, Options) *>(&options)) \
        return make_unique<T>(config, *_options);

/// Represents a factory for multiview adaptive bitrate controllers.
export class MultiviewABRControllerFactory {
public:
    /// Creates a multiview adaptive bitrate controller with the specified configuration and options.
    /// @param config The configuration for the multiview adaptive bitrate controller.
    /// @param options The options for the multiview adaptive bitrate controller.
    /// @returns A multiview adaptive bitrate controller with the specified configuration and options.
    [[nodiscard]] static unique_ptr<IMultiviewABRController> Create(const MultiviewABRControllerConfig &config,
                                                                    const BaseMultiviewABRControllerOptions &options) {
        FOR_EACH(TRY_CREATE, (
                     ThroughputBasedController
                 ))
        throw runtime_error("Unknown multiview adaptive bitrate controller options.");
    }
};
