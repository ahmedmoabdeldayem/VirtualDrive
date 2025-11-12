include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Sources under test (no main.cpp, no stack adapters — unit tests are protocol-independent)
set(TESTABLE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/drive/DriveStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/drive/MotorModel.cpp
    ${CMAKE_SOURCE_DIR}/src/rt/CyclicThread.cpp
)

# DriveStateMachine tests
add_executable(DriveStateMachineTest
    DriveStateMachineTest.cpp
    ${CMAKE_SOURCE_DIR}/src/drive/DriveStateMachine.cpp
)
target_include_directories(DriveStateMachineTest PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(DriveStateMachineTest PRIVATE GTest::gtest_main Threads::Threads)
gtest_discover_tests(DriveStateMachineTest)

# MotorModel tests
add_executable(MotorModelTest
    MotorModelTest.cpp
    ${CMAKE_SOURCE_DIR}/src/drive/MotorModel.cpp
)
target_include_directories(MotorModelTest PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(MotorModelTest PRIVATE GTest::gtest_main)
gtest_discover_tests(MotorModelTest)

# CyclicThread tests
add_executable(CyclicThreadTest
    CyclicThreadTest.cpp
    ${CMAKE_SOURCE_DIR}/src/rt/CyclicThread.cpp
)
target_include_directories(CyclicThreadTest PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(CyclicThreadTest PRIVATE GTest::gtest_main Threads::Threads rt)
gtest_discover_tests(CyclicThreadTest)
