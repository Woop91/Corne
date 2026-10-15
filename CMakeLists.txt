# The stock widget is disabled only for the custom left-hand display.
if(CONFIG_CORNE_BATTERY_DISPLAY)
  set(nice_view_widgets ${APPLICATION_SOURCE_DIR}/boards/shields/nice_view/widgets)
  target_include_directories(app PRIVATE ${nice_view_widgets})
  target_sources(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/status.c
    ${CMAKE_CURRENT_LIST_DIR}/src/custom_status_screen.c
    ${nice_view_widgets}/util.c
    ${nice_view_widgets}/bolt.c
  )
endif()
