ecm_add_test(sanitytest.cpp
    TEST_NAME sanitytest
    LINK_LIBRARIES Qt6::Core Qt6::Test
)

ecm_add_test(configtest.cpp
    TEST_NAME configtest
    LINK_LIBRARIES Qt6::Core Qt6::Test vdwcore
)

ecm_add_test(configwritertest.cpp
    TEST_NAME configwritertest
    LINK_LIBRARIES Qt6::Core Qt6::Test vdwcore
)
# The test uses plain string literals for brevity.
target_compile_options(configwritertest PRIVATE -UQT_NO_CAST_FROM_ASCII -UQT_NO_CAST_TO_ASCII)

ecm_add_test(layouttest.cpp
    TEST_NAME layouttest
    LINK_LIBRARIES Qt6::Core Qt6::Test vdwcore
)

ecm_add_test(execargstest.cpp
    TEST_NAME execargstest
    LINK_LIBRARIES Qt6::Core Qt6::Test vdwcore
)

ecm_add_test(matchertest.cpp
    TEST_NAME matchertest
    LINK_LIBRARIES Qt6::Core Qt6::Test vdwcore
)

ecm_add_test(shortcutkeystest.cpp
    TEST_NAME shortcutkeystest
    LINK_LIBRARIES Qt6::Core Qt6::Test vdwcore
)

# Skips unless run inside the nested session:
#   scripts/dev-session.sh run -- ctest --preset dev -R integrationtest
ecm_add_test(integrationtest.cpp
    TEST_NAME integrationtest
    LINK_LIBRARIES Qt6::Core Qt6::DBus Qt6::Test vdwcore
)
target_compile_definitions(integrationtest PRIVATE
    VDWD_PATH="$<TARGET_FILE:vdwd>"
    VDWUI_PATH="$<TARGET_FILE:vdw-ui>"
    VDWCTL_PATH="$<TARGET_FILE:vdwctl>"
    S1INJECT_PATH="${CMAKE_SOURCE_DIR}/spikes/S1/build/s1inject"
    S2FAKEINPUT_PATH="${CMAKE_SOURCE_DIR}/spikes/S2/build/s2-fakeinput"
)
add_dependencies(integrationtest vdwd vdw-ui vdwctl)
