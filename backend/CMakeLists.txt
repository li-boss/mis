cmake_minimum_required(VERSION 3.20)
project(mis_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

find_path(OCI_INCLUDE_DIR oci.h)
find_library(OCI_LIBRARY NAMES clntsh oci)

if(NOT OCI_INCLUDE_DIR OR NOT OCI_LIBRARY)
    message(FATAL_ERROR "Oracle OCI headers/library not found. Set OCI_INCLUDE_DIR and OCI_LIBRARY.")
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
)
