/*
 * Copyright (c) 2021, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cuda_runtime.h>

#include <vector>

#include "HugeCTR/include/common.hpp"
#include "HugeCTR/include/embeddings/hybrid_embedding/calibration_data.hpp"
#include "HugeCTR/include/embeddings/hybrid_embedding/data.hpp"
#include "HugeCTR/include/embeddings/hybrid_embedding/utils.hpp"
#include "HugeCTR/include/tensor2.hpp"

namespace HugeCTR {

namespace hybrid_embedding {

///
/// This class defines the hybrid embedding model:
///    which categories are frequent, which are infrequent
///    where are the corresponding embedding vectors stored.
///
/// Also the mlp network - nodes topology is defined here:
///    The node_id, instance_id where the current model instance is
///    associated with is stored. However, keep in mind that these are the only
///    differentiating variables inside this class that differ from other
///    instances. As this model describes the same distribution across the nodes
///    and gpu's (networks).
///
template <typename dtype>
struct Model {
 public:
  Model() {}
  ~Model() {}

  uint32_t node_id;
  uint32_t instance_id;
  uint32_t global_instance_id;

  CommunicationType communication_type;

  dtype num_frequent;
  dtype num_categories;

  uint32_t num_instances;
  std::vector<uint32_t> h_num_instances_per_node;
  Tensor2<uint32_t>
      num_instances_per_node;  // number of gpus for each node, .size() == number of nodes

  Tensor2<dtype> category_frequent_index;  // indicator frequent category => location in cache
  Tensor2<dtype> category_location;  // indicator infrequent category => location embedding vector

  void init_model(const CommunicationType communication_type, const CalibrationData &calibration,
                  const Data<dtype> &data, cudaStream_t stream);
};

}  // namespace hybrid_embedding

}  // namespace HugeCTR