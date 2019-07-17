#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include <mosquitto.h>
#include <mqtt_protocol.h>
#include "client_common.h"
#include "pub_utils.h"
#include "sub_utils.h"

void publish_callback_parser_func(struct mosquitto *mosq, void *obj, int mid, int reason_code,
                                  const mosquitto_property *properties);
void message_callback_parser_func(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message,
                                  const mosquitto_property *properties);
void connect_callback_parser_func(struct mosquitto *mosq, void *obj, int result, int flags,
                                  const mosquitto_property *properties);
void subscribe_callback_parser_func(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos);
void disconnect_callback_parser_func(struct mosquitto *mosq, void *obj, mosq_retcode_t ret,
                                     const mosquitto_property *properties);
void log_callback_parser_func(struct mosquitto *mosq, void *obj, int level, const char *str);
rc_mosq_retcode_t parser_config_init(struct mosquitto **config_mosq, mosq_config_t *config_cfg);
rc_mosq_retcode_t gossip_channel_setting(mosq_config_t *channel_cfg, char *host, char *sub_topic, char *pub_topic);
rc_mosq_retcode_t gossip_message_set(mosq_config_t *channel_cfg, char *message);
rc_mosq_retcode_t parser_loop(struct mosquitto *loop_mosq, mosq_config_t *loop_cfg);
#endif