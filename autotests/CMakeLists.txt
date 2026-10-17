ecm_add_test(sanitytest.cpp
    TEST_NAME sanitytest
    LINK_LIBRARIES Qt6::Core Qt6::Test
)

ecm_add_test(configtest.cpp
    TEST_NAME configtest
    LINK_LIBRARIES Qt6::Core Qt6::Test vdwcore
)

# Skips unless run inside the nested session:
#   scripts/dev-session.sh run -- ctest --preset dev -R integrationtest
ecm_add_test(integrationtest.cpp
    TEST_NAME integrationtest
    LINK_LIBRARIES Qt6::Core Qt6::DBus Qt6::Test
)
target_compile_definitions(integrationtest PRIVATE
    VDWD_PATH="$<TARGET_FILE:vdwd>"
    VDWCTL_PATH="$<TARGET_FILE:vdwctl>"
)
add_dependencies(integrationtest vdwd vdwctl)
