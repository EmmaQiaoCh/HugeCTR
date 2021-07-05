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

#include "operation/operation.h"
#include "common.h"

namespace SparseOperationKit {

Operation::Operation(ConstructionContext_t context)
: base_context_(context), op_name_(gen_unique_op_name("sok_operation"))
{}

void Operation::AllocateForwardSpaces(size_t const global_batch_size) {
    allocate_forward_spaces(global_batch_size);
    if (next_op_) next_op_->AllocateForwardSpaces(global_batch_size);
}

void Operation::AllocateBackwardSpaces(size_t const global_batch_size) {
    allocate_backward_spaces(global_batch_size);
    if (next_op_) next_op_->AllocateBackwardSpaces(global_batch_size);
}

void Operation::Forward(const Context_t &replica_context, const bool training) {
    forward(replica_context, training);
    if (next_op_) next_op_->Forward(replica_context, training);
}

void Operation::Backward(const Context_t &replica_context) {
    if (next_op_) next_op_->Backward(replica_context);
    backward(replica_context);
}

void Operation::set_next(std::shared_ptr<Operation> operation) {
    if (nullptr == next_op_) { // next_op_ is invalid
        next_op_ = operation;
        return;
    } else { // next_op_ is valid, then link it to its next_op
        return next_op_->set_next(operation);
    }
}

ConstructionContext_t Operation::base_context() const {
    return base_context_;
}

std::unordered_set<std::string> Operation::operation_names;

std::string Operation::gen_unique_op_name(const std::string op_name) {
    std::string unique_op_name = op_name;

    while (true) {
        auto iter = operation_names.find(unique_op_name);
        if (operation_names.end() != iter) { // already exists
            auto name_vec = str_split(unique_op_name, /*pattern=*/"_");
            const int32_t num = string2num(*(name_vec.rbegin()));
            if (-1 == num) { // not numerical
                unique_op_name = op_name + "_" + std::to_string(1);
            } else { // numerical
                *(name_vec.rbegin()) = std::to_string(num + 1);
                unique_op_name = strs_concat(name_vec, /*connection=*/"_");
            }
        } else { // not exists
            operation_names.emplace(unique_op_name);
            break;
        }
    }

    return unique_op_name;
}

void Operation::set_op_name(const std::string& op_name) {
    const std::string &unique_op_name = gen_unique_op_name(op_name);
    op_name_ = unique_op_name;
}

std::string Operation::get_op_name() const {
    return op_name_;
}

void Operation::DumpToFile(const std::string filepath) const {
    try {
        const std::string filename = filepath + "/" + get_op_name() + ".file";
        std::ofstream file_stream = std::ofstream(filename, std::ios::binary | std::ios::out);
        dump(file_stream);
        const size_t file_size_in_bytes = file_stream.tellp();
        file_stream.close();
        if (0 == file_size_in_bytes) delete_file(filename);

        if (next_op_) next_op_->DumpToFile(filepath);

    } catch (const std::exception &error) {
        throw std::runtime_error(ErrorBase + error.what());
    }
}

void Operation::dump(std::ofstream &filestream) const {
    // by default, it does nothing.
}

void Operation::RestoreFromFile(const std::string filepath) {
    try {
        const std::string filename = filepath + "/" + get_op_name() + ".file";
        if (file_exist(filename)) {
            std::ifstream file_stream = std::ifstream(filename, std::ios::binary | std::ios::out);
            restore(file_stream);
            file_stream.close();
        }

        if (next_op_) next_op_->RestoreFromFile(filepath);
    } catch (const std::exception &error) {
        throw std::runtime_error(ErrorBase + error.what());
    }
}

void Operation::restore(const std::ifstream &filestream) {
    // by default, it does nothing.
}

void Operation::LoadEmbeddingValues(const std::vector<std::shared_ptr<Tensor>>& tensor_list) {
    load_embedding_values(tensor_list);
    if (next_op_) next_op_->LoadEmbeddingValues(tensor_list);
}

void Operation::load_embedding_values(const std::vector<std::shared_ptr<Tensor>>& tensor_list) {
    // by default, it does nothing.
}

} // namespace SparseOperationKit