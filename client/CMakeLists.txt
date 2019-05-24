include_directories(../third_party/mosquitto)
include_directories(../third_party/mosquitto/lib)
include_directories(common)

set(shared_src common/client_common.c common/client_common.h)
set(sub_shared sub_client/sub_utils.c sub_client/sub_utils.h)
set(pub_shared pub_client/pub_utils.c pub_client/pub_utils.h)
set(duplex_shared duplex_client/duplex_utils.c duplex_client/duplex_utils.h duplex_client/duplex_callback.c duplex_client/duplex_callback.h)
set(mos_lib_loc ../third_party/mosquitto/lib/libmosquitto.so.1)

add_library(mos_lib SHARED IMPORTED)
set_property(TARGET mos_lib PROPERTY IMPORTED_LOCATION ${mos_lib_loc})

add_executable(sub_client sub_client/sub_client.c ${shared_src} ${sub_shared})
add_executable(pub_client pub_client/pub_client.c ${shared_src} ${pub_shared})
target_link_libraries(sub_client mos_lib)
target_link_libraries(pub_client mos_lib)

#ADD_DEFINITIONS(-DWITH_TLS)

include_directories(sub_client)
include_directories(pub_client)
add_executable(duplex duplex_client/duplex_client.c ${duplex_shared} ${shared_src} ${pub_shared} ${sub_shared})
target_link_libraries(duplex mos_lib)