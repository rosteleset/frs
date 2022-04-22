set(CROW_ROOT_DIR "${frs_SOURCE_DIR}/contrib/crow")
if(NOT EXISTS "${CROW_ROOT_DIR}/CMakeLists.txt")
  message(FATAL_ERROR " submodule contrib/crow is missing. To fix try run: \n git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS "FALSE")
add_subdirectory("${CROW_ROOT_DIR}" "${frs_BINARY_DIR}/contrib/crow")
add_library(crow_library INTERFACE)

target_link_libraries(crow_library INTERFACE
  Crow::Crow
)

get_target_property(CROW_INCLUDE_DIR Crow::Crow INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories (crow_library SYSTEM BEFORE INTERFACE ${CROW_INCLUDE_DIR})
