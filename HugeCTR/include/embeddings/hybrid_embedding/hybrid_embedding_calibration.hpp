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

#include "HugeCTR/include/tensor2.hpp"
#include <vector>

namespace HugeCTR {


///
/// This class contains the calibrated measurements for all-to-all and all-reduce
/// for different data sizes. Each calibration consists of two arrays, 
/// ._data_size array and the ._time array which represent a mapping. 
///
/// This class will be executed on the cpu instead of the gpu if no
/// gpu memory is allocated for the calibration data.
struct CalibrationData {
  CalibrationData() {}
  ~CalibrationData() {}

  // Calibration all-to-all : 
  //   the following two arrays map data sizes to all-to-all times / latencies.
  std::vector<double> h_all_to_all_data_size;
  std::vector<double> h_all_to_all_times;
  Tensor2<float> all_to_all_data_size; // data size of message per gpu
  Tensor2<float> all_to_all_times;     // calibrated all-to-all times

  // Calibration all-reduce : 
  //   the following two arrays map data sizes to all-to-all times / latencies.
  std::vector<double> h_all_reduce_data_size;
  std::vector<double> h_all_reduce_times;
  Tensor2<float> all_reduce_data_size; // data size of message per gpu
  Tensor2<float> all_reduce_times;     // calibrated all-reduce times

  // Alternative calibration: (if no calibration provided)
  //   the threshold for frequent categories is calculated from maximum bandwidths
  //   for the all-reduce and all-to-all respectively. 
  //   This approximation assumes that the communications are bandwidth limited.
  double max_all_reduce_bandwidth; // algorithm bandwidth all-reduce [data size message per gpu in bytes / sec]
  double max_all_to_all_bandwidth; // algorithm bandwidth all-to-all [data size message per gpu in bytes / sec]

  // cpu functions
  double interpolate(
    const std::vector<double> &calibrated_data_size,
    const std::vector<double> &calibrated_times,
    const std::vector<double> &data_size,
    std::vector<double> &communication_times);
  double interpolate_all_reduce(
    const std::vector<double> &data_size,
    std::vector<double> &communication_times);
  double interpolate_all_to_all(
    const std::vector<double> &data_size,
    std::vector<double> &communication_times);

  // gpu functions
  float interpolate(
    const Tensor2<float> &calibrated_data_size,
    const Tensor2<float> &calibrated_times,
    const Tensor2<float> &data_size,
    Tensor2<float> &communication_times);
  float interpolate_all_reduce(
    const Tensor2<float> &data_size,
    Tensor2<float> &communication_times);
  float interpolate_all_to_all(
    const Tensor2<float> &data_size,
    Tensor2<float> &communication_times);
};


template <typename dtype>
class ModelInitializationFunctors {
 public:
  double calculate_threshold(
    CommunicationType communication_type,
    size_t batch_size, 
    size_t num_networks,
    size_t num_iterations,
    size_t num_tables);
  uint32_t calculate_num_frequent_categories(
    const CommunicationType &communication_type,
    const CalibrationData<dtype> &calibration,
    const HybridEmbeddingStatistics<dtype> &statistics,
    const HybridEmbeddingData<dtype> &data,
    cudaStream_t stream);
};


}