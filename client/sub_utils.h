#ifndef SUB_UTILS_H
#define SUB_UTILS_H

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mosquitto.h>
#include <mqtt_protocol.h>
#include "client_shared.h"
#include "sub_utils.h"

extern struct mosq_config cfg;
extern bool process_messages;
extern int msg_count;
extern struct mosquitto *mosq;
extern int last_mid;

void signal_handler_func(int signum);
void publish_callback_func(struct mosquitto *mosq, void *obj, int mid, int reason_code,
                           const mosquitto_property *properties);
void message_callback_func(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message,
                           const mosquitto_property *properties);
void connect_callback_func(struct mosquitto *mosq, void *obj, int result, int flags,
                           const mosquitto_property *properties);
void subscribe_callback_func(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos);
void log_callback_func(struct mosquitto *mosq, void *obj, int level, const char *str);

#endif