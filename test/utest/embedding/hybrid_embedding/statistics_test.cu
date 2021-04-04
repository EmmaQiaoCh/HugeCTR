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

#include <algorithm>
#include <cmath>
#include <cstdlib>
// #include <gmock/gmock.h>
// #include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <numeric>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"
#include "HugeCTR/include/common.hpp"
#include "HugeCTR/include/embeddings/hybrid_embedding/data.hpp"
#include "HugeCTR/include/embeddings/hybrid_embedding/utils.hpp"
#include "HugeCTR/include/embeddings/hybrid_embedding/statistics.hpp"
#include "HugeCTR/include/tensor2.hpp"
#include "utest/embedding/hybrid_embedding/input_generator.hpp"
#include "test/utest/embedding/hybrid_embedding/statistics_test.hpp"

using namespace HugeCTR;
using namespace HugeCTR::hybrid_embedding;

namespace HugeCTR {

namespace hybrid_embedding {

template <typename dtype>
void arg_sort(const std::vector<dtype> &v, std::vector<size_t> &arg) {
  arg.resize(v.size());
  std::iota(arg.begin(), arg.end(), (size_t) 0);
  std::stable_sort(arg.begin(), arg.end(), [&v](size_t i1, size_t i2) { return v[i1] > v[i2]; });
}

void generate_table_sizes(std::vector<size_t> &table_sizes, 
                          const size_t num_tables, const size_t vec_size, 
                          const size_t max_mem) {
  table_sizes.resize(num_tables);

  // mem = sizeof(float) * vec_size * num_tables * max_table_size;
  // => 
  const size_t max_table_size = max_mem / (sizeof(float) * vec_size * num_tables);
  const double max_exp = log(max_table_size) / log(10.);

  for (size_t embedding = 0; embedding < num_tables; ++embedding) {
    double r = rand() / (double) RAND_MAX;
    table_sizes[embedding] = std::max((size_t)2, (size_t) floor(pow(10., 1.+r*(max_exp-1))));
  }
}

template <typename dtype>
void generate_reference_stats(const std::vector<dtype> &data, 
                              std::vector<dtype> &samples,
                              std::vector<size_t> &categories_stats,
                              std::vector<size_t> &counts_stats,
                              const std::vector<size_t> &table_sizes,
                              const size_t batch_size) {
  const size_t num_embeddings = table_sizes.size();

  std::vector<dtype> embedding_offsets;
  EmbeddingTableFunctors<dtype>::get_embedding_offsets(embedding_offsets, table_sizes);

  for (size_t embedding = 0; embedding < num_embeddings; ++embedding) {
    std::cout << "embedding " << embedding << " offset : " << embedding_offsets[embedding] << std::endl;
  }

  samples.resize(data.size());
  for (size_t sample = 0; sample < batch_size; ++sample) {
    for (size_t embedding = 0; embedding < num_embeddings; ++embedding) {
      size_t indx = sample*num_embeddings + embedding;
      samples[indx] = embedding_offsets[embedding] + data[indx];
    }
  }

  // create statistics
  std::set<dtype> category_set(samples.begin(), samples.end());
  const size_t num_unique_categories = category_set.size();

  std::cout << "number of unique categories : " << num_unique_categories << std::endl;

  // helper structures
  std::map<dtype, size_t> category_index;
  std::vector<dtype> categories(num_unique_categories);
  size_t indx = (size_t)0;
  for (const auto &category : category_set) {
    category_index[category] = indx;
    categories[indx] = category;
    indx++;
  }

  std::vector<size_t> counts(num_unique_categories, (size_t)0);
  for (size_t sample = 0; sample < samples.size(); ++sample) {
    size_t indx = category_index[samples[sample]];
    counts[indx]++;
  }

  // sort categories and counts by argument
  std::vector<size_t> arg;
  arg_sort(counts, arg);
  categories_stats.resize(num_unique_categories);
  counts_stats.resize(num_unique_categories);
  for (indx = 0; indx < num_unique_categories; ++indx) {
    categories_stats[indx] = categories[arg[indx]];
    counts_stats[indx] = counts[arg[indx]];

    // check order counts
    if (indx > 0 && counts_stats[indx] > counts_stats[indx-1]) 
      std::cout << "incorrect counts order!" << std::endl;
  }

  std::cout << "done generating reference stats and data!" << std::endl;
}


}

}

