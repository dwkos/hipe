remove_definitions(-DQT_ASCII_CAST_WARNINGS)

if (ENABLE_API_TESTS)
    add_subdirectory(TestWebKitAPI)
endif ()
