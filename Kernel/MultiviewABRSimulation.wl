BeginPackage["MultiviewABRSimulation`", {"LibraryLinkUtilities`", "Utilities`"}];

MultiviewABRSimulate::usage = UsageString@"MultiviewABRSimulate[`streamingConfig`, `controller`, {`networkData`, `primaryStreamData`}] simulates a multiview adaptive bitrate streaming configuration on a collection of network series and primary stream series.";

Begin["`Private`"];

$ContextAliases["LLU`"] = "LibraryLinkUtilities`Private`LLU`";

LLU`InitializePacletLibrary["MultiviewABRSimulationLink"];

LLU`PacletFunctionSet[$MultiviewABRSimulate, {"Object", "TypedOptions",
    LibraryDataType[TemporalData, Real], LibraryDataType[TemporalData, Integer],
    "TypedOptions", "TypedOptions"}, "DataStore"];

Options[MultiviewABRSimulate] = {
    "ThroughputPredictor" -> "EMAPredictor",
    "ViewPredictor" -> "StaticPredictor"
};

MultiviewABRSimulate[streamingConfig_Association, controller : _String | _List,
    {networkData_TemporalData, primaryStreamData_TemporalData}, options : OptionsPattern[]] :=
        Association @@ $MultiviewABRSimulate[streamingConfig, controller, networkData, primaryStreamData - 1,
            OptionValue["ThroughputPredictor"], OptionValue["ViewPredictor"]];

End[];

EndPackage[];
