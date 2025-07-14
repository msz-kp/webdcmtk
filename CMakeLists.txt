cmake_minimum_required(VERSION 3.16)

project(webdcmtk VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#add_compile_options(-fsanitize=address,undefined,leak -fsanitize-recover=address,undefined -fsanitize=pointer-compare,pointer-subtract)
#add_link_options(-fsanitize=address,undefined,leak)

include(cmake/CPM.cmake)
set(CPM_USE_LOCAL_PACKAGES ON)

# CPMAddPackage("gh:fmtlib/fmt#9.1.0")
# CPMAddPackage(
#   NAME dcmtk
#   FIND_PACKAGE_ARGUMENTS "NAMES DCMTK"
#   VERSION 3.6.7
#   GITHUB_REPOSITORY DCMTK/dcmtk
#   GIT_TAG DCMTK-3.6.9
#   OPTIONS "DCMTK_SHARED_LIBRARIES OFF"
#   "DCMTK_BUILD_APPS OFF"
#   "DCMTK_WITH_OPENSSL ON"
#   "DCMTK_ENABLE_PRIVATE_TAGS ON"
#   "DCMTK_WITH_XML ON"
#   "DCMTK_WITH_TIFF ON"
#   "DCMTK_WITH_PNG ON"
#   "DCMTK_WITH_OPENJPEG ON"
#   "DCMTK_WITH_ZLIB ON"
#   "DCMTK_WITH_ICONV ON"
#   "DCMTK_WITH_ICU OFF"
#   "USE_COMPILER_HIDDEN_VISIBILITY ON"
#   "CMAKE_VERBOSE_MAKEFILE ON"
#   "CMAKE_SKIP_RPATH OFF"
#   "DCMTK_ENABLE_STL ON"
#   "CMAKE_CXX_STANDARD 20"
#   "DCMTK_DEFAULT_DICT builtin"
# )
# if (dcmtk_ADDED)
#   add_library(DCMTK::DCMTK ALIAS DCMTK)
# endif()

find_package(DCMTK REQUIRED)
find_package(fmt REQUIRED)

CPMAddPackage(
  NAME drogon
  FIND_PACKAGE_ARGUMENTS "NAMES Drogon"
  VERSION 1.9.9
  GITHUB_REPOSITORY drogonframework/drogon
  GIT_TAG v1.9.9
  OPTIONS "BUILD_CTL OFF" "BUILD_EXAMPLES OFF" "BUILD_REDIS OFF" "BUILD_MYSQL OFF" "BUILD_SQLITE OFF BUILD_C-ARES OFF" "BUILD_ORM OFF"
)
if (drogon_ADDED)
  add_library(Drogon::Drogon ALIAS drogon)
endif()

add_executable(${PROJECT_NAME}
  dicomweb.cpp
  dimse.cpp
  image.cpp
  mime.cpp
  misc.cpp
  webdcmtk.cpp
)

target_link_libraries(${PROJECT_NAME} DCMTK::DCMTK Drogon::Drogon fmt::fmt)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
target_link_options(${PROJECT_NAME} PRIVATE -Wl,--as-needed)
