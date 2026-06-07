# FetchCurlImpersonate.cmake
#
# Provides the IMPORTED target `curl_impersonate::curl_impersonate`: a drop-in
# libcurl (lexiforest curl-impersonate) that presents Chrome's TLS/JA3 + HTTP-2
# fingerprint via curl_easy_impersonate(). This is what gets the inventory tier
# past Akamai Bot Manager on the Kmart GraphQL gateway.
#
# Behaviour:
#   * -DCURL_IMPERSONATE_ROOT=<dir> : use a pre-extracted/self-built tree, no download.
#       Expected layout: <root>/include/curl/curl.h (optional on Linux),
#       <root>/lib/<implib|.so>, and on Windows <root>/bin/*.dll.
#   * otherwise: download the pinned prebuilt release for this platform.
#
# Outputs:
#   * target curl_impersonate::curl_impersonate
#   * CURL_IMPERSONATE_RUNTIME_DLLS : list of DLLs to copy next to the exe (Windows only)

set(CI_VERSION "v1.5.6")
set(CI_BASE_URL "https://github.com/lexiforest/curl-impersonate/releases/download/${CI_VERSION}")

# Pinned archive name + SHA256 per platform (x86_64 only for now).
if(WIN32)
    set(_ci_archive "libcurl-impersonate-${CI_VERSION}.x86_64-win32.tar.gz")
    set(_ci_sha256 "fe8ce2488d5467fda6061b8b130b5834bc30cdfff40712692e8c5685dbbda6c7")
elseif(UNIX AND NOT APPLE)
    set(_ci_archive "libcurl-impersonate-${CI_VERSION}.x86_64-linux-gnu.tar.gz")
    set(_ci_sha256 "f07e25084020c54d6fd5654c8d458e09b3a44c312f88e480c255399f00487b25")
else()
    message(FATAL_ERROR
        "curl-impersonate: unsupported platform. Pass -DCURL_IMPERSONATE_ROOT=<dir> "
        "pointing at a build for your OS (see https://github.com/lexiforest/curl-impersonate/releases).")
endif()

# Resolve the extracted root, downloading if no override was given.
if(DEFINED CURL_IMPERSONATE_ROOT AND CURL_IMPERSONATE_ROOT)
    set(_ci_root "${CURL_IMPERSONATE_ROOT}")
    message(STATUS "curl-impersonate: using CURL_IMPERSONATE_ROOT=${_ci_root}")
else()
    set(_ci_dl_dir "${CMAKE_BINARY_DIR}/_deps/curl-impersonate")
    set(_ci_root "${_ci_dl_dir}/extracted")
    set(_ci_archive_path "${_ci_dl_dir}/${_ci_archive}")
    if(NOT EXISTS "${_ci_root}/.stamp")
        message(STATUS "curl-impersonate: downloading ${_ci_archive}")
        file(DOWNLOAD "${CI_BASE_URL}/${_ci_archive}" "${_ci_archive_path}"
             EXPECTED_HASH SHA256=${_ci_sha256}
             SHOW_PROGRESS STATUS _ci_dl_status)
        list(GET _ci_dl_status 0 _ci_dl_code)
        if(NOT _ci_dl_code EQUAL 0)
            list(GET _ci_dl_status 1 _ci_dl_msg)
            message(FATAL_ERROR "curl-impersonate download failed: ${_ci_dl_msg}")
        endif()
        file(MAKE_DIRECTORY "${_ci_root}")
        file(ARCHIVE_EXTRACT INPUT "${_ci_archive_path}" DESTINATION "${_ci_root}")
        file(WRITE "${_ci_root}/.stamp" "${CI_VERSION}")
    endif()
endif()

add_library(curl_impersonate::curl_impersonate SHARED IMPORTED GLOBAL)
set(CURL_IMPERSONATE_RUNTIME_DLLS "" CACHE INTERNAL "curl-impersonate runtime DLLs")

if(WIN32)
    # Windows archive layout: ./include ./lib ./bin
    set(_ci_include "${_ci_root}/include")
    set(_ci_implib "${_ci_root}/lib/libcurl-impersonate_imp.lib")
    set(_ci_dll "${_ci_root}/bin/libcurl-impersonate.dll")
    if(NOT EXISTS "${_ci_implib}")
        message(FATAL_ERROR "curl-impersonate: import lib not found at ${_ci_implib}")
    endif()
    set_target_properties(curl_impersonate::curl_impersonate PROPERTIES
        IMPORTED_IMPLIB "${_ci_implib}"
        IMPORTED_LOCATION "${_ci_dll}"
        INTERFACE_INCLUDE_DIRECTORIES "${_ci_include}")
    # The DLL dynamically links zlib.dll; ship both next to the exe.
    set(_ci_dlls "${_ci_dll}")
    if(EXISTS "${_ci_root}/bin/zlib.dll")
        list(APPEND _ci_dlls "${_ci_root}/bin/zlib.dll")
    endif()
    set(CURL_IMPERSONATE_RUNTIME_DLLS "${_ci_dlls}" CACHE INTERNAL "curl-impersonate runtime DLLs")
else()
    # Linux archive is flat (lib only, no headers). Use the real .so by path and
    # fall back to system curl headers; the curl_easy_impersonate() prototype is
    # declared in our own source as a safety net.
    set(_ci_so "${_ci_root}/libcurl-impersonate.so")
    if(NOT EXISTS "${_ci_so}")
        file(GLOB _ci_so_glob "${_ci_root}/lib*curl-impersonate*.so*")
        if(_ci_so_glob)
            list(GET _ci_so_glob 0 _ci_so)
        endif()
    endif()
    if(NOT EXISTS "${_ci_so}")
        message(FATAL_ERROR "curl-impersonate: shared object not found under ${_ci_root}")
    endif()
    set_target_properties(curl_impersonate::curl_impersonate PROPERTIES
        IMPORTED_LOCATION "${_ci_so}"
        IMPORTED_NO_SONAME TRUE)
    # Prefer bundled headers if a root override supplied them, else system curl.
    if(EXISTS "${_ci_root}/include/curl/curl.h")
        set_target_properties(curl_impersonate::curl_impersonate PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_ci_root}/include")
    else()
        find_path(SYSTEM_CURL_INCLUDE_DIR curl/curl.h)
        if(NOT SYSTEM_CURL_INCLUDE_DIR)
            message(FATAL_ERROR
                "curl-impersonate (Linux): no bundled headers and no system curl/curl.h found. "
                "Install libcurl headers (e.g. apt install libcurl4-openssl-dev) or pass "
                "-DCURL_IMPERSONATE_ROOT to a tree that includes include/curl/.")
        endif()
        set_target_properties(curl_impersonate::curl_impersonate PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${SYSTEM_CURL_INCLUDE_DIR}")
    endif()
endif()
