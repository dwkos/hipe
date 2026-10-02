set(TESTWEBKITAPI_DIR "${TOOLS_DIR}/TestWebKitAPI")
set(test_wtf_LIBRARIES
    WTF${DEBUG_SUFFIX}
    gtest
)

set(test_webcore_LIBRARIES
    WTF${DEBUG_SUFFIX}
    WebCore${DEBUG_SUFFIX}
    gtest
)

set(TestWebKitAPI_LIBRARIES
    WTF${DEBUG_SUFFIX}
)

list(APPEND TestWebKitAPI_LIBRARIES
    WebKit${DEBUG_SUFFIX}
)

set(TestWTF_SOURCES
    ${TESTWEBKITAPI_DIR}/Counters.cpp
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/AtomicString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/CString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Condition.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/CheckedArithmeticOperations.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/DateMath.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Deque.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Functional.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/HashCountedSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/HashMap.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/HashSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/IntegerToStringConversion.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/ListHashSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Lock.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MD5.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MathExtras.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MediaTime.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MetaAllocator.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/NakedPtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/OptionSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/ParkingLot.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/RedBlackTree.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Ref.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/RefPtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/SHA1.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/SaturatedArithmeticOperations.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringBuilder.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringHasher.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringImpl.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringOperators.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringView.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/TemporaryChange.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Vector.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/WTFString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/WorkQueue.cpp
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

include_directories(
    ${TESTWEBKITAPI_DIR}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${DERIVED_SOURCES_WEBCORE_DIR}
    ${THIRDPARTY_DIR}/gtest/include
    ${WEBCORE_DIR}/css
    ${WEBCORE_DIR}/dom
    ${WEBCORE_DIR}/editing
    ${WEBCORE_DIR}/loader/cache
    ${WEBCORE_DIR}/platform
    ${WEBCORE_DIR}/platform/animation
    ${WEBCORE_DIR}/platform/graphics
    ${WEBCORE_DIR}/platform/text
    ${WEBCORE_DIR}/platform/network
    ${WEBCORE_DIR}/rendering/style
    ${WTF_DIR}
)

if (WIN32)
    add_definitions(-DGTEST_LINKED_AS_SHARED_LIBRARY=0)
else ()
    add_definitions(-DGTEST_LINKED_AS_SHARED_LIBRARY=1)
endif ()

add_definitions(-DGTEST_HAS_RTTI=0)

# FIXME: This works around compatibility problems in the old version of the third-pary
# googletest source code checkout. It should be removed once we upgrade to a newer version.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions(-DGTEST_HAS_TR1_TUPLE=0)
endif ()

add_executable(TestWTF
    ${test_main_SOURCES}
    ${TestWTF_SOURCES}
)

if (WIN32)
    add_dependencies(TestWTF TestWTFLib)
endif ()

target_link_libraries(TestWTF ${test_wtf_LIBRARIES})
add_dependencies(TestWTF WTF ${ForwardingHeadersForTestWebKitAPI_NAME} ${ForwardingNetworkHeadersForTestWebKitAPI_NAME})
add_test(TestWTF ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF}/TestWTF)
set_tests_properties(TestWTF PROPERTIES TIMEOUT 240)
set_target_properties(TestWTF PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF}
)

# TestWebCore is defined in PlatformQt.cmake.
