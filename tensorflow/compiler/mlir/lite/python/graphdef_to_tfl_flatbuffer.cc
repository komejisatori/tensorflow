/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/mlir/lite/python/graphdef_to_tfl_flatbuffer.h"

#include <ostream>
#include <utility>

#include "llvm/Support/ToolOutputFile.h"
#include "mlir/IR/MLIRContext.h"  // TF:llvm-project
#include "mlir/IR/Module.h"  // TF:llvm-project
#include "mlir/Pass/Pass.h"  // TF:llvm-project
#include "mlir/Support/FileUtilities.h"  // TF:llvm-project
#include "mlir/Transforms/ViewOpGraph.h"  // TF:llvm-project
#include "tensorflow/compiler/mlir/lite/common/tfl_pass_config.h"
#include "tensorflow/compiler/mlir/lite/python/tf_tfl_flatbuffer_helpers.h"
#include "tensorflow/compiler/mlir/lite/tf_tfl_passes.h"
#include "tensorflow/compiler/mlir/lite/tf_to_tfl_flatbuffer.h"
#include "tensorflow/compiler/mlir/lite/transforms/passes.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/import_model.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/mlir_roundtrip_flags.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/protobuf/graph_debug_info.pb.h"
#include "tensorflow/lite/toco/model_flags.pb.h"
#include "tensorflow/lite/toco/toco_flags.pb.h"
#include "tensorflow/lite/toco/types.pb.h"
#include "tensorflow/stream_executor/lib/statusor.h"

namespace tensorflow {
Status ConvertGraphDefToTFLiteFlatBuffer(const toco::ModelFlags& model_flags,
                                         const toco::TocoFlags& toco_flags,
                                         const GraphDebugInfo& debug_info,
                                         const GraphDef& input,
                                         string* result) {
  mlir::MLIRContext context;
  GraphImportConfig specs;
  mlir::TFL::QuantizationSpecs quant_specs;

  // Parse input arrays.
  std::vector<string> node_names;
  std::vector<string> node_dtypes;
  std::vector<std::vector<int>> node_shapes;
  std::vector<double> node_mins;
  std::vector<double> node_maxs;

  // Populate quantization specs.
  TF_RETURN_IF_ERROR(internal::PopulateQuantizationSpecs(
      model_flags, toco_flags, &quant_specs, &node_names, &node_dtypes,
      &node_shapes, &node_mins, &node_maxs));

  TF_RETURN_IF_ERROR(tensorflow::ParseInputArrayInfo(
      node_names, node_dtypes, node_shapes, &specs.inputs));

  // Parse output arrays.
  std::vector<string> output_arrays(model_flags.output_arrays().begin(),
                                    model_flags.output_arrays().end());
  TF_RETURN_IF_ERROR(
      tensorflow::ParseOutputArrayInfo(output_arrays, &specs.outputs));

  specs.prune_unused_nodes = true;
  specs.convert_legacy_fed_inputs = true;
  specs.graph_as_function = false;
  specs.upgrade_legacy = true;
  internal::WarningUnusedFlags(model_flags, toco_flags);

  // Register all custom ops, including user-specified custom ops.
  TF_RETURN_IF_ERROR(internal::RegisterAllCustomOps(toco_flags));

  TF_ASSIGN_OR_RETURN(
      auto module, ConvertGraphdefToMlir(input, debug_info, specs, &context));

  return internal::ConvertMLIRToTFLiteFlatBuffer(toco_flags, std::move(module),
                                                 quant_specs, result);
}

}  // namespace tensorflow
