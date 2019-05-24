# Introduction
The structure of this repo consists of threee components

### 1. rpi client

This is a client implemented with rpi and the modem we used at this moment. The current using modem accepts we sending MQTT message with AT-command-like command. So the program running on rpi doesn't need to use `mosquitto` based programs.
The only thing `rpi client` needs is chose the right topic (we use different topics to simulate RESTful methodology) and serialize the data/message into the demanded format for TA requests. After the message is serialized into the demanded format, we can send this message with modem provided command as simple as we send a http request with Python.

### 2. TA server

TA plays a role of server which receives requests from NB-IoT and processes the requests. However, under the structure of MQTT network, both `TA server` and `rpi client` are `MQTT client`. `TA server` is not a broker under the topoloy of MQTT.
`TA server` run as a MQTT subscriber which listen to the requests on several different topics. We treat each topic as different URL path of http protocol does.

### 3. MQTT message parser client
It might necessitate parsing MQTT nessages, since the message sent from modem may vary from one to another. In order to support a wide range of modem from differernt manufactures, we can use a parser client to revise the message from modem into the demanded format, then sent the right format message to a specific topic which contains only requests follow TA's request format.
Thus, this `parse client` plays a role of both subscriber and publisher, and we need to implemented the parser client with modifying `mosquitto` source code.
