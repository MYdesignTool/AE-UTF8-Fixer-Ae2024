file(MAKE_DIRECTORY "${CASE_DIRECTORY}")
get_filename_component(bridge_name "${BRIDGE}" NAME)
configure_file("${BRIDGE}" "${CASE_DIRECTORY}/${bridge_name}" COPYONLY)
file(WRITE "${CASE_DIRECTORY}/AE-Locale-Bridge.ini" "[Bridge]\nOnlyEnglish=0\nProjectTextBridge=${PROJECT_BRIDGE}\nProjectDiagnostics=1\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "AE_BRIDGE_TEST_OPEN_STYLE=${OPEN_STYLE}"
    "${HOST}" "${CASE_DIRECTORY}/${bridge_name}" "${CASE_DIRECTORY}" "${ACTIVE}" "${LANGUAGE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
message("${output}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Missing host failed (${result}): ${error}")
endif()
