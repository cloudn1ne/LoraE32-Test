#include <Arduino.h>
#include <TTN_esp32.h>
#include <U8g2lib.h>

#define LED_PIN      25
#define VBAT_PIN     37
#define VBAT_MEASURE_PIN 21
#define TX_DELAY_SEC 120

/***************************************************************************
 *  Go to your TTN console register a device then the copy fields
 *  and replace the CHANGE_ME strings below
 ****************************************************************************/
const char* devEui = "0000000000BABE01"; // TTN Device EUI
const char* appEui = "000000000000AE01"; // Application EUI
const char* appKey = "000000000000000000000000DEADBEEF"; // Application Key

static bool led_on=false;

// tx delay counter
static uint16_t tx_delay_counter=TX_DELAY_SEC;
// current battery voltage
static uint16_t bat_int;

// tx message counter
static uint16_t tx_msg_count = 0;

// wheely helpers
const char wheely_ar[4]={'/', '-', '\\', '|'};
const char wheely_size=4;
char wheely_idx=0;

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);
TTN_esp32 ttn ;

void banner(void)
{
  u8g2.setFont(u8g2_font_6x10_mf);
  u8g2.firstPage();
  do {
    u8g2.setCursor(0,15);
    u8g2.print("DevEUI: BABE01");
    u8g2.setCursor(0,30);
    u8g2.print("joining LoRAWAN ...");
  } while ( u8g2.nextPage() );
}

// show a spinning wheely at coordinates
void spin_wheely(int x, int y)
{
  char wheely_chr = wheely_ar[wheely_idx++];
  if (wheely_idx == wheely_size) wheely_idx=0;
  u8g2.setFont(u8g2_font_6x10_mf);  
  u8g2.setCursor(x,y);        
  u8g2.print(wheely_chr);
  u8g2.sendBuffer();
}

void message(const uint8_t* payload, size_t size, int rssi)
{    
    // bit 0 of lsb byte controls the LED
    led_on = payload[3] & 1;

    digitalWrite(LED_PIN, led_on);
    
    Serial.println("-- MESSAGE");
    Serial.print("Received " + String(size) + " bytes RSSI=" + String(rssi) + "db : [ ");
    for (int i = 0; i < size; i++)
    {
        Serial.printf("%02X ", payload[i]);        
    }
    Serial.println(" ]");

    u8g2.setFont(u8g2_font_5x7_mf);
    u8g2.setCursor(0,45);        
    u8g2.printf("RSSI = %d db SIZE = %d", rssi, size);
    u8g2.setFont(u8g2_font_6x10_mf);  
    u8g2.setCursor(0,60);        
    for (int i = 0; i < size; i++)
    {
        u8g2.setCursor(i*15,60);  
        u8g2.printf("%02X", payload[i]);        
    }
    u8g2.sendBuffer();
}

float getBatteryVoltageFloat(int bat_int)
{
  float voltageDivider = 338;
  float ret_val;
  // int readMin = 1080; // -> 338 * 3.2 // If you want to draw a progress bar, this is 0%
  // int readMax = 1420; // -> 338 * 4.2 // If you want to draw a progress bar, this is 100%
  ret_val = (bat_int/voltageDivider);
  return ret_val; 
}
uint16_t getBatteryVoltage(int nbMeasurements) 
{
    uint16_t bat_int; // 12bit battery value (0-4095)

    // Take x measurements and average
    digitalWrite(VBAT_MEASURE_PIN, LOW);
    int readValue = 0;
    for (int i = 0; i < nbMeasurements; i++) {
        readValue += analogRead(VBAT_PIN);
        delay(100);
    }
    digitalWrite(VBAT_MEASURE_PIN, HIGH);

    bat_int = (readValue/nbMeasurements); 
    return bat_int;
}

void message_n_sent(uint32_t c)
{
  u8g2.firstPage();
  do {
      u8g2.setFont(u8g2_font_5x7_mf);
      u8g2.setCursor(0,20);        
      u8g2.printf("Message #%d sent !", c);
  } while ( u8g2.nextPage() );
}

void setup()
{
    pinMode(LED_PIN, OUTPUT);           // set pin to input     
    pinMode(VBAT_MEASURE_PIN, OUTPUT);           // set pin to input     
    digitalWrite(LED_PIN, led_on);      // we start turned off (led_on=false)
    bat_int = getBatteryVoltage(10); 
    u8g2.begin();
    banner();
    Serial.begin(115200);
    delay(500);
    Serial.printf("Battery: %1.3fV\n", getBatteryVoltageFloat(bat_int));    
    Serial.println("Starting\n");
    ttn.begin();    
    ttn.onMessage(message); // Declare callback function for handling downlink
                            // messages from server
    ttn.join(devEui, appEui, appKey);
    Serial.print("Joining Network\n");
    while (!ttn.isJoined())  // show a wheel while we wait for the Lora Join
    {
        Serial.print(".");        
        spin_wheely(120,30);
        delay(250);
    }
    Serial.println("\njoined !\n");
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    ttn.setDataRate(EU868_DR_SF7);    
    ttn.showStatus();
}

void loop()
{     
    uint8_t data[5];
    float voltage;

      
    if (tx_delay_counter > 0)
    { // delay
      spin_wheely(120,20);
      u8g2.setCursor(95,20);  
      u8g2.printf("%03d", tx_delay_counter);     
      voltage = getBatteryVoltageFloat(bat_int);  
      u8g2.setCursor(0,35);      
      u8g2.printf("Bat: %1.3fV", voltage);     
      u8g2.sendBuffer();      
      delay(1000);
      tx_delay_counter--;  
    }
    else
    { // action
        bat_int = getBatteryVoltage(10);  
        // measure battery        
        data[0] = (bat_int>>8)&0xFF;
        data[1] = bat_int&0xFF;    
        // tx payload is our tx_msg_count (16bit)
        data[2] = (tx_msg_count>>8)&0xFF;
        data[3] = tx_msg_count&0xFF;        
        data[4] = led_on;

        tx_delay_counter = TX_DELAY_SEC;
        if ( ttn.sendBytes(data, 5, 1, false) )
        {      
          message_n_sent(tx_msg_count);       
          Serial.printf("Message #%d sent !\n", tx_msg_count);
        }
        else
        {
          Serial.printf("Message #%d not acknowledged !\n", tx_msg_count);
        }
        tx_msg_count++;
    }    
    
}