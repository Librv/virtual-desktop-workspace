add_library(vdwgesture STATIC gesturemachine.cpp)
target_include_directories(vdwgesture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vdwgesture PUBLIC Qt6::Core)

if(BUILD_TESTING)
    ecm_add_test(tst_gesturemachine.cpp
        TEST_NAME tst_gesturemachine
        LINK_LIBRARIES vdwgesture Qt6::Test)
endif()
