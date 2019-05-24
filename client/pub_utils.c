#include "pub_utils.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "config.h"

/* Global variables for use in callbacks. See sub_client.c for an example of
 * using a struct to hold variables for use in callbacks. */
int mid_sent = 0;
int status = STATUS_CONNECTING;
struct mosq_config cfg;

static bool first_publish = true;
static int last_mid = -1;
static int last_mid_sent = -1;
static char *line_buf = NULL;
static int line_buf_len = 1024;
static bool connected = true;
static bool disconnect_sent = false;
static int publish_count = 0;
static bool ready_for_repeat = false;
static struct timeval next_publish_tv;

void set_repeat_time(void) {
  gettimeofday(&next_publish_tv, NULL);
  next_publish_tv.tv_sec += cfg.repeat_delay.tv_sec;
  next_publish_tv.tv_usec += cfg.repeat_delay.tv_usec;

  next_publish_tv.tv_sec += next_publish_tv.tv_usec / 1e6;
  next_publish_tv.tv_usec = next_publish_tv.tv_usec % 1000000;
}

int check_repeat_time(void) {
  struct timeval tv;

  gettimeofday(&tv, NULL);

  if (tv.tv_sec > next_publish_tv.tv_sec) {
    return EXIT_FAILURE;
  } else if (tv.tv_sec == next_publish_tv.tv_sec && tv.tv_usec > next_publish_tv.tv_usec) {
    return EXIT_FAILURE;
  }
  return 0;
}

int publish_message(struct mosquitto *mosq, int *mid, const char *topic, int payloadlen, void *payload, int qos,
                    bool retain) {
  ready_for_repeat = false;
  if (cfg.protocol_version == MQTT_PROTOCOL_V5 && cfg.have_topic_alias && first_publish == false) {
    return mosquitto_publish_v5(mosq, mid, NULL, payloadlen, payload, qos, retain, cfg.publish_props);
  } else {
    first_publish = false;
    return mosquitto_publish_v5(mosq, mid, topic, payloadlen, payload, qos, retain, cfg.publish_props);
  }
}

void log_callback_func(struct mosquitto *mosq, void *obj, int level, const char *str) {
  UNUSED(mosq);
  UNUSED(obj);
  UNUSED(level);

  printf("log: [%s]\n", str);
}

void disconnect_callback_func(struct mosquitto *mosq, void *obj, mosq_retcode_t ret,
                              const mosquitto_property *properties) {
  UNUSED(mosq);
  UNUSED(obj);
  UNUSED(ret);
  UNUSED(properties);

  connected = false;
  fprintf(stdout, "Publisher disconnect callback.\n");
}

void connect_callback_func(struct mosquitto *mosq, void *obj, int result, int flags,
                           const mosquitto_property *properties) {
  mosq_retcode_t ret = MOSQ_ERR_SUCCESS;

  UNUSED(obj);
  UNUSED(flags);
  UNUSED(properties);

  if (!result) {
    switch (cfg.pub_mode) {
      case MSGMODE_CMD:
      case MSGMODE_FILE:
      case MSGMODE_STDIN_FILE:
        printf("callbakc \n");
        ret = publish_message(mosq, &mid_sent, cfg.topic, cfg.msglen, cfg.message, cfg.qos, cfg.retain);
        break;
      case MSGMODE_NULL:
        ret = publish_message(mosq, &mid_sent, cfg.topic, 0, NULL, cfg.qos, cfg.retain);
        break;
      case MSGMODE_STDIN_LINE:
        status = STATUS_CONNACK_RECVD;
        break;
    }
    if (ret) {
      if (!cfg.quiet) {
        switch (ret) {
          case MOSQ_ERR_INVAL:
            fprintf(stderr, "Error: Invalid input. Does your topic contain '+' or '#'?\n");
            break;
          case MOSQ_ERR_NOMEM:
            fprintf(stderr, "Error: Out of memory when trying to publish message.\n");
            break;
          case MOSQ_ERR_NO_CONN:
            fprintf(stderr, "Error: Client not connected when trying to publish.\n");
            break;
          case MOSQ_ERR_PROTOCOL:
            fprintf(stderr, "Error: Protocol error when communicating with broker.\n");
            break;
          case MOSQ_ERR_PAYLOAD_SIZE:
            fprintf(stderr, "Error: Message payload is too large.\n");
            break;
          case MOSQ_ERR_QOS_NOT_SUPPORTED:
            fprintf(stderr, "Error: Message QoS not supported on broker, try a lower QoS.\n");
            break;
        }
      }
      mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
    }
  } else {
    if (result && !cfg.quiet) {
      if (cfg.protocol_version == MQTT_PROTOCOL_V5) {
        fprintf(stderr, "%s\n", mosquitto_reason_string(result));
      } else {
        fprintf(stderr, "%s\n", mosquitto_connack_string(result));
      }
    }
  }
  fprintf(stdout, "Publisher connect callback.\n");
}

