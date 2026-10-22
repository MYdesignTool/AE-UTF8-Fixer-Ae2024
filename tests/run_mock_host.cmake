# Isolate each case's INI beside its own bridge DLL, including under parallel CTest.
file(MAKE_DIRECTORY "${CASE_DIRECTORY}")
get_filename_component(bridge_name "${BRIDGE}" NAME)
configure_file("${BRIDGE}" "${CASE_DIRECTORY}/${bridge_name}" COPYONLY)
set(ini "${CASE_DIRECTORY}/AE-Locale-Bridge.ini")
if(NO_INI)
    file(REMOVE "${ini}")
else()
    file(WRITE "${ini}" "[Bridge]\nEnabled=1\nOnlyEnglish=${ONLY_ENGLISH}\nSourceEncoding=auto\nLegacyBridge=${BRIDGE_MODE}\n")
endif()
execute_process(COMMAND "${HOST}" "${CASE_DIRECTORY}/${bridge_name}" "${LEGACY_EFFECT}"
    "${LANGUAGE}" "${ACTIVE}" "${LEGACY}" "${CODE_PAGE}" "${LANGUAGE_STATUS}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
message("${output}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Mock host failed (${result}): ${error}")
endif()
