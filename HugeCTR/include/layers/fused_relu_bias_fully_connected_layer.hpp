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

#pragma once

#include <cublas_v2.h>
#include <cublasLt.h>
#include <functional>
#include <layer.hpp>
#include <vector>

namespace HugeCTR {

typedef enum 
{
    HEAD=0,
    BODY,
    TAIL,
    ISOLATED
} Position;

/**
 * @brief
 * This class implements the fully connected layer.
 */
class FusedReluBiasFullyConnectedLayer : public Layer {
  // Optimized cublasGemmEx algorithm selection
  cublasLtMatmulAlgo_t falgo_k_;
  cublasGemmAlgo_t balgo_k_{CUBLAS_GEMM_DEFAULT};
  cublasGemmAlgo_t balgo_x_{CUBLAS_GEMM_DEFAULT};

  cublasLtMatrixLayout_t cublas_kernel_desc_ = NULL;
  cublasLtMatrixLayout_t cublas_top_desc_ = NULL;
  cublasLtMatrixLayout_t cublas_bottom_desc_ = NULL;
  cublasLtMatmulDesc_t cublas_op_desc_ = NULL;

  cublasLtMatmulPreference_t cublas_preference_ = NULL;
  size_t cublaslt_workspace_size_ = 1024*1024*8;
  void* cublaslt_workspace_;

  /*
   * stores the weight tensors for compute of this layer.
   */
  // std::vector<TensorPtr<float>> master_weights_; It is inherited from Layer, and named as
  // weights_;

  /*
   * stores the weight tensors for compute of this layer.
   */
  // std::vector<TensorPtr<__half>> weights_;
  Tensors2<__half> weights_half_;

  /*
   * stores the weight gradient tensors of this layer.
   */
  Tensors2<__half> weights_grad_;

  /*
   * stores the references to the bottom tensors of this layer.
   */
  Tensor2<__half> train_bottom_tensor_fprop_;
  Tensor2<__half> train_bottom_tensor_bprop_;

  /*
   * stores the references to the top tensors of this layer.
   */
  Tensor2<__half> top_tensor_fprop_;
  Tensor2<__half> top_tensor_bprop_;

  /*
   * stores the references to the intermediate bias grad tensors of this layer.
   */
  Tensor2<float> bias_grad_tensor_;

  /*
   * stores the position of this layer in the network
   */
  Position pos_;

  std::unique_ptr<DataSimulator> get_uniform_initializer(const int index) override;
  std::unique_ptr<DataSimulator> get_xavier_uniform_initializer(const int index) override;
  std::unique_ptr<DataSimulator> get_xavier_norm_initializer(const int index) override;
  std::unique_ptr<DataSimulator> get_default_initializer(const int index) override;

  Tensor2<__half>& get_bottom_tensor_fprop(bool is_train) {
    if (is_train) {
      return train_bottom_tensor_fprop_;
    }
  }

  Tensor2<__half>& get_bottom_tensor_bprop(bool is_train) {
    if (is_train) {
      return train_bottom_tensor_bprop_;
    }
  }

 public:
  /**
   * forward pass
   */
  void fprop(bool is_train) final;
  /**
   * backward pass
   */
  void bprop() final;
  /*
   * algorithm search for cublasGemmEx
   */
  void search_algorithm() final;
  void initialize() final;

  /**
   * This is the constructor of the FullyConnectedLayer.
   * It will check whether the format combination of all tensors is supported or not.
   * Only two kinds of tensor formats are supported:
   * (1) weight, input, output, wgrad are all in row-major.
   * (2) weight, input, output, wgrad are all in column-major.
   * @param weight_buff: stores the weight tensor
   * @param wgrad_buff: stores the gradient values of the weight calculated in backward pass
   * @param train_bottom_tensor_fprop: stores the tensor from bottom layer for forward propogation
   * @param train_bottom_tensor_fprop: stores the tensor from bottom layer for forward propogation
   * @param top_tensor_fprop: stores the tensor to top layer when forward propogation
   * @param top_tensor_bprop: stores the tensor to top layer when backward propogation
   * @param pos: stores the position of this layer: HEAD, BODY, TAIL, ISOLATED.
   */
  FusedReluBiasFullyConnectedLayer(
      const std::shared_ptr<BufferBlock2<float>>& master_weights_buff,
      const std::shared_ptr<BufferBlock2<__half>>& weights_buff,
      const std::shared_ptr<BufferBlock2<__half>>& weights_grad_buff,
      const std::shared_ptr<GeneralBuffer2<CudaAllocator>>& blobs_buff,
      const Tensor2<__half>& train_bottom_tensor_fprop,
      const Tensor2<__half>& train_bottom_tensor_bprop,
      const Tensor2<__half>& top_tensor_fprop, 
      const Tensor2<__half>& top_tensor_bprop, 
      const std::shared_ptr<GPUResource>& gpu_resource,
      const std::string& pos,
      std::vector<Initializer_t> initializer_types = std::vector<Initializer_t>());
  FusedReluBiasFullyConnectedLayer(const FusedReluBiasFullyConnectedLayer&) = delete;
  FusedReluBiasFullyConnectedLayer& operator=(const FusedReluBiasFullyConnectedLayer&);
};
}  // namespace HugeCTR
