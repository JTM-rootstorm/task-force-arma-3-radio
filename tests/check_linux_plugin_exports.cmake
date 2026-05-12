if(NOT DEFINED TFAR_PLUGIN_PATH)
    message(FATAL_ERROR "TFAR_PLUGIN_PATH is required")
endif()

execute_process(
    COMMAND nm -D ${TFAR_PLUGIN_PATH}
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE nm_output
    ERROR_VARIABLE nm_error
)

if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "nm failed for ${TFAR_PLUGIN_PATH}: ${nm_error}")
endif()

set(required_exports
    ts3plugin_apiVersion
    ts3plugin_init
    ts3plugin_onEditCapturedVoiceDataEvent
    ts3plugin_onEditPostProcessVoiceDataEvent
    ts3plugin_onPluginCommandEvent
    ts3plugin_onTalkStatusChangeEvent
    ts3plugin_setFunctionPointers
)

foreach(export_name IN LISTS required_exports)
    if(NOT nm_output MATCHES "[\n ]T ${export_name}([\n]|$)")
        message(FATAL_ERROR "Missing Linux TeamSpeak plugin export: ${export_name}")
    endif()
endforeach()
