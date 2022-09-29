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

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/protobufs)

set(PROTOBUF_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/dredd.proto)

set(DREDD_PROTOC_COMMAND
    "protobuf::protoc"
    CACHE
      STRING
      "The command to invoke the protobuf compiler (protoc). By default it is the protobufs::protoc CMake target."
)

add_custom_command(
  OUTPUT protobufs/dredd.pb.cc protobufs/dredd.pb.h
  COMMAND "${DREDD_PROTOC_COMMAND}" -I=${CMAKE_CURRENT_SOURCE_DIR}/src
          --cpp_out=protobufs ${PROTOBUF_SOURCE}
  DEPENDS ${PROTOBUF_SOURCE}
  COMMENT "Generate protobuf sources from proto definition file.")

add_library(
  libdredd STATIC
  include/libdredd/mutation.h
  include/libdredd/mutation_info.h
  include/libdredd/mutation_remove_stmt.h
  include/libdredd/mutation_replace_binary_operator.h
  include/libdredd/mutation_replace_expr.h
  include/libdredd/mutation_replace_unary_operator.h
  include/libdredd/new_mutate_frontend_action_factory.h
  include_private/include/libdredd/mutate_ast_consumer.h
  include_private/include/libdredd/mutate_visitor.h
  include_private/include/libdredd/mutation_tree_node.h
  include_private/include/libdredd/util.h
  ${CMAKE_CURRENT_BINARY_DIR}/protobufs/dredd.pb.h
  src/mutate_ast_consumer.cc
  src/mutate_visitor.cc
  src/mutation.cc
  src/mutation_info.cc
  src/mutation_remove_stmt.cc
  src/mutation_replace_binary_operator.cc
  src/mutation_replace_expr.cc
  src/mutation_replace_unary_operator.cc
  src/mutation_tree_node.cc
  src/new_mutate_frontend_action_factory.cc
  src/util.cc
  ${CMAKE_CURRENT_BINARY_DIR}/protobufs/dredd.pb.cc)

# Compilation of the auto-generated protobuf source file will yield warnings,
# which should be ignored.
if(MSVC)
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/protobufs/dredd.pb.cc
                              PROPERTIES COMPILE_FLAGS /w)
else()
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/protobufs/dredd.pb.cc
                              PROPERTIES COMPILE_FLAGS -w)
endif()

target_include_directories(
  libdredd
  PUBLIC include
  PRIVATE include_private/include
  PRIVATE ${CMAKE_BINARY_DIR}/src)

set(CLANG_LIBS clangTooling clangTransformer)

foreach(clang_target ${CLANG_LIBS})
  if(NOT TARGET ${clang_target})
    message(FATAL_ERROR "${clang_target} is not a target")
  endif()
endforeach()

target_link_libraries(libdredd PRIVATE ${CLANG_LIBS} protobuf::libprotobuf)

target_compile_features(libdredd PUBLIC cxx_std_20)
