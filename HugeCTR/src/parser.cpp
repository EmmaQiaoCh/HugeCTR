/*
 * Copyright (c) 2020, NVIDIA CORPORATION.
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

#include <data_readers/data_reader.hpp>
#include <embeddings/distributed_slot_sparse_embedding_hash.hpp>
#include <embeddings/localized_slot_sparse_embedding_hash.hpp>
#include <embeddings/localized_slot_sparse_embedding_one_hot.hpp>
#include <layer.hpp>
#include <layers/add_layer.hpp>
#include <layers/batch_norm_layer.hpp>
#include <layers/cast_layer.hpp>
#include <layers/concat_layer.hpp>
#include <layers/dot_product_layer.hpp>
#include <layers/dropout_cudnn_layer.hpp>
#include <layers/dropout_layer.hpp>
#include <layers/elu_layer.hpp>
#include <layers/fm_order2_layer.hpp>
#include <layers/fully_connected_layer.hpp>
#include <layers/fully_connected_layer_half.hpp>
#include <layers/fused_fully_connected_layer.hpp>
#include <layers/interaction_layer.hpp>
#include <layers/multi_cross_layer.hpp>
#include <layers/multiply_layer.hpp>
#include <layers/reduce_sum_layer.hpp>
#include <layers/relu_layer.hpp>
#include <layers/reshape_layer.hpp>
#include <layers/sigmoid_layer.hpp>
#include <layers/slice_layer.hpp>
#include <loss.hpp>
#include <metrics.hpp>
#include <optimizer.hpp>
#include <parser.hpp>
#include <regularizers/l1_regularizer.hpp>
#include <regularizers/l2_regularizer.hpp>
#include <regularizers/no_regularizer.hpp>

#ifdef ENABLE_MPI
#include <mpi.h>
#endif

namespace HugeCTR {
Parser::Parser(const std::string& configure_file, size_t batch_size, size_t batch_size_eval,
               bool repeat_dataset, bool i64_input_key, bool use_mixed_precision, float scaler,
               bool use_algorithm_search, bool use_cuda_graph)
    : batch_size_(batch_size),
      batch_size_eval_(batch_size_eval),
      repeat_dataset_(repeat_dataset),
      i64_input_key_(i64_input_key),
      use_mixed_precision_(use_mixed_precision),
      scaler_(scaler),
      use_algorithm_search_(use_algorithm_search),
      use_cuda_graph_(use_cuda_graph) {
  try {
    std::ifstream file(configure_file);
    if (!file.is_open()) {
      CK_THROW_(Error_t::FileCannotOpen, "file.is_open() failed: " + configure_file);
    }
    file >> config_;
    file.close();
  } catch (const std::runtime_error& rt_err) {
    std::cerr << rt_err.what() << std::endl;
    throw;
  }
  return;
}

template <typename TypeKey>
void Parser::create_pipeline_internal(std::shared_ptr<IDataReader>& data_reader,
                                      std::shared_ptr<IDataReader>& data_reader_eval,
                                      std::vector<std::shared_ptr<IEmbedding>>& embedding,
                                      std::vector<std::unique_ptr<Network>>& network,
                                      const std::shared_ptr<ResourceManager>& resource_manager) {
  try {
    nlohmann::json config = config_;
    size_t batch_size = batch_size_;
    size_t batch_size_eval = batch_size_eval_;
    bool use_mixed_precision = use_mixed_precision_;
    float scaler = scaler_;
    bool use_algorithm_search = use_algorithm_search_;
    bool use_cuda_graph = use_cuda_graph_;

    std::map<std::string, SparseInput<TypeKey>> sparse_input_map;
    std::vector<TensorEntry> tensor_entries_list[resource_manager->get_local_gpu_count()];
    {
      if (!network.empty()) {
        CK_THROW_(Error_t::WrongInput, "vector network is not empty");
      }

      auto j_layers_array = get_json(config, "layers");
      auto j_optimizer = get_json(config, "optimizer");

      // Create Data Reader
      {
        const nlohmann::json& j = j_layers_array[0];
        create_datareader<TypeKey>()(j, sparse_input_map, tensor_entries_list, data_reader, data_reader_eval, batch_size, batch_size_,use_mixed_precision, repeat_dataset_, resource_manager);
      }

      // Create Embedding
      {
        for (unsigned int i = 1; i < j_layers_array.size(); i++) {
          // if not embedding then break
          const nlohmann::json& j = j_layers_array[i];
          auto embedding_name = get_value_from_json<std::string>(j, "type");
          Embedding_t embedding_type;
          if (!find_item_in_map(embedding_type, embedding_name, EMBEDDING_TYPE_MAP)) {
            Layer_t layer_type;
            if (!find_item_in_map(layer_type, embedding_name, LAYER_TYPE_MAP) &&
                !find_item_in_map(layer_type, embedding_name, LAYER_TYPE_MAP_MP)) {
              CK_THROW_(Error_t::WrongInput, "No such layer: " + embedding_name);
            }
            break;
          }

          if (use_mixed_precision) {
            create_embedding<TypeKey, __half>()(
                sparse_input_map, tensor_entries_list, embedding, embedding_type, config,
                resource_manager, batch_size, batch_size_eval, use_mixed_precision, scaler, j);
          } else {
            create_embedding<TypeKey, float>()(sparse_input_map, tensor_entries_list, embedding,
                                               embedding_type, config, resource_manager, batch_size,
                                               batch_size_eval, use_mixed_precision, scaler, j);
          }
        }  // for ()
      }    // Create Embedding

      // create network
      int total_gpu_count = resource_manager->get_global_gpu_count();
      if (0 != batch_size % total_gpu_count) {
        CK_THROW_(Error_t::WrongInput, "0 != batch_size\%total_gpu_count");
      }
      for (size_t i = 0; i < resource_manager->get_local_gpu_count(); i++) {
        network.emplace_back(Network::create_network(
            j_layers_array, j_optimizer, tensor_entries_list[i], total_gpu_count,
            resource_manager->get_local_cpu(), resource_manager->get_local_gpu(i),
            use_mixed_precision, scaler, use_algorithm_search, use_cuda_graph, false));
      }
    }

  } catch (const std::runtime_error& rt_err) {
    std::cerr << rt_err.what() << std::endl;
    throw;
  }
}

void Parser::create_pipeline(std::shared_ptr<IDataReader>& data_reader,
                             std::shared_ptr<IDataReader>& data_reader_eval,
                             std::vector<std::shared_ptr<IEmbedding>>& embedding,
                             std::vector<std::unique_ptr<Network>>& network,
                             const std::shared_ptr<ResourceManager>& resource_manager) {
  if (i64_input_key_) {
    create_pipeline_internal<long long>(data_reader, data_reader_eval, embedding, network,
                                        resource_manager);
  } else {
    create_pipeline_internal<unsigned int>(data_reader, data_reader_eval, embedding, network,
                                           resource_manager);
  }
}

}  // namespace HugeCTR
