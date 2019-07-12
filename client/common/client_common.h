#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <mosquitto.h>
#include <mqtt_protocol.h>
#include <stdio.h>
#include <sys/time.h>

/* pub_client.c modes */
#define MSGMODE_SUB 0
#define MSGMODE_NONE 0
#define MSGMODE_CMD 1
#define MSGMODE_STDIN_LINE 2
#define MSGMODE_STDIN_FILE 3
#define MSGMODE_FILE 4
#define MSGMODE_NULL 5

#define CLIENT_RESPONSE_TOPIC 4

#define EXIT_FAILURE 1

#define HOST "140.116.82.61"
#define TOPIC "NB/test/room1"
#define TOPIC_RES "NB/test/room2"
#define MESSAGE "this is the testing message, so say hi to me"

// typedef enum client_type_s { pub = pub, sub = sub, duplex = duplex } client_type_t;
typedef enum client_type_s { client_pub, client_sub, client_duplex } client_type_t;

typedef enum mosq_err_t mosq_retcode_t;  // typedef the original enum
// out own return error code for MQTT layer
typedef enum mosq_retcode_s {
  RC_MOS_OK,
  RC_MOS_INIT_ERROR,
  RC_MOS_CRASH,
  RC_MOS_CHANNEL_SETTING,
  RC_MOS_MESSAGE_SETTING,
} rc_mosq_retcode_t;

typedef struct mosq_general_config_s {
  char *id;
  char *id_prefix;
  int protocol_version;
  int keepalive;
  char *host;
  int port;
  int qos;
  bool retain;
  client_type_t client_type;
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
  char *bind_address;
  bool clean_session;
} mosq_general_config_t;

typedef struct mosq_pub_config_s {
  int pub_mode;                /* pub, rr */
  char *file_input;            /* pub, rr */
  char *message;               /* pub, rr */
  long msglen;                 /* pub, rr */
  char *topic;                 /* pub, rr */
  int repeat_count;            /* pub */
  struct timeval repeat_delay; /* pub */
  bool have_topic_alias;       /* pub */
  char *response_topic;        /* rr */
} mosq_pub_config_t;

typedef struct mosq_sub_config_s {
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
} mosq_sub_config_t;

typedef struct mosq_property_config_s {
  mosquitto_property *connect_props;
  mosquitto_property *publish_props;
  mosquitto_property *subscribe_props;
  mosquitto_property *unsubscribe_props;
  mosquitto_property *disconnect_props;
  mosquitto_property *will_props;
} mosq_property_config_t;

#ifdef WITH_TLS
typedef struct mosq_tls_config_s {
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
} mosq_tls_config_t;
#endif

#ifdef WITH_SOCKS
typedef struct mosq_socks_config_s {
  char *socks5_host;
  int socks5_port;
  char *socks5_username;
  char *socks5_password;
} mosq_socks_config_t;
#endif

struct mosq_config {
  mosq_general_config_t *general_config;
  mosq_pub_config_t *pub_config;
  mosq_sub_config_t *sub_config;
  mosq_property_config_t *property_config;
#ifdef WITH_TLS
  mosq_tls_config_t *tls_config;
#endif

#ifdef WITH_SOCKS
  mosq_socks_config_t *socks_config;
#endif
};

void mosq_config_cleanup(struct mosq_config *cfg);
void init_mosq_config(struct mosq_config *cfg, client_type_t client_type);
int mosq_opts_set(struct mosquitto *mosq, struct mosq_config *cfg);
int generate_client_id(struct mosq_config *cfg);
int mosq_client_connect(struct mosquitto *mosq, struct mosq_config *cfg);
int cfg_add_topic(struct mosq_config *cfg, client_type_t client_type, char *topic);

#endif