template <typename dtype>
void statistics_test(const size_t batch_size, const size_t num_tables) {
  // 1. generate reference samples and stats
  cudaStream_t stream = 0;

  std::vector<size_t> categories;
  std::vector<size_t> counts;

  std::vector<size_t> table_sizes;
  HugeCTR::hybrid_embedding::generate_table_sizes(table_sizes, num_tables);
  std::cout << "Number of tables : " << num_tables << std::endl;
  std::cout << "Table sizes : ";
  for (size_t embedding = 0; embedding < table_sizes.size(); ++embedding)
    std::cout << '\t' << table_sizes[embedding];
  std::cout << std::endl;

  size_t num_categories = EmbeddingTableFunctors<dtype>::get_num_categories(table_sizes);

  HugeCTR::hybrid_embedding::HybridEmbeddingConfig<dtype> config = {
    1,              // num_nodes
    1,              // num_instances
    num_tables,     // num_tables
    128,            // embedding_vec_size
    (uint32_t) num_categories, // num_categories
    (dtype) 0,      // num_frequent, not used for generator
    (float) 1       // lr, not used
  };
  HugeCTR::hybrid_embedding::HybridEmbeddingInputGenerator<dtype> 
    input_generator(config, table_sizes, 848484);

  std::vector<dtype> raw_data(batch_size*num_tables);
  input_generator.generate_categorical_input_per_feature(raw_data.data(), batch_size);

  std::vector<dtype> samples_ref;
  HugeCTR::hybrid_embedding::generate_reference_stats<dtype>(
      raw_data, samples_ref, categories, counts, table_sizes, batch_size);

  std::cout << "raw data: " << std::endl;
  for (size_t sample = 0; sample < batch_size; ++sample) {
    std::cout << sample  << " : ";
    for (size_t embedding = 0; embedding < table_sizes.size(); ++embedding) {
      std::cout << raw_data[sample*table_sizes.size() + embedding] << '\t';
    }
    std::cout << std::endl;
  }

  std::cout << std::endl;
  std::cout << "samples : " << std::endl;
  for (size_t sample = 0; sample < batch_size; ++sample) {
    std::cout << sample  << " : ";
    for (size_t embedding = 0; embedding < table_sizes.size(); ++embedding) {
      std::cout << samples_ref[sample*table_sizes.size() + embedding] << '\t';
    }
    std::cout << std::endl;
  }

  size_t tot_count = 0;
  for (size_t c = 0; c < categories.size(); ++c) {
    tot_count += counts[c];
  }
  std::cout << "total counts reference = " << tot_count << std::endl;

  // create the gpu tensor for the raw data
  std::cout << "placing raw data on gpu..." << std::endl;
  Tensor2<dtype> d_raw_data;
  std::shared_ptr<GeneralBuffer2<CudaAllocator>> buf = GeneralBuffer2<CudaAllocator>::create();
  std::cout << "number of samples  : " << raw_data.size() << std::endl;
  std::cout << "number of elements : " << batch_size * num_tables << std::endl;
  buf->reserve({raw_data.size(), 1}, &d_raw_data);
  buf->allocate();
  upload_tensor(raw_data, d_raw_data, stream);

  // 2. perform hybrid_embedding statistics on gpu
  Data<dtype> data(table_sizes, batch_size, 1);
  data.data_to_unique_categories(d_raw_data, stream);
  HugeCTR::hybrid_embedding::Statistics<dtype> statistics(data);
  statistics.sort_categories_by_count(data.samples, stream);
  CK_CUDA_THROW_(cudaStreamSynchronize(stream));

  std::cout << "number of unique categories statistics = " << statistics.num_unique_categories << std::endl;

  // check that the samples are the same..
  std::vector<dtype> h_samples(samples_ref.size());
  download_tensor(h_samples, data.samples, stream);
  for (size_t sample = 0; sample < samples_ref.size(); ++sample) {
    EXPECT_EQ(h_samples[sample], samples_ref[sample]);
  }

  // 3. check that hybrid_embedding calculated stats == ref stats
  std::vector<dtype> h_categories_sorted;
  std::vector<uint32_t> h_counts_sorted;
  download_tensor(h_categories_sorted, statistics.categories_sorted, stream);
  download_tensor(h_counts_sorted, statistics.counts_sorted, stream);

  tot_count = 0;
  for (size_t c = 0; c < categories.size(); ++c) {
    tot_count += h_counts_sorted[c];
  }
  std::cout << "total counts stats = " << tot_count << std::endl;

  for (size_t c = 0; c < h_categories_sorted.size(); ++c) {
    std::cout << "categories : " << h_categories_sorted[c] 
              << '\t' << categories[c]
              << "\t counts : " << h_counts_sorted[c] 
              << '\t' << counts[c] << std::endl;
  }
  for (size_t c = 0; c < categories.size(); ++c) {
    EXPECT_EQ(h_categories_sorted[c], categories[c]);
    EXPECT_EQ(h_counts_sorted[c], counts[c]);
  }

  const size_t num_categories_sorted_test = statistics.num_unique_categories;
  if (num_categories_sorted_test != categories.size()) {
    std::cout << "Number of categories_sorted is NOT the same as the reference!" << std::endl;
  } else {
    std::cout << "Number of categories_sorted is the same as the reference!" << std::endl;
  }
  EXPECT_EQ(num_categories_sorted_test, categories.size());
  std::unordered_set<dtype> category_set_test(h_categories_sorted.begin(), 
                                              h_categories_sorted.begin() + num_categories_sorted_test);
  std::unordered_set<dtype> category_set_samples_test(
    h_samples.begin(), h_samples.end()
  );
  if (category_set_test == category_set_samples_test) {
    std::cout << "The sorted categories are the same as in the samples and cover all samples!" << std::endl;
  } else {
    std::cout << "The sorted categories are NOT the same as in the samples and cover all samples!" << std::endl;
  }
  EXPECT_TRUE(category_set_test == category_set_samples_test);
  std::unordered_set<dtype> category_set_ref(categories.begin(), 
                                             categories.end());
  if (category_set_test == category_set_ref) {
    std::cout << "The sorted categories are the same as the reference sorted!" << std::endl;
  } else {
    std::cout << "The sorted categories are NOT the same as the reference sorted!" << std::endl;
  }
  EXPECT_TRUE(category_set_test == category_set_ref);
  size_t count_ne = (size_t)0;
  for (size_t c = 0; c < categories.size(); ++c) {
    count_ne += ((size_t)h_categories_sorted[c] != (size_t)categories[c] ? 1 : 0);
  } 
  if (count_ne > 0) std::cout << "Number of different categories : "
                              << (double) count_ne / (double) categories.size() * 100.
                              << " %"
                              << std::endl;
  EXPECT_EQ(count_ne, 0);
}

TEST(calculate_statistics_test, dtype_uint32) {
  // const size_t N = 5;
  // for (size_t batch_size=128; batch_size < 15*64*1024; batch_size = 2*batch_size) {
  //   for (size_t num_tables=1; num_tables <= 32; num_tables = 2*num_tables) {
  //     for (size_t i = 0; i < N; ++i) {
  //       statistics_test<uint32_t>(batch_size, num_tables);
  //     }
  //   }
  // }
  const size_t N = 5;
  for (size_t batch_size=8; batch_size < 16; batch_size = 2*batch_size) {
    for (size_t num_tables=2; num_tables <= 2; num_tables = 2*num_tables) {
      for (size_t i = 0; i < N; ++i) {
        statistics_test<uint32_t>(batch_size, num_tables);
      }
    }
  }
}

// TEST(calculate_statistics_test, dtype_long_long) { 
//   const size_t N = 5;
//   for (size_t batch_size=128; batch_size < 15*64*1024; batch_size = 2*batch_size) {
//     for (size_t num_tables=1; num_tables <= 32; num_tables = 2*num_tables) {
//       for (size_t i = 0; i < N; ++i) {
//         statistics_test<long long>(batch_size, num_tables);
//       }
//     }
//   }
// }