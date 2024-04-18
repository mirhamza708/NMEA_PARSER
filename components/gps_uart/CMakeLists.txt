idf_component_register(SRCS "gps_uart.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver 
                    PRIV_REQUIRES gps_parser)