void publish_callback_func(struct mosquitto *mosq, void *obj, int mid, int reason_code,
                           const mosquitto_property *properties) {
  UNUSED(obj);
  UNUSED(properties);

  last_mid_sent = mid;
  if (reason_code > 127) {
    if (!cfg.quiet) {
      fprintf(stderr, "Warning: Publish %d failed: %s.\n", mid, mosquitto_reason_string(reason_code));
    }
  }
  publish_count++;

  if (cfg.pub_mode == MSGMODE_STDIN_LINE) {
    if (mid == last_mid) {
      mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
      disconnect_sent = true;
    }
  } else if (publish_count < cfg.repeat_count) {
    ready_for_repeat = true;
    set_repeat_time();
  } else if (disconnect_sent == false) {
    mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
    disconnect_sent = true;
  }

  fprintf(stdout, "Publisher publish callback.\n");
}

int pub_shared_init(void) {
  line_buf = malloc(line_buf_len);
  if (!line_buf) {
    fprintf(stderr, "Error: Out of memory.\n");
    return EXIT_FAILURE;
  }
  return 0;
}

int publish_loop(struct mosquitto *mosq) {
  int read_len;
  int pos;
  mosq_retcode_t ret, ret2;
  char *buf2;
  int buf_len_actual;
  int mode;
  int loop_delay = 1000;

  if (cfg.repeat_count > 1 && (cfg.repeat_delay.tv_sec == 0 || cfg.repeat_delay.tv_usec != 0)) {
    loop_delay = cfg.repeat_delay.tv_usec / 2000;
  }

  mode = cfg.pub_mode;

  if (mode == MSGMODE_STDIN_LINE) {
    mosquitto_loop_start(mosq);
  }

  do {
    if (mode == MSGMODE_STDIN_LINE) {
      if (status == STATUS_CONNACK_RECVD) {
        pos = 0;
        read_len = line_buf_len;
        while (connected && fgets(&line_buf[pos], read_len, stdin)) {
          buf_len_actual = strlen(line_buf);
          if (line_buf[buf_len_actual - 1] == '\n') {
            line_buf[buf_len_actual - 1] = '\0';
            ret2 = publish_message(mosq, &mid_sent, cfg.topic, buf_len_actual - 1, line_buf, cfg.qos, cfg.retain);
            if (ret2) {
              if (!cfg.quiet) fprintf(stderr, "Error: Publish returned %d, disconnecting.\n", ret2);
              mosquitto_disconnect_v5(mosq, MQTT_RC_DISCONNECT_WITH_WILL_MSG, cfg.disconnect_props);
            }
            break;
          } else {
            line_buf_len += 1024;
            pos += 1023;
            read_len = 1024;
            buf2 = realloc(line_buf, line_buf_len);
            if (!buf2) {
              fprintf(stderr, "Error: Out of memory.\n");
              return MOSQ_ERR_NOMEM;
            }
            line_buf = buf2;
          }
        }
        if (feof(stdin)) {
          if (mid_sent == -1) {
            /* Empty file */
            mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
            disconnect_sent = true;
            status = STATUS_DISCONNECTING;
          } else {
            last_mid = mid_sent;
            status = STATUS_WAITING;
          }
        }
      } else if (status == STATUS_WAITING) {
        if (last_mid_sent == last_mid && disconnect_sent == false) {
          mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
          disconnect_sent = true;
        }
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 100000000;
        nanosleep(&ts, NULL);
      }
      ret = MOSQ_ERR_SUCCESS;
    } else {
      ret = mosquitto_loop(mosq, loop_delay, 1);
      if (ready_for_repeat && check_repeat_time()) {
        ret = 0;
        switch (cfg.pub_mode) {
          case MSGMODE_CMD:
          case MSGMODE_FILE:
          case MSGMODE_STDIN_FILE:
            printf("loop \n");
            ret = publish_message(mosq, &mid_sent, cfg.topic, cfg.msglen, cfg.message, cfg.qos, cfg.retain);
            break;
          case MSGMODE_NULL:
            ret = publish_message(mosq, &mid_sent, cfg.topic, 0, NULL, cfg.qos, cfg.retain);
            break;
          case MSGMODE_STDIN_LINE:
            break;
        }
        if (ret) {
          fprintf(stderr, "Error sending repeat publish: %s", mosquitto_strerror(ret));
        }
      }
    }
  } while (ret == MOSQ_ERR_SUCCESS && connected);

  if (mode == MSGMODE_STDIN_LINE) {
    mosquitto_loop_stop(mosq, false);
  }
  return 0;
}

