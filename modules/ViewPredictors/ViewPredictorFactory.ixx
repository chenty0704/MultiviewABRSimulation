module;

#include <System/Macros.h>

export module MultiviewABRSimulation.ViewPredictors.ViewPredictorFactory;

import System.Base;

import MultiviewABRSimulation.ViewPredictors.IViewPredictor;
import MultiviewABRSimulation.ViewPredictors.MarkovPredictor;
import MultiviewABRSimulation.ViewPredictors.StaticPredictor;

using namespace std;

#define TRY_CREATE(T) \
    if (const auto *const _options = dynamic_cast<const CONCAT(T, Options) *>(&options)) \
        return make_unique<T>(config, *_options);

/// Represents a factory for view predictors.
export class ViewPredictorFactory {
public:
    /// Creates a view predictor with the specified configuration and options.
    /// @param config The configuration for the view predictor.
    /// @param options The options for the view predictor.
    /// @returns A view predictor with the specified configuration and options.
    [[nodiscard]] static unique_ptr<IViewPredictor> Create(const ViewPredictorConfig &config,
                                                           const BaseViewPredictorOptions &options) {
        FOR_EACH(TRY_CREATE, (
                     MarkovPredictor,
                     StaticPredictor
                 ))
        throw runtime_error("Unknown view predictor options.");
    }
};
