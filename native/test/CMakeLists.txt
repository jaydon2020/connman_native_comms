find_package(GTest REQUIRED)

add_executable(test_types test_types.cpp)
target_include_directories(test_types PRIVATE ../include)
target_link_libraries(test_types PRIVATE GTest::gtest_main)

add_executable(test_connman_manager test_connman_manager.cpp)
target_include_directories(test_connman_manager PRIVATE ../include ../generated)
target_link_libraries(test_connman_manager PRIVATE 
    connman_core_obj 
    dart_api_dl
    sdbus-c++ 
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_types)
gtest_discover_tests(test_connman_manager)