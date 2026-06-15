cmake_minimum_required(VERSION 3.20)
project(mis_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# ---- httplib (header-only, at D:/dev/db) ----
find_path(HTTPLIB_INCLUDE_DIR httplib.h
    HINTS "D:/dev/db"
)
if(NOT HTTPLIB_INCLUDE_DIR)
    message(FATAL_ERROR "httplib.h not found. Expected at D:/dev/db")
endif()

# ---- nlohmann_json (header-only, bundled in include/) ----
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    HINTS "${CMAKE_SOURCE_DIR}/include"
)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    message(FATAL_ERROR "nlohmann/json.hpp not found in include/")
endif()

# ---- Oracle OCI — 23ai Free ----
set(ORACLE_HOME "D:/tools/26ai/dbhomeFree" CACHE PATH "Oracle 23ai Free install dir")

find_path(OCI_INCLUDE_DIR oci.h
    HINTS "${ORACLE_HOME}/oci/include"
)
find_library(OCI_LIBRARY NAMES oci
    HINTS "${ORACLE_HOME}/oci/lib/mingw"
)

if(NOT OCI_INCLUDE_DIR OR NOT OCI_LIBRARY)
    message(WARNING "Oracle OCI not found. Building without Oracle support (in-memory mode only).")
    set(MIS_HAS_ORACLE OFF)
else()
    set(MIS_HAS_ORACLE ON)
    add_compile_definitions(MIS_HAS_ORACLE)
endif()

add_executable(mis_backend
    src/main.cpp
    src/services/InventoryService.cpp
    src/utils/Jwt.cpp
    src/controllers/AuthController.cpp
    src/controllers/InventoryController.cpp
    src/controllers/SkuController.cpp
    src/controllers/SupplierController.cpp
)

if(MIS_HAS_ORACLE)
    target_sources(mis_backend PRIVATE src/dao/OracleConnector.cpp)
endif()

target_include_directories(mis_backend
    PRIVATE
        include
        ${HTTPLIB_INCLUDE_DIR}
        ${NLOHMANN_JSON_INCLUDE_DIR}
)

if(MIS_HAS_ORACLE)
    target_include_directories(mis_backend PRIVATE ${OCI_INCLUDE_DIR})
    target_link_libraries(mis_backend PRIVATE ${OCI_LIBRARY})
endif()

target_link_libraries(mis_backend
    PRIVATE
        Threads::Threads
        ws2_32
        bcrypt
)

# 静态链接 gcc/libstdc++/winpthread（避免 DLL 依赖）
target_link_options(mis_backend PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic,-lwinpthread)
