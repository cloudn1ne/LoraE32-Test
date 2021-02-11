#include <Arduino.h>
#include <TTN_esp32.h>
#include <U8g2lib.h>

#define LED_PIN      25
#define TX_DELAY_SEC 120

/***************************************************************************
 *  Go to your TTN console register a device then the copy fields
 *  and replace the CHANGE_ME strings below
 ****************************************************************************/
const char* devEui = "0000000000BABE01"; // TTN Device EUI
const char* appEui = "000000000000AE01"; // Application EUI
const char* appKey = "000000000000000000000000DEADBEEF"; // Application Key

// tx delay counter
static uint16_t tx_delay_counter=TX_DELAY_SEC;

// tx message counter
static uint32_t tx_msg_count = 0;

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
    if (payload[3] & 1)
      digitalWrite(LED_PIN, HIGH);
    else
      digitalWrite(LED_PIN, LOW);

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
    u8g2.begin();
    banner();
    Serial.begin(115200);
    delay(2000);
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
    uint8_t data[4];

    // tx payload is our tx_msg_count (32bit)
    data[0] = (tx_msg_count>>24)&0xFF;
    data[1] = (tx_msg_count>>16)&0xFF;
    data[2] = (tx_msg_count>>8)&0xFF;
    data[3] = tx_msg_count&0xFF;    

    if (tx_delay_counter > 0)
    { // delay
      spin_wheely(120,20);
      u8g2.setCursor(95,20);  
      u8g2.printf("%03d", tx_delay_counter);            
      u8g2.sendBuffer();      
      delay(1000);
      tx_delay_counter--;  
    }
    else
    { // action
        tx_delay_counter = TX_DELAY_SEC;
        if ( ttn.sendBytes(data, 4, 1, false) )
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