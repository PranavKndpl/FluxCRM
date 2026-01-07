cmake_minimum_required(VERSION 3.15)
project(FluxCRM VERSION 0.1.0 LANGUAGES CXX)

# 1. Standard C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. Add the FluxDB Driver
# Option A: If you copied files to vendor/fluxdb
add_subdirectory(vendor/fluxdb) 

# Option B: (The Pro Move) Download directly from GitHub!
# include(FetchContent)
# FetchContent_Declare(
#   fluxdb_driver
#   GIT_REPOSITORY https://github.com/PranavKndpl/FluxDB.git
#   SOURCE_SUBDIR drivers/cpp
# )
# FetchContent_MakeAvailable(fluxdb_driver)

# 3. Create the CRM Executable
add_executable(flux_crm src/main.cpp)

# 4. Link Dependencies
# Notice we use the alias we defined in step 1
target_link_libraries(flux_crm PRIVATE fluxdb::driver)