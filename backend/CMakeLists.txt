cmake_minimum_required(VERSION 3.20)
project(mis_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Oracle OCI/OCCI — 23ai Free 默认路径
set(ORACLE_HOME "D:/tools/26ai/dbhomeFree" CACHE PATH "Oracle 23ai Free 安装目录")

find_path(OCI_INCLUDE_DIR oci.h
    HINTS "${ORACLE_HOME}/oci/include"
)
find_library(OCI_LIBRARY NAMES oci
    HINTS "${ORACLE_HOME}/oci/lib/msvc"
)
find_library(OCCI_LIBRARY NAMES oraocci23
    HINTS "${ORACLE_HOME}/oci/lib/msvc"
)

if(NOT OCI_INCLUDE_DIR OR NOT OCI_LIBRARY OR NOT OCCI_LIBRARY)
    message(FATAL_ERROR "Oracle OCI/OCCI not found. Check ORACLE_HOME=${ORACLE_HOME}")
endif()

add_executable(mis_backend
    src/main.cpp
    src/dao/OracleConnector.cpp
    src/dao/InventoryDAO.cpp
    src/services/InventoryService.cpp
    src/controllers/InventoryController.cpp
)

target_include_directories(mis_backend
    PRIVATE
        include
        ${OCI_INCLUDE_DIR}
)

target_link_libraries(mis_backend
    PRIVATE
        httplib::httplib
        nlohmann_json::nlohmann_json
        Threads::Threads
        ${OCI_LIBRARY}
        ${OCCI_LIBRARY}
)
