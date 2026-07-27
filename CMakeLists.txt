cmake_minimum_required(VERSION 3.20)
project(he_profile_steering LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_SEAL "Build with Microsoft SEAL support" OFF)
option(USE_CKKS "Use CKKS instead of BFV" OFF)

add_library(profile_steering
        src/old/ProfileSteering.cpp
        src/old/ProfileSteeringPSM.cpp
        src/Household.cpp
        src/SecureAggregator.cpp
        src/Devices.cpp
        src/OptAlg.cpp
)

target_include_directories(profile_steering PUBLIC include)

add_executable(he_profile_steering src/old/main.cpp)
target_link_libraries(he_profile_steering PRIVATE profile_steering)

add_executable(he_profile_steering_psm src/old/mainPSM.cpp)
target_link_libraries(he_profile_steering_psm PRIVATE profile_steering)

add_executable(he_profile_steering_psmcc src/old/mainPSMCC.cpp)
target_link_libraries(he_profile_steering_psmcc PRIVATE profile_steering)

add_executable(he_component_bench src/mainComponentBench.cpp)
target_link_libraries(he_component_bench PRIVATE profile_steering)

add_executable(he_encrypt_bench src/mainEncrypt.cpp)
target_link_libraries(he_encrypt_bench PRIVATE profile_steering)
if(USE_SEAL)
    find_package(SEAL REQUIRED)

    target_compile_definitions(profile_steering PUBLIC USE_SEAL)
    target_link_libraries(profile_steering PUBLIC SEAL::seal)

    if(USE_CKKS)
        target_sources(profile_steering PRIVATE src/HEAggregatorCKKS.cpp)
        target_compile_definitions(profile_steering PUBLIC USE_CKKS)
    else()
        target_sources(profile_steering PRIVATE src/HEAggregator.cpp)
    endif()
endif()