set(ABSL_ROOT_DIR "${frs_SOURCE_DIR}/contrib/abseil-cpp")
if(NOT EXISTS "${ABSL_ROOT_DIR}/CMakeLists.txt")
  message(FATAL_ERROR " submodule contrib/abseil-cpp is missing. To fix try run: \n git submodule update --init --recursive")
endif()

set(ABSL_PROPAGATE_CXX_STD "ON")
set(BUILD_SHARED_LIBS "FALSE")
set(BUILD_TESTING "OFF")
add_subdirectory("${ABSL_ROOT_DIR}" "${frs_BINARY_DIR}/contrib/abseil-cpp")
add_library(abseil_library INTERFACE)

target_link_libraries(abseil_library INTERFACE
  absl::flat_hash_map
  absl::flat_hash_set
  absl::strings
  absl::time
)

get_target_property(FLAT_HASH_MAP_INCLUDE_DIR absl::flat_hash_map INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories (abseil_library SYSTEM BEFORE INTERFACE ${FLAT_HASH_MAP_INCLUDE_DIR})

get_target_property(FLAT_HASH_SET_INCLUDE_DIR absl::flat_hash_set INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories (abseil_library SYSTEM BEFORE INTERFACE ${FLAT_HASH_SET_INCLUDE_DIR})

get_target_property(STRINGS_INCLUDE_DIR absl::strings INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories (abseil_library SYSTEM BEFORE INTERFACE ${STRINGS_INCLUDE_DIR})

get_target_property(STRINGS_INCLUDE_DIR absl::time INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories (abseil_library SYSTEM BEFORE INTERFACE ${STRINGS_INCLUDE_DIR})
