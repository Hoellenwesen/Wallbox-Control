// Copyright (c) 2021 steff393, MIT license

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "globalConfig.h"
#include "logger.h"
#include "mbComm.h"
#include <PubSubClient.h>

const uint8_t m = 2;

WiFiClient espClient;
PubSubClient client(espClient);
uint32_t 	lastMsg = 0;
uint32_t 	lastReconnect = 0;


void callback(char* _topic, byte* payload, uint8_t length)
{
	String topic = String(_topic);

	// handle received message
	log(m, "Received: " + topic + ", Payload: ", false);
	char buffer[length];
	for (uint8_t i = 0; i < length; i++) {
		log(0, String((char)payload[i]), false);
		buffer[i] = (char)payload[i];
	}
	buffer[length] = '\0';			// add string termination

	if (topic.startsWith("openWB/lp/") && topic.endsWith("/AConfigured")) {
		uint8_t val = String(buffer).toInt();				// Alternative: toFloat()
		uint8_t lp  = topic.substring(10,11).toInt();		// loadpoint nr.
		uint8_t i;
		// search, which index fits to loadpoint, first element will be selected
		for (i = 0; i < cfgCntWb; i++) {
			if (cfgMqttLp[i] == lp) {break;}
		}
		if (cfgMqttLp[i] == lp) {
			// openWB has 1A resolution, wbec has 0.1A resulotion
			val = val * 10;
			// set current
			if (val == 0 || (val >= CURR_ABS_MIN && val <= CURR_ABS_MAX)) {
				log(0, ", Write to box: " + String(i) + " Value: " + String(val));
				mb_writeReg(i, REG_CURR_LIMIT, val);
			}
		} else {
			log(0, ", no box assigned");
		}
	}
}


void mqtt_begin() {
	if (strcmp(cfgMqttIp, "") != 0) {
  	client.setServer(cfgMqttIp, 1883);
		client.setCallback(callback);
	}
}

void reconnect() {
	log(m, "Attempting MQTT connection...", false);
	// Create a random client ID
	String clientId = "wbec-";
	clientId += String(random(0xffff), HEX);
	// Attempt to connect
	if (client.connect(clientId.c_str()))				// alternative: client.connect(clientId,userName,passWord)
	{
		log(0, "connected");
		//once connected to MQTT broker, subscribe command if any
		for (uint8_t i = 0; i < cfgCntWb; i++) {
			String topic = "openWB/lp/+/AConfigured";
			if (cfgMqttLp[i] != 0) {
				topic.setCharAt(10, char(cfgMqttLp[i] + '0'));
				client.subscribe(topic.c_str());
			}
		}
	} else {
		log(m, String("failed, rc=") + client.state() + " try again in 5 seconds");
	}
}

void mqtt_handle() {
	if (strcmp(cfgMqttIp, "") != 0) {
		uint32_t now = millis();

		if (!client.connected()) {
			if (now - lastReconnect > 5000 || lastReconnect == 0) {
				reconnect();
				lastReconnect = now;
			}
		}

		client.loop();
	}
}


void mqtt_publish(uint8_t i) {
	if (strcmp(cfgMqttIp, "") == 0 || cfgMqttLp[i] == 0) {
		return;	// do nothing, when Mqtt is not configured, or box has no loadpoint assigned
	}
	// publish the contents of box i
	String header = String("openWB/set/lp/") + String(cfgMqttLp[i]);
	boolean retain = true;
	uint8_t ps = 0;
	uint8_t cs = 0;

	switch(content[i][1]) {
		case 2:  ps = 0; cs = 0; break;
		case 3:  ps = 0; cs = 0; break;
		case 4:  ps = 1; cs = 0; break;
		case 5:  ps = 1; cs = 0; break;
		case 6:  ps = 1; cs = 0; break;
		case 7:  ps = 1; cs = 1; break;
		default: ps = 0; cs = 0; break; 
	}
	client.publish(String(header + "/plugStat").c_str(),   String(ps).c_str(), retain);
	client.publish(String(header + "/chargeStat").c_str(), String(cs).c_str(), retain);

	client.publish(String(header + "/W").c_str(),          String(content[i][10]).c_str(), retain);
	client.publish(String(header + "/kWhCounter").c_str(), String((float)((uint32_t) content[i][13] << 16 | (uint32_t)content[i][14]) / 1000.0, 3).c_str(), retain);
	client.publish(String(header + "/VPhase1").c_str(),    String(content[i][6]).c_str(), retain);
	client.publish(String(header + "/VPhase2").c_str(),    String(content[i][7]).c_str(), retain);
	client.publish(String(header + "/VPhase3").c_str(),    String(content[i][8]).c_str(), retain);
	client.publish(String(header + "/APhase1").c_str(),    String((float)content[i][2] / 10.0, 1).c_str(), retain);
	client.publish(String(header + "/APhase2").c_str(),    String((float)content[i][3] / 10.0, 1).c_str(), retain);
	client.publish(String(header + "/APhase3").c_str(),    String((float)content[i][4] / 10.0, 1).c_str(), retain);
	log(m, "Publish to " + header);
}
