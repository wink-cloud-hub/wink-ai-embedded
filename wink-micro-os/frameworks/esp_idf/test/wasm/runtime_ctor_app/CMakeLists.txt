set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
set(WINK_APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/global_resources.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/h4_same_time.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/phase4_restart.cpp"
    PARENT_SCOPE)
