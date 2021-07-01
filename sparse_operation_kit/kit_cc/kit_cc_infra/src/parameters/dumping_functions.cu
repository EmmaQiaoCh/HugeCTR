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

#include "parameters/dumping_functions.h"
#include "embeddings/forward_functions.cuh"

namespace SparseOperationKit {

void get_hash_value(size_t count, size_t embedding_vec_size, const size_t *value_index,
                const float *embedding_table, float *value_retrieved,
                cudaStream_t stream) {
const size_t block_size = embedding_vec_size;
const size_t grid_size = count;

HugeCTR::get_hash_value_kernel<<<grid_size, block_size, 0, stream>>>(count, embedding_vec_size,
                                                value_index, embedding_table, value_retrieved);
}


void indexes_add_offset(size_t *indexes, const size_t count, const size_t offset) {
    const size_t block_size = 1024;
    const size_t grid_size = (count + block_size - 1) / block_size;
    auto helper = [offset] __device__ (size_t index) -> size_t { return index + offset; };
    transform_array<<<grid_size, block_size>>>(indexes, indexes, count, helper);
}

} // namespace SparseOperationKit