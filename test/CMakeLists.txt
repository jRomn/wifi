idf_component_register(
    SRCS "test_wifi.c" "test_adc.c"
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES unity adc
)
