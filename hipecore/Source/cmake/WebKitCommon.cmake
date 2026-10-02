# -----------------------------------------------------------------------------
# This file is included individually from various subdirectories (JSC, WTF,
# WebCore, WebKit) in order to allow scripts to build only part of WebKit.
# We want to run this file only once.
# -----------------------------------------------------------------------------
if (NOT HAS_RUN_WEBKIT_COMMON)
    set(HAS_RUN_WEBKIT_COMMON TRUE)

    # -----------------------------------------------------------------------------
    # Find common packages (used by all ports)
    # -----------------------------------------------------------------------------
    if (WIN32)
        list(APPEND CMAKE_PROGRAM_PATH $ENV{SystemDrive}/cygwin/bin)
    endif ()

    # bison builds the CSS and XPath grammars. flex and ruby are not used by hipecore:
    # there are no lexer (.l) inputs, and Ruby was only ever needed for JavaScriptCore's
    # offlineasm assembler, which is gone with the JS engine.
    find_package(BISON 2.1 REQUIRED)

    # TODO Enforce version requirement for gperf
    find_package(Gperf 3.0.1 REQUIRED)

    # TODO Enforce version requirement for perl
    find_package(Perl 5.10.0 REQUIRED)

    set(Python_ADDITIONAL_VERSIONS 3)
    find_package(PythonInterp 2.7.0 REQUIRED)

    # -----------------------------------------------------------------------------
    # Helper macros and feature defines
    # -----------------------------------------------------------------------------

    # To prevent multiple inclusion, most modules should be included once here.
    include(CheckFunctionExists)

    include(WebKitMacros)
    include(WebKitFS)
    include(WebKitHelpers)
    include(WebKitFeatures)

    include(OptionsCommon)
    include(Options${PORT})

    # -----------------------------------------------------------------------------
    # config.h
    # -----------------------------------------------------------------------------

    CREATE_CONFIGURATION_HEADER()
endif ()
