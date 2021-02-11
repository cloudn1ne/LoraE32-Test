## Simple LoRAWAN Test for the Heltec WIFI LoRa 32 v2 module

Simple LoRaWAN Client that sends an incrementing 32bit number as payload, and will display any bytes downlinked (only about 6 fit on the display).

You need to **change devEui, appEui, appKey in main.cpp** and configure the OTAA parameters in your LoraGateway Software.

Module: https://www.bastelgarage.ch/wifi-lora-32-v2-sx1276-868mhz-mit-oled


### Downlink Example with Chirpstack

Bit 0 of the LSB is controlling GPIO25 (White LED on the Heltec Board) meaning you can turn it on/off via the downlink.

> mosquitto_pub -f on/off.mqtt -h <Chirpstack_HOST> -t "application/<chirpstack_ApplicationID>/device/<devEui>/command/down"

on.mqtt:

```js
{
 "confirmed": false,
 "fPort": 3,
 "object": {
	"data": 1
 }
}
```

off.mqtt:

```js
{
 "confirmed": false,
 "fPort": 3,
 "object": {
	"data": 2
 }
}
```


### Sample Device Profiles for Chirpstack
```js
function Decode(fPort, bytes, variables) {
  var counter;
  
  counter = (bytes[0]<<24) | (bytes[1]<<16) | (bytes[2]<<8) | (bytes[3]);
  return { len:bytes.length, data:counter };
}

function Encode(fPort, obj, variables) {
  b0 = (obj.data>>24) & 0xFF;
  b1 = (obj.data>>16) & 0xFF;
  b2 = (obj.data>>8) & 0xFF;
  b3 = obj.data & 0xFF;
  return [ b0, b1, b2, b3];
}
```
