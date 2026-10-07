idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES detector_core board_cyd35 display_st7796
)
