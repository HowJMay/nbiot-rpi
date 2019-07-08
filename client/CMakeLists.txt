include_directories(${mosquitto_SOURCE_DIR} ${mosquitto_SOURCE_DIR}/lib
			${STDBOOL_H_PATH} ${STDINT_H_PATH} ${PTHREAD_INCLUDE_DIR}
			${OPENSSL_INCLUDE_DIR})
link_directories(${mosquitto_BINARY_DIR}/lib)
add_executable(mosquitto_sub sub_client.c sub_client_output.c ${shared_src})
