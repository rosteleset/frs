set(CPR_ROOT_DIR "${frs_SOURCE_DIR}/contrib/cpr")
if(NOT EXISTS "${CPR_ROOT_DIR}/CMakeLists.txt")
  message(FATAL_ERROR " submodule contrib/cpr is missing. To fix try run: \n git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS "FALSE")
add_subdirectory("${CPR_ROOT_DIR}" "${frs_BINARY_DIR}/contrib/cpr")
add_library(cpr_library INTERFACE)

target_link_libraries(cpr_library INTERFACE
  cpr::cpr
)

get_target_property(CPR_INCLUDE_DIR cpr::cpr INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories (cpr_library SYSTEM BEFORE INTERFACE ${CPR_INCLUDE_DIR})
