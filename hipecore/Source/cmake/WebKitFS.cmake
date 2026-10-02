if (NOT BMALLOC_DIR)
    set(BMALLOC_DIR "${CMAKE_SOURCE_DIR}/Source/bmalloc")
endif ()
if (NOT WTF_DIR)
    set(WTF_DIR "${CMAKE_SOURCE_DIR}/Source/WTF")
endif ()
# hipecore: JavaScriptCore was removed from the tree (Bucket 5.7). JAVASCRIPTCORE_DIR
# / DERIVED_SOURCES_JAVASCRIPTCORE_DIR are kept defined only so the few remaining
# vestigial references (ThirdParty/gtest, the macOS-system-ICU branch) still expand
# to a harmless non-existent path rather than an empty string.
if (NOT JAVASCRIPTCORE_DIR)
    set(JAVASCRIPTCORE_DIR "${CMAKE_SOURCE_DIR}/Source/JavaScriptCore")
endif ()
if (NOT WEBCORE_DIR)
    set(WEBCORE_DIR "${CMAKE_SOURCE_DIR}/Source/WebCore")
endif ()
if (NOT WEBKIT_DIR)
    set(WEBKIT_DIR "${CMAKE_SOURCE_DIR}/Source/WebKit")
endif ()
if (NOT THIRDPARTY_DIR)
    set(THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/Source/ThirdParty")
endif ()
if (NOT TOOLS_DIR)
    set(TOOLS_DIR "${CMAKE_SOURCE_DIR}/Tools")
endif ()

set(DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources")
set(DERIVED_SOURCES_JAVASCRIPTCORE_DIR "${CMAKE_BINARY_DIR}/DerivedSources/JavaScriptCore")
set(DERIVED_SOURCES_WEBCORE_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebCore")
set(DERIVED_SOURCES_WEBKITLEGACY_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKitLegacy")
set(DERIVED_SOURCES_WEBKIT_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKit")

set(FORWARDING_HEADERS_DIR ${DERIVED_SOURCES_DIR}/ForwardingHeaders)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR})

if (ENABLE_WEBKIT)
    file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKITLEGACY_DIR})
    file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT_DIR})
endif ()
