# 
# Copyright (c) 2020, NVIDIA CORPORATION.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#      http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cmake_minimum_required(VERSION 3.8)
file(GLOB auc_test_src
  auc_test.cpp
)

add_executable(auc_test ${auc_test_src})
target_compile_features(auc_test PUBLIC cxx_std_14)
target_link_libraries(auc_test PUBLIC huge_ctr_static gtest gtest_main)

add_custom_command(
  OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/python_auc.py"
  COMMAND ${CMAKE_COMMAND} -E copy
  "${CMAKE_CURRENT_SOURCE_DIR}/python_auc.py"
  "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/python_auc.py"
)

add_custom_target(copy_python DEPENDS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/python_auc.py")
add_dependencies(auc_test copy_python)