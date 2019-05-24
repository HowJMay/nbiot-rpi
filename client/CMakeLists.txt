include_directories(../third_party/mosquitto)
include_directories(../third_party/mosquitto/lib)

set(shared_src client_props.c client_shared.c client_shared.h)
set(sub_shared sub_utils.c sub_utils.h)
set(pub_shared pub_utils.c pub_utils.h)
set(mos_lib_loc ../third_party/mosquitto/lib/libmosquitto.so.1)

add_library(mos_lib SHARED IMPORTED)
set_property(TARGET mos_lib PROPERTY IMPORTED_LOCATION ${mos_lib_loc})

add_executable(sub_client sub_client.c ${shared_src} ${sub_shared})
add_executable(publish_client pub_client.c ${shared_src} ${pub_shared})
target_link_libraries(sub_client mos_lib)
target_link_libraries(publish_client mos_lib)
