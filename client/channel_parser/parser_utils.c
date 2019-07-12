#include "parser_utils.h"
#include <errno.h>
#include <string.h>
#include "client_common.h"
#include "mosquitto_internal.h"

void publish_callback_parser_func(struct mosquitto *mosq, void *obj, int mid, int reason_code,
                                  const mosquitto_property *properties) {
  publish_callback_pub_func(mosq, obj, mid, reason_code, properties);
  printf("publish_callback_parser_func \n");
}

void message_callback_parser_func(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message,
                                  const mosquitto_property *properties) {
  message_callback_sub_func(mosq, obj, message, properties);
  mosquitto_disconnect_v5(mosq, 0, cfg.property_config->disconnect_props);
  printf("message_callback_parser_func \n");
}

void connect_callback_parser_func(struct mosquitto *mosq, void *obj, int result, int flags,
                                  const mosquitto_property *properties) {
  mosq_retcode_t ret = MOSQ_ERR_SUCCESS;
  if (cfg.general_config->client_type == client_pub) {
    connect_callback_pub_func(mosq, obj, result, flags, properties);
  } else if (cfg.general_config->client_type == client_sub) {
    connect_callback_sub_func(mosq, obj, result, flags, properties);
  }

  printf("connect_callback_parser_func \n");
}

void disconnect_callback_parser_func(struct mosquitto *mosq, void *obj, mosq_retcode_t ret,
                                     const mosquitto_property *properties) {
  if (cfg.general_config->client_type == client_pub) {
    disconnect_callback_parser_func(mosq, obj, ret, properties);
  } else if (cfg.general_config->client_type == client_sub) {
  }

  fprintf(stdout, "Publisher disconnect pub callback.\n");
}

void subscribe_callback_parser_func(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos) {
  subscribe_callback_sub_func(mosq, obj, mid, qos_count, granted_qos);
  printf("subscribe_callback_parser_func \n");
}

void log_callback_parser_func(struct mosquitto *mosq, void *obj, int level, const char *str) { printf("%s\n", str); }

rc_mosq_retcode_t parser_config_init(struct mosquitto **config_mosq, struct mosq_config *config_cfg) {
  rc_mosq_retcode_t ret = RC_MOS_OK;

  init_mosq_config(config_cfg, client_duplex);
  mosquitto_lib_init();
  if (pub_shared_init()) {
    return RC_MOS_CRASH;
  }

  if (generate_client_id(config_cfg)) {
    return RC_MOS_INIT_ERROR;
  }

  init_check_error(config_cfg, client_pub);

  *config_mosq = mosquitto_new(config_cfg->general_config->id, true, NULL);
  if (!mosq) {
    switch (errno) {
      case ENOMEM:
        if (!config_cfg->general_config->quiet) fprintf(stderr, "Error: Out of memory.\n");
        break;
      case EINVAL:
        if (!config_cfg->general_config->quiet) fprintf(stderr, "Error: Invalid id.\n");
        break;
    }
    return RC_MOS_INIT_ERROR;
  }

  if (mosq_opts_set(*config_mosq, config_cfg)) {
    return RC_MOS_INIT_ERROR;
  }
}

rc_mosq_retcode_t gossip_channel_setting(struct mosq_config *channel_cfg, char *host, char *sub_topic,
                                         char *pub_topic) {
  rc_mosq_retcode_t ret = RC_MOS_OK;

  channel_cfg->general_config->host = strdup(host);
  channel_cfg->general_config->client_type = client_pub;
  if (cfg_add_topic(channel_cfg, client_sub, sub_topic)) {
    ret = RC_MOS_CHANNEL_SETTING;
    goto done;
  }
  if (cfg_add_topic(channel_cfg, client_pub, pub_topic)) {
    ret = RC_MOS_CHANNEL_SETTING;
    goto done;
  }

done:
  return ret;
}

rc_mosq_retcode_t gossip_message_set(struct mosq_config *channel_cfg, char *message) {
  rc_mosq_retcode_t ret = RC_MOS_OK;

  if (channel_cfg->pub_config->pub_mode != MSGMODE_NONE) {
    fprintf(stderr, "Error: Only one type of message can be sent at once.\n\n");
    ret = RC_MOS_MESSAGE_SETTING;
  } else {
    channel_cfg->pub_config->message = strdup(message);
    channel_cfg->pub_config->msglen = strlen(channel_cfg->pub_config->message);
    channel_cfg->pub_config->pub_mode = MSGMODE_CMD;
  }

  return ret;
}

rc_mosq_retcode_t parser_loop(struct mosquitto *loop_mosq, struct mosq_config *loop_cfg) {
  rc_mosq_retcode_t ret = MOSQ_ERR_SUCCESS;

  loop_cfg->general_config->client_type = client_sub;
  ret = mosq_client_connect(loop_mosq, loop_cfg);
  if (ret) {
    goto done;
  }

  ret = mosquitto_loop_forever(loop_mosq, -1, 1);
  if (ret) {
    goto done;
  }

  loop_cfg->general_config->client_type = client_pub;
  ret = mosq_client_connect(loop_mosq, loop_cfg);
  if (ret) {
    goto done;
  }
  ret = publish_loop(loop_mosq);

done:
  return ret;
}