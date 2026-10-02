find_package(${HIPE_QT}Core QUIET)

if (HIPE_QT STREQUAL "Qt6")
    set(_qmake_executable_default "qmake6")
else ()
    set(_qmake_executable_default "qmake-qt5")
endif ()
if (TARGET ${HIPE_QT}::qmake)
    get_target_property(_qmake_executable_default ${HIPE_QT}::qmake LOCATION)
endif()
set(QMAKE_EXECUTABLE ${_qmake_executable_default}
    CACHE FILEPATH "Location of the qmake executable of the Qt in use")

# This is not public API (yet)!
function(query_qmake result_variable qt_variable)
    execute_process(
        COMMAND ${QMAKE_EXECUTABLE} -query "${qt_variable}"
        RESULT_VARIABLE return_code
        OUTPUT_VARIABLE output
    )
    if(return_code EQUAL 0)
        string(STRIP "${output}" output)
        file(TO_CMAKE_PATH "${output}" output_path)
        set(${result_variable} "${output_path}" PARENT_SCOPE)
    else()
        message(WARNING "Failed call: ${QMAKE_EXECUTABLE} -query \"${qt_variable}\"")
        message(FATAL_ERROR "QMake call failed: ${return_code}")
    endif()
endfunction()
