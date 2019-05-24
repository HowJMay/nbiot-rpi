/*
Copyright (c) 2018 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License v1.0
and Eclipse Distribution License v1.0 which accompany this distribution.

The Eclipse Public License is available at
   http://www.eclipse.org/legal/epl-v10.html
and the Eclipse Distribution License is available at
  http://www.eclipse.org/org/documents/edl-v10.php.

Contributors:
   Roger Light - initial implementation and documentation.
*/

#include <stdio.h>
#include <stdlib.h>
#include "mosquitto.h"

int main(int argc, char *argv[]) {
  int rc;
  struct mosquitto_message *msg;
  mosquitto_lib_init();

  rc = mosquitto_subscribe_simple(&msg, 1, true, "NB/test/room1", 0, "node0.puyuma.org", 1883, NULL, 60, true, NULL,
                                  NULL, NULL, NULL);

  if (rc) {
    printf("Error: %s\n", mosquitto_strerror(rc));
    mosquitto_lib_cleanup();
    return rc;
  }

  printf("message: %s %s\n", msg->topic, (char *)msg->payload);
  mosquitto_message_free(&msg);

  mosquitto_lib_cleanup();

  return 0;
}
