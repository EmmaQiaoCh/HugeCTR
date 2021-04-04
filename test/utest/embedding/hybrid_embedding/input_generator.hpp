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

#include <random>
#include <vector>

namespace HugeCTR {

namespace hybrid_embedding {

template <typename dtype>
struct HybridEmbeddingConfig {
  size_t num_nodes;
  size_t num_instances;
  size_t num_tables;
  size_t embedding_vec_size;
  dtype num_categories;
  dtype num_frequent;
  float lr;
};

template <typename dtype>
class HybridEmbeddingInputGenerator {
 public:
  HybridEmbeddingInputGenerator(HybridEmbeddingConfig<dtype> config, size_t seed);
  HybridEmbeddingInputGenerator(HybridEmbeddingConfig<dtype> config,
                                const std::vector<size_t> &table_sizes, size_t seed);
  // Multiple calls return different data

  // _per_feature means that the data is in the 'raw' format: categories are indexed
  // according to the tables they belong to
  std::vector<dtype> generate_categorical_input_per_feature(size_t batch_size);

  // _flattened means that the category indices are unique
  // (i.e., table offsets are added to the raw data)
  std::vector<dtype> generate_flattened_categorical_input(size_t batch_size);

  void generate_categorical_input_per_feature(dtype *batch, size_t batch_size);
  void generate_flattened_categorical_input(dtype *batch, size_t batch_size);
  void generate_category_location();

  // Multiple calls return the same data
  std::vector<dtype> &get_category_location();
  std::vector<dtype> &get_category_frequent_index();
  std::vector<size_t> &get_table_sizes();

 private:
  HybridEmbeddingConfig<dtype> config_;
  std::vector<std::vector<double>> embedding_prob_distribution_;
  std::vector<size_t> table_sizes_;
  size_t seed_;
  std::mt19937 gen_;

  std::vector<dtype> category_location_, category_frequent_index_;
  std::vector<std::vector<size_t>> embedding_shuffle_args;

  void generate_uniform_rand_table_sizes();
  void create_probability_distribution();
  void generate_categories(dtype *data, size_t batch_size, bool normalized);
};

}

}
