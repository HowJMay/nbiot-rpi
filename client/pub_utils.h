#ifndef PUB_UTILS_H
#define PUB_UTILS_H

#include <mosquitto.h>
#include <mqtt_protocol.h>
#include "client_shared.h"

/* Global variables for use in callbacks. See sub_client.c for an example of
 * using a struct to hold variables for use in callbacks. */
#define STATUS_CONNECTING 0
#define STATUS_CONNACK_RECVD 1
#define STATUS_WAITING 2
#define STATUS_DISCONNECTING 3

extern int mid_sent;
extern int status;
extern struct mosq_config cfg;

void set_repeat_time(void);
int check_repeat_time(void);
int publish_message(struct mosquitto *mosq, int *mid, const char *topic, int payloadlen, void *payload, int qos,
                    bool retain);
void log_callback_func(struct mosquitto *mosq, void *obj, int level, const char *str);
void disconnect_callback_func(struct mosquitto *mosq, void *obj, mosq_retcode_t ret,
                              const mosquitto_property *properties);
void connect_callback_func(struct mosquitto *mosq, void *obj, int result, int flags,
                           const mosquitto_property *properties);
void publish_callback_func(struct mosquitto *mosq, void *obj, int mid, int reason_code,
                           const mosquitto_property *properties);
int pub_shared_init(void);
int publish_loop(struct mosquitto *mosq);
void pub_shared_cleanup(void);
mosq_retcode_t init_check_error(struct mosq_config *cfg, int pub_or_sub);

#endif
