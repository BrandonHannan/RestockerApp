cmake_minimum_required(VERSION 3.20)
project(RestockerApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Threads REQUIRED)

# libcurl-impersonate (Chrome TLS/JA3 + HTTP-2 fingerprint) for the HTTP transport.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchCurlImpersonate.cmake)

add_executable(RestockerApp
  src/main.cpp
  src/Config.cpp
  src/HttpClient.cpp
  src/Database.cpp
  src/ConstructorClient.cpp
  src/SitemapClient.cpp
  src/KmartGraphQLClient.cpp
  src/KmartHttpTransport.cpp
  src/CdpClient.cpp
  src/DiscordNotifier.cpp
  src/GenericWebhookNotifier.cpp
  src/NotifierManager.cpp
  src/DiscoveryLoop.cpp
  src/InventoryLoop.cpp
)

target_include_directories(RestockerApp PRIVATE src)

target_link_libraries(RestockerApp PRIVATE
  curl_impersonate::curl_impersonate
  nlohmann_json::nlohmann_json
  SQLiteCpp
  spdlog::spdlog
  Threads::Threads
)

if(WIN32)
  target_link_libraries(RestockerApp PRIVATE ws2_32)
endif()

# Ship the curl-impersonate runtime DLLs next to the executable on Windows.
if(WIN32 AND CURL_IMPERSONATE_RUNTIME_DLLS)
  foreach(_dll ${CURL_IMPERSONATE_RUNTIME_DLLS})
    add_custom_command(TARGET RestockerApp POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${_dll}" "$<TARGET_FILE_DIR:RestockerApp>"
      VERBATIM)
  endforeach()
endif()

# On Linux, find the impersonate .so at runtime from its extracted location.
if(UNIX AND NOT APPLE)
  set_target_properties(RestockerApp PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/_deps/curl-impersonate/extracted"
    INSTALL_RPATH "$ORIGIN")
endif()
