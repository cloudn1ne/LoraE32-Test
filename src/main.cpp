#include <Arduino.h>
#include <TTN_esp32.h>
#include <U8g2lib.h>

// #define PRINT_SERIAL 1
#define POWERSAVE    1
#define LED_PIN      25
#define VBAT_PIN     37
#define VBAT_MEASURE_PIN 21



#ifndef POWERSAVE
 #define TX_DELAY_SEC 120
#else
 #define TX_DELAY_SEC 120/TIME_TO_SLEEP
#endif

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  120        /* Time ESP32 will go to sleep (in seconds) */

/***************************************************************************
 *  Go to your TTN console register a device then the copy fields
 *  and replace the CHANGE_ME strings below
 ****************************************************************************/
const char* devEui = "0000000000BABE01"; // TTN Device EUI
const char* appEui = "000000000000AE01"; // Application EUI
const char* appKey = "000000000000000000000000DEADBEEF"; // Application Key

static RTC_NOINIT_ATTR bool led_on=false;

// tx delay counter
static uint16_t tx_delay_counter=TX_DELAY_SEC;
// current battery voltage
static uint16_t bat_int;

// tx message counter
static RTC_NOINIT_ATTR uint16_t tx_msg_count = 0;

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

#ifdef PRINT_SERIAL
    Serial.println("-- MESSAGE");
    Serial.print("Received " + String(size) + " bytes RSSI=" + String(rssi) + "db : [ ");
    for (int i = 0; i < size; i++)
    {
        Serial.printf("%02X ", payload[i]);        
    }
    Serial.println(" ]");
    Serial.printf(" setting led = %d\n", led_on);
#endif

#ifndef POWERSAVE    
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
#endif    
}

float getBatteryVoltageFloat(int b_int)
{
  float voltageDivider = 338;
  float ret_val;
  // int readMin = 1080; // -> 338 * 3.2 // If you want to draw a progress bar, this is 0%
  // int readMax = 1420; // -> 338 * 4.2 // If you want to draw a progress bar, this is 100%
  ret_val = (b_int/voltageDivider);
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

void disp_message_n_sent(uint32_t c)
{
  u8g2.firstPage();
  do {
      u8g2.setFont(u8g2_font_5x7_mf);
      u8g2.setCursor(0,20);        
      u8g2.printf("Message #%d sent !", c);
  } while ( u8g2.nextPage() );
}


void print_wakeup_reason()
{
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason)
    {
    case 1:
        Serial.println("Wakeup caused by external signal using RTC_IO");
        break;
    case 2:
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        break;
    case 3:
        Serial.println("Wakeup caused by timer");
        break;
    case 4:
        Serial.println("Wakeup caused by touchpad");
        break;
    case 5:
        Serial.println("Wakeup caused by ULP program");
        break;
    default:
        Serial.println("Wakeup was not caused by deep sleep");
        break;
    }
}

void senddata()
{
  uint8_t data[5];
  int s;

  bat_int = getBatteryVoltage(10);  
  // measure battery        
  data[0] = (bat_int>>8)&0xFF;
  data[1] = bat_int&0xFF;    
  // tx payload is our tx_msg_count (16bit)
  data[2] = (tx_msg_count>>8)&0xFF;
  data[3] = tx_msg_count&0xFF;        
  data[4] = led_on;
  tx_delay_counter = TX_DELAY_SEC;
  s = ttn.sendBytes(data, 5, 1, false);
        
  if (s)
  {                
      #ifndef POWERSAVE
        disp_message_n_sent(tx_msg_count);         
      #endif
      #ifdef PRINT_SERIAL
        Serial.printf("Message #%d sent !\n", tx_msg_count);      
      #endif
  } 
  else
  {
      #ifdef PRINT_SERIAL
        Serial.printf("Message #%d not acknowledged !\n", tx_msg_count);
      #endif
  }
}

/*
** ======================================= SETUP ===============================================
*/
void setup()
{  
    pinMode(LED_PIN, OUTPUT);           // set pin to input     
    pinMode(VBAT_MEASURE_PIN, OUTPUT);  // set pin to input         
    bat_int = getBatteryVoltage(10); 
   
#ifndef POWERSAVE    
    u8g2.begin();
    banner();    
#endif        

#ifdef PRINT_SERIAL
    Serial.begin(115200);    
    delay(500);
    print_wakeup_reason();
#endif    

      // initialize global variables     
    esp_reset_reason_t reason = esp_reset_reason();
    if ((reason != ESP_RST_DEEPSLEEP) && (reason != ESP_RST_SW)) {
#ifdef PRINT_SERIAL        
        Serial.println("initializing global variables due to real poweron event\n");
#endif        
    	tx_msg_count = 0;        
        led_on = false;        
    }
    else
    { 
#ifdef PRINT_SERIAL               
        Serial.printf("Recovered global variabels\n");
#endif        
        tx_msg_count++;
#ifdef PRINT_SERIAL        
        Serial.printf(" tx_msg_count  = %d\n", tx_msg_count);
        Serial.printf(" led_on  = %d\n", led_on);
#endif        
    }

#ifdef PRINT_SERIAL    
    Serial.printf("Battery: %1.3fV\n", getBatteryVoltageFloat(bat_int));        
#endif

    ttn.begin();    
    ttn.onMessage(message);                             
    ttn.join(devEui, appEui, appKey);
    while (!ttn.isJoined())  // show a wheel while we wait for the Lora Join
    {
#ifndef POWERSAVE      
        Serial.print(".");                      
        spin_wheely(120,30);
#endif          
        delay(250);
    }
    ttn.setDataRate(EU868_DR_SF7);    

#ifdef PRINT_SERIAL    
    Serial.println("\njoined !\n");
#endif
#ifndef POWERSAVE        
    u8g2.clearBuffer();
    u8g2.sendBuffer();        
    ttn.showStatus();
#endif    

    // Make sure any pending transactions are handled first
    ttn.waitForPendingTransactions();
    // Send our data
    senddata();
    // Make sure our transactions is handled before going to sleep
    ttn.waitForPendingTransactions();

    digitalWrite(LED_PIN, led_on);     
    if (led_on)         // show led for 5secs
        delay(5000);

#ifdef PRINT_SERIAL        
    Serial.println("going to ZZZzZ now\n\n");
#endif    
  
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);         
    esp_deep_sleep_start();
}

void loop()
{     

    // NEVER REACHED 

    /*

    uint8_t data[5];
    float voltage;
      
    if (tx_delay_counter > 0)
    { // delay
#ifndef POWERSAVE
      spin_wheely(120,20);
      u8g2.setCursor(95,20);  
      u8g2.printf("%03d", tx_delay_counter);     
      voltage = getBatteryVoltageFloat(bat_int);  
      u8g2.setCursor(0,35);      
      u8g2.printf("Bat: %1.3fV", voltage);     
      u8g2.sendBuffer();      
      delay(1000);        
#else      
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);     
      esp_light_sleep_start(); 
      //esp_deep_sleep_start();      
#endif      
      tx_delay_counter--;
    }
    else
    { // action
        senddata();
        tx_msg_count++;
    }    
 */   
}