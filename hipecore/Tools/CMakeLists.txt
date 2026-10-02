WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (WIN32)
    if (NOT "${PORT}" STREQUAL "Qt")
        add_subdirectory(TestWebKitAPI)
    endif ()
endif ()
