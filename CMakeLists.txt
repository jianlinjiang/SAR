# MIT License

# Copyright (c) 2020 jianlinjiang

cmake_minimum_required(VERSION 2.8.10)

project(SAR C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})
find_package(SGX REQUIRED)

############################################################
# brpc begin 
option(LINK_SO "Whether examples are linked dynamically" OFF)

include(FindThreads)
include(FindProtobuf)
# include PROTO_HEADER
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_path(GPERFTOOLS_INCLUDE_DIR NAMES gperftools/heap-profiler.h)
find_library(GPERFTOOLS_LIBRARIES NAMES tcmalloc_and_profiler)
include_directories(${GPERFTOOLS_INCLUDE_DIR})

find_path(BRPC_INCLUDE_PATH NAMES brpc/server.h)
if(LINK_SO)
  find_library(BRPC_LIB NAMES brpc)
else()
  find_library(BRPC_LIB NAMES libbrpc.a brpc)
endif()
if((NOT BRPC_INCLUDE_PATH) OR (NOT BRPC_LIB))
  message(FATAL_ERROR "Fail to find brpc!")
endif()
include_directories(${BRPC_INCLUDE_PATH})

find_path(GFLAGS_INCLUDE_PATH gflags/gflags.h)
find_library(GFLAGS_LIBRARY NAMES gflags libgflags)
if((NOT GFLAGS_INCLUDE_PATH) OR (NOT GFLAGS_LIBRARY))
    message(FATAL_ERROR "Fail to find gflags")
endif()

include_directories(${GFLAGS_INCLUDE_PATH})

execute_process(
    COMMAND bash -c "grep \"namespace [_A-Za-z0-9]\\+ {\" ${GFLAGS_INCLUDE_PATH}/gflags/gflags_declare.h | head -1 | awk '{print $2}' | tr -d '\n'"
    OUTPUT_VARIABLE GFLAGS_NS
)
if(${GFLAGS_NS} STREQUAL "GFLAGS_NAMESPACE")
    execute_process(
        COMMAND bash -c "grep \"#define GFLAGS_NAMESPACE [_A-Za-z0-9]\\+\" ${GFLAGS_INCLUDE_PATH}/gflags/gflags_declare.h | head -1 | awk '{print $3}' | tr -d '\n'"
        OUTPUT_VARIABLE GFLAGS_NS
    )
endif()

find_path(LEVELDB_INCLUDE_PATH NAMES leveldb/db.h)
find_library(LEVELDB_LIB NAMES leveldb)
if ((NOT LEVELDB_INCLUDE_PATH) OR (NOT LEVELDB_LIB))
    message(FATAL_ERROR "Fail to find leveldb")
endif()
include_directories(${LEVELDB_INCLUDE_PATH})

find_library(SSL_LIB NAMES ssl)
if (NOT SSL_LIB)
    message(FATAL_ERROR "Fail to find ssl")
endif()

find_library(CRYPTO_LIB NAMES crypto)
if (NOT CRYPTO_LIB)
    message(FATAL_ERROR "Fail to find crypto")
endif()

set(DYNAMIC_LIB
    ${CMAKE_THREAD_LIBS_INIT}
    ${GFLAGS_LIBRARY}
    ${PROTOBUF_LIBRARIES}
    ${LEVELDB_LIB}
    ${SSL_LIB}
    ${CRYPTO_LIB}
    dl
    )
# generate protobuf source code
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER srcs/proto/ra.proto srcs/proto/sar.proto)

# set(BRPC_CMAKE_CPP_FLAGS "-DGFLAGS_NS=${GFLAGS_NS}")
set(BRPC_CMAKE_CPP_FLAGS "")
set(BRPC_CMAKE_CXX_FLAGS "${BRPC_CMAKE_CPP_FLAGS} -DDEBUG -O0 -D__const__= -pipe -W -Wall -Wno-unused-parameter -fPIC -fno-omit-frame-pointer")
set(BRPC_CMAKE_CXX_FLAGS "${BRPC_CMAKE_CXX_FLAGS} -DBRPC_ENABLE_CPU_PROFILER -std=c++11")
# brpc end
############################################################

############################################################
# sgx start

if (SGX_HW)
    set(EPID_Link_Flags "sgx_epid")
    set(QUOTE_Link_Flags "sgx_quote_ex") 
else ()
    set(EPID_Link_Flags "sgx_epid_sim")
    set(QUOTE_Link_Flags "sgx_quote_ex_sim") 
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)

set(CONFIG_PATH srcs)
set(EDL_SEARCH_PATH srcs/enclave)
set(E_SRCS srcs/enclave/ra_enclave.cpp srcs/enclave/sar_enclave.cpp)
set(ULIB_SRCS 
    # srcs/ra_service/ra_service.cpp 
    srcs/ra_service/server_config.cpp
    srcs/ra_service/sar_server.cpp
    srcs/ra_service/sar_ocall.cpp
    # srcs/ra_service/ecc_context.cpp
    # srcs/ra_service/enclave_context.cpp
    ${PROTO_SRC} )

if(SGX_HW AND SGX_MODE STREQUAL "Release")
    set(LDS ${CONFIG_PATH}/enclave.lds)
else ()
    set(LDS ${CONFIG_PATH}/enclave_debug.lds)
endif()
# add enclave library which is compiled by trusted part
add_enclave_library(enclave SRCS ${E_SRCS} EDL ${EDL_SEARCH_PATH}/enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATH} LDSCRIPT ${LDS})

# use the test key to sign the enclave
enclave_sign(enclave KEY ${CONFIG_PATH}/enclave_test.pem CONFIG ${CONFIG_PATH}/enclave.config.xml)

# add untrusted library
add_untrusted_library(untrusted_lib STATIC SRCS ${ULIB_SRCS} LIBS ${BRPC_LIB} ${DYNAMIC_LIB} FLAGS ${BRPC_CMAKE_CXX_FLAGS})

set(SRCS 
    srcs/ra_service/ra_service.cpp
    srcs/ra_service/sar_service.cpp
    srcs/main.cpp 
    srcs/ra_service/ecc_context.cpp
    srcs/ra_service/enclave_context.cpp 
    srcs/ra_service/sar_util.cpp)
add_untrusted_executable(SAR_SERVER SRCS ${SRCS} EDL ${EDL_SEARCH_PATH}/enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATH} UTRUST_LIBS untrusted_lib "sgx_ukey_exchange" ${EPID_Link_Flags} ${QUOTE_Link_Flags} ${GPERFTOOLS_LIBRARIES} FLAGS "-DBRPC_ENABLE_CPU_PROFILER")
