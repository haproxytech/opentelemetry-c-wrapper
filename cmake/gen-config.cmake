# Function to generate config.h from CMakeLists.txt automatically
#
function(generate_config_from_cmakefile)
    set(_cmfile "${CMAKE_SOURCE_DIR}/CMakeLists.txt")
    set(_cfgin  "${CMAKE_BINARY_DIR}/config.h.in")
    file(WRITE "${_cfgin}"
        "/* Auto-generated config.h.in */\n\n"
        "#ifndef _COMMON_CONFIG_H\n"
        "#define _COMMON_CONFIG_H\n\n")

    file(READ "${_cmfile}" _contents)
    string(REPLACE "\r\n" "\n" _contents "${_contents}")  # normalize line endings
    string(REPLACE "\r" "\n" _contents "${_contents}")
    string(REPLACE ";" "\\;" _contents "${_contents}")    # escape list separators

    set(_seen "")

    # Extract all option() commands -> #cmakedefine VAR 1
    string(REGEX MATCHALL "option\\([ \t]*[A-Za-z0-9_]+" _matches "${_contents}")
    foreach(_m IN LISTS _matches)
        string(REGEX REPLACE "^option\\([ \t]*" "" _name "${_m}")
        if(NOT _name IN_LIST _seen)
            list(APPEND _seen "${_name}")
            file(APPEND "${_cfgin}" "#cmakedefine ${_name} 1\n")
        endif()
    endforeach()

    # Extract check_include_file(header HAVE_VAR) -> #cmakedefine HAVE_VAR 1
    string(REGEX MATCHALL "check_include_file\\([^ \t)]+[ \t]+[A-Za-z0-9_]+" _matches "${_contents}")
    foreach(_m IN LISTS _matches)
        string(REGEX REPLACE ".*[ \t]" "" _name "${_m}")
        if(NOT _name IN_LIST _seen)
            list(APPEND _seen "${_name}")
            file(APPEND "${_cfgin}" "#cmakedefine ${_name} 1\n")
        endif()
    endforeach()

    # Extract check_function_exists(func HAVE_VAR) -> #cmakedefine HAVE_VAR 1
    string(REGEX MATCHALL "check_function_exists\\([^ \t)]+[ \t]+[A-Za-z0-9_]+" _matches "${_contents}")
    foreach(_m IN LISTS _matches)
        string(REGEX REPLACE ".*[ \t]" "" _name "${_m}")
        if(NOT _name IN_LIST _seen)
            list(APPEND _seen "${_name}")
            file(APPEND "${_cfgin}" "#cmakedefine ${_name} 1\n")
        endif()
    endforeach()

    # Extract check_symbol_exists(sym "headers" HAVE_VAR) -> #cmakedefine HAVE_VAR 1
    string(REGEX MATCHALL "check_symbol_exists\\([^\n)]+\\)" _matches "${_contents}")
    foreach(_m IN LISTS _matches)
        string(REGEX REPLACE "[ \t]*\\)$" "" _stripped "${_m}")
        string(REGEX REPLACE ".*[ \t]" "" _name "${_stripped}")
        if(NOT _name IN_LIST _seen)
            list(APPEND _seen "${_name}")
            file(APPEND "${_cfgin}" "#cmakedefine ${_name} 1\n")
        endif()
    endforeach()

    # Extract check_c_source_compiles("..." VAR) and
    # check_cxx_source_compiles("..." VAR) result variables.
    # Multi-line calls: [^"]* spans newlines in negated character classes.
    string(REGEX MATCHALL
        "check_c(xx)?_source_compiles\\(\"[^\"]*\"[ \t]+[A-Za-z0-9_]+"
        _matches "${_contents}")
    foreach(_m IN LISTS _matches)
        string(REGEX REPLACE ".*[ \t]" "" _name "${_m}")
        if(NOT _name IN_LIST _seen)
            list(APPEND _seen "${_name}")
            file(APPEND "${_cfgin}" "#cmakedefine ${_name} 1\n")
        endif()
    endforeach()

    # Extract probe_exporter(NAME ...) -> #cmakedefine HAVE_OTEL_EXPORTER_NAME 1
    string(REGEX MATCHALL "probe_exporter\\([ \t]*[A-Za-z0-9_]+" _matches "${_contents}")
    foreach(_m IN LISTS _matches)
        string(REGEX REPLACE "^probe_exporter\\([ \t]*" "" _name "${_m}")
        set(_varname "HAVE_OTEL_EXPORTER_${_name}")
        if(NOT _varname IN_LIST _seen)
            list(APPEND _seen "${_varname}")
            file(APPEND "${_cfgin}" "#cmakedefine ${_varname} 1\n")
        endif()
    endforeach()

    # Extract set(VAR <integer>) without CACHE -> #cmakedefine VAR 1
    string(REGEX MATCHALL "set\\([ \t]*[A-Za-z0-9_]+[ \t]+[0-9]+[ \t]*\\)" _matches "${_contents}")
    foreach(_m IN LISTS _matches)
        string(REGEX REPLACE "^set\\([ \t]*" "" _rest "${_m}")
        string(REGEX REPLACE "[ \t].*" "" _name "${_rest}")
        if((NOT _name MATCHES "^CMAKE_") AND (NOT _name IN_LIST _seen))
            list(APPEND _seen "${_name}")
            file(APPEND "${_cfgin}" "#cmakedefine ${_name} 1\n")
        endif()
    endforeach()

    # Extract set(VAR "string") for package/version metadata -> #cmakedefine VAR "@VAR@"
    string(REGEX MATCHALL
        "set\\([ \t]*(PACKAGE[A-Za-z0-9_]*|VERSION)[ \t]+\"[^\"]*\"[ \t]*\\)"
        _matches "${_contents}")
    foreach(_m IN LISTS _matches)
        string(REGEX REPLACE "^set\\([ \t]*" "" _rest "${_m}")
        string(REGEX REPLACE "[ \t].*" "" _name "${_rest}")
        if(NOT _name IN_LIST _seen)
            list(APPEND _seen "${_name}")
            file(APPEND "${_cfgin}" "#cmakedefine ${_name} \"@${_name}@\"\n")
        endif()
    endforeach()

    file(APPEND "${_cfgin}" "\n#endif /* _COMMON_CONFIG_H */\n")

    # Generate config.h from the .in template
    configure_file("${_cfgin}" "${CMAKE_BINARY_DIR}/config.h" @ONLY)
endfunction()
