#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <mosquitto.h>
#include <stdio.h>
#include <sys/time.h>

/* pub_client.c modes */
#define MSGMODE_NONE 0
#define MSGMODE_CMD 1
#define MSGMODE_STDIN_LINE 2
#define MSGMODE_STDIN_FILE 3
#define MSGMODE_FILE 4
#define MSGMODE_NULL 5

#define CLIENT_PUB 1
#define CLIENT_SUB 2
#define CLIENT_RR 3
#define CLIENT_RESPONSE_TOPIC 4

#define EXIT_FAILURE 1

#define HOST "140.116.82.61"
#define TOPIC "NB/test/room1"
#define MESSAGE "this is the testing message, so say hi to me"

typedef enum mosq_err_t mosq_retcode_t;  // typedef the original enum
// out own return error code for MQTT layer
typedef enum mosq_retcode_s {
  RC_MOS_OK,
  RC_MOS_INIT_ERROR,
} rc_mosq_retcode_t;

struct mosq_config {
  char *id;
  char *id_prefix;
  int protocol_version;
  int keepalive;
  char *host;
  int port;
  int qos;
  bool retain;
  int pub_mode;     /* pub, rr */
  char *file_input; /* pub, rr */
  char *message;    /* pub, rr */
  long msglen;      /* pub, rr */
  char *topic;      /* pub, rr */
  char *bind_address;
  int repeat_count;            /* pub */
  struct timeval repeat_delay; /* pub */
#ifdef WITH_SRV
  bool use_srv;
#endif
  bool debug;
  bool quiet;
  unsigned int max_inflight;
  char *username;
  char *password;
  char *will_topic;
  char *will_payload;
  long will_payloadlen;
  int will_qos;
  bool will_retain;
#ifdef WITH_TLS
  char *cafile;
  char *capath;
  char *certfile;
  char *keyfile;
  char *ciphers;
  bool insecure;
  char *tls_alpn;
  char *tls_version;
  char *tls_engine;
  char *tls_engine_kpass_sha1;
  char *keyform;
#ifdef FINAL_WITH_TLS_PSK
  char *psk;
  char *psk_identity;
#endif
#endif
  bool clean_session;
  char **topics;         /* sub */
  int topic_count;       /* sub */
  bool exit_after_sub;   /* sub */
  bool no_retain;        /* sub */
  bool retained_only;    /* sub */
  bool remove_retained;  /* sub */
  char **filter_outs;    /* sub */
  int filter_out_count;  /* sub */
  char **unsub_topics;   /* sub */
  int unsub_topic_count; /* sub */
  bool verbose;          /* sub */
  bool eol;              /* sub */
  int msg_count;         /* sub */
  char *format;          /* sub */
  int timeout;           /* sub */
  int sub_opts;          /* sub */
#ifdef WITH_SOCKS
  char *socks5_host;
  int socks5_port;
  char *socks5_username;
  char *socks5_password;
#endif
  mosquitto_property *connect_props;
  mosquitto_property *publish_props;
  mosquitto_property *subscribe_props;
  mosquitto_property *unsubscribe_props;
  mosquitto_property *disconnect_props;
  mosquitto_property *will_props;
  bool have_topic_alias; /* pub */
  char *response_topic;  /* rr */
};

void client_config_cleanup(struct mosq_config *cfg);
int client_opts_set(struct mosquitto *mosq, struct mosq_config *cfg);
int client_id_generate(struct mosq_config *cfg);
int client_connect(struct mosquitto *mosq, struct mosq_config *cfg);
int cfg_add_topic(struct mosq_config *cfg, int type, char *topic);
void init_config(struct mosq_config *cfg, int pub_or_sub);
#endif
