# Client library SO version. Bump if incompatible API/ABI changes are made.
SOVERSION=1

sub_single : single.o
	${CROSS_COMPILE}${CC} $^ -o $@ ../third_party/mosquitto/lib/libmosquitto.so.${SOVERSION}

single.o : single.c ../third_party/mosquitto/lib/libmosquitto.so.${SOVERSION}
	${CROSS_COMPILE}${CC} -c $< -o $@ -I../third_party/mosquitto/lib ${CFLAGS}

../third_party/mosquitto/lib/libmosquitto.so.${SOVERSION} :
	$(MAKE) -C ../third_party/mosquitto/lib