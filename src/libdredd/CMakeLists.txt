# Copyright 2022 The Dredd Project Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.13)

add_library(
  libdredd STATIC
  include/libdredd/mutation.h
  include/libdredd/mutation_remove_statement.h
  include/libdredd/mutation_replace_binary_operator.h
  include/libdredd/mutation_replace_unary_operator.h
  include/libdredd/new_mutate_frontend_action_factory.h
  include_private/include/libdredd/mutate_ast_consumer.h
  include_private/include/libdredd/mutate_visitor.h
  include_private/include/libdredd/util.h
  src/mutate_ast_consumer.cc
  src/mutate_visitor.cc
  src/mutation.cc
  src/mutation_remove_statement.cc
  src/mutation_replace_binary_operator.cc
  src/mutation_replace_unary_operator.cc
  src/new_mutate_frontend_action_factory.cc
  src/util.cc)

target_include_directories(
  libdredd
  PUBLIC include
  PRIVATE include_private/include)

set(CLANG_LIBS clangTooling clangTransformer)

foreach(clang_target ${CLANG_LIBS})
  if(NOT TARGET ${clang_target})
    message(FATAL_ERROR "${clang_target} is not a target")
  endif()
endforeach()

target_link_libraries(libdredd PRIVATE ${CLANG_LIBS})

target_compile_features(libdredd PUBLIC cxx_std_20)