mosq_retcode_t init_check_error(struct mosq_config *cfg, int pub_or_sub) {
  rc_mosq_retcode_t ret = RC_MOS_OK;

  if (cfg->will_payload && !cfg->will_topic) {
    fprintf(stderr, "Error: Will payload given, but no will topic given.\n");
    return EXIT_FAILURE;
  }
  if (cfg->will_retain && !cfg->will_topic) {
    fprintf(stderr, "Error: Will retain given, but no will topic given.\n");
    return EXIT_FAILURE;
  }
  if (cfg->password && !cfg->username) {
    if (!cfg->quiet) fprintf(stderr, "Warning: Not using password since username not set.\n");
  }

#ifndef WITH_THREADING
  if (cfg->pub_mode == MSGMODE_STDIN_LINE) {
    fprintf(stderr, "Error: '-l' mode not available, threading support has not been compiled in.\n");
    return RC_MOS_INIT_ERROR;
  }
#endif

  if (!cfg->topic || cfg->pub_mode == MSGMODE_NONE) {
    fprintf(stderr, "Error: Both topic and message must be supplied.\n");
    return RC_MOS_INIT_ERROR;
  }

  if (cfg->will_payload && !cfg->will_topic) {
    fprintf(stderr, "Error: Will payload given, but no will topic given.\n");
    return EXIT_FAILURE;
  }
  if (cfg->will_retain && !cfg->will_topic) {
    fprintf(stderr, "Error: Will retain given, but no will topic given.\n");
    return EXIT_FAILURE;
  }
  if (cfg->password && !cfg->username) {
    if (!cfg->quiet) fprintf(stderr, "Warning: Not using password since username not set.\n");
  }
#ifdef WITH_TLS
  if ((cfg->certfile && !cfg->keyfile) || (cfg->keyfile && !cfg->certfile)) {
    fprintf(stderr, "Error: Both certfile and keyfile must be provided if one of them is set.\n");
    return EXIT_FAILURE;
  }
  if ((cfg->keyform && !cfg->keyfile)) {
    fprintf(stderr, "Error: If keyform is set, keyfile must be also specified.\n");
    return EXIT_FAILURE;
  }
  if ((cfg->tls_engine_kpass_sha1 && (!cfg->keyform || !cfg->tls_engine))) {
    fprintf(stderr, "Error: when using tls-engine-kpass-sha1, both tls-engine and keyform must also be provided.\n");
    return EXIT_FAILURE;
  }
#endif
#ifdef FINAL_WITH_TLS_PSK
  if ((cfg->cafile || cfg->capath) && cfg->psk) {
    if (!cfg->quiet) fprintf(stderr, "Error: Only one of --psk or --cafile/--capath may be used at once.\n");
    return EXIT_FAILURE;
  }
  if (cfg->psk && !cfg->psk_identity) {
    if (!cfg->quiet) fprintf(stderr, "Error: --psk-identity required if --psk used.\n");
    return EXIT_FAILURE;
  }
#endif

  if (cfg->clean_session == false && (cfg->id_prefix || !cfg->id)) {
    if (!cfg->quiet) fprintf(stderr, "Error: You must provide a client id if you are using the -c option.\n");
    return EXIT_FAILURE;
  }

  if (pub_or_sub == CLIENT_SUB) {
    if (cfg->topic_count == 0) {
      if (!cfg->quiet) fprintf(stderr, "Error: You must specify a topic to subscribe to.\n");
      return EXIT_FAILURE;
    }
  }

  if (!cfg->host) {
    cfg->host = strdup("localhost");
    if (!cfg->host) {
      if (!cfg->quiet) fprintf(stderr, "Error: Out of memory.\n");
      return EXIT_FAILURE;
    }
  }

  ret = mosquitto_property_check_all(CMD_CONNECT, cfg->connect_props);
  if (ret) {
    if (!cfg->quiet) fprintf(stderr, "Error in CONNECT properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }
  ret = mosquitto_property_check_all(CMD_PUBLISH, cfg->publish_props);
  if (ret) {
    if (!cfg->quiet) fprintf(stderr, "Error in PUBLISH properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }
  ret = mosquitto_property_check_all(CMD_SUBSCRIBE, cfg->subscribe_props);
  if (ret) {
    if (!cfg->quiet) fprintf(stderr, "Error in SUBSCRIBE properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }
  ret = mosquitto_property_check_all(CMD_UNSUBSCRIBE, cfg->unsubscribe_props);
  if (ret) {
    if (!cfg->quiet) fprintf(stderr, "Error in UNSUBSCRIBE properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }
  ret = mosquitto_property_check_all(CMD_DISCONNECT, cfg->disconnect_props);
  if (ret) {
    if (!cfg->quiet) fprintf(stderr, "Error in DISCONNECT properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }
  ret = mosquitto_property_check_all(CMD_WILL, cfg->will_props);
  if (ret) {
    if (!cfg->quiet) fprintf(stderr, "Error in Will properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }

  return MOSQ_ERR_SUCCESS;
}

void pub_shared_cleanup(void) { free(line_buf); }