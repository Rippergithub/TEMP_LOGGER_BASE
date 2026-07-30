#include <OneWire.h>
#include "EPD.h"        
#include <SPI.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include "TH09C.h"
#include <Adafruit_MAX31865.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_sleep.h"


// Ağ Parametreleri Tanımları
const char* ssid = "srv"; // WiFi Ağ Adı
const char* password = "12345678"; // WiFi Ağ Şifresi
const char* url = "http://192.168.2.111:8000/data"; //WiFi Yayın IP Adresi

//  Pin Tanımları 
#define REG_CTL   0   // Buck Regulater Enable Pin
#define LDO_CTL   1   // LDO Enable Pin
#define IO4       4   // IO4 LP Wake Pin
#define EPD_DC    5   // E-Paper Data/Commannd Pin
#define EPD_BUSY  14  // E-Paper Busy Pin
#define EPD_RES   19  // E-Paper Reset Pin
#define EPD_CS    20  // E-Paper SPI Chip Select Pin
#define DS18_PIN  15  // DS18B20 Data Pin
#define MAX_CS    21  // MAX31865 SPI Chip Select Pin

#define WAKE_PIN GPIO_NUM_4   // IO4

#define MISO 2     // SPI MISO Pin
#define SPI_CLK 6  // SPI Clock Pin
#define MOSI 7     // SPI MOSI Pin

#define SDA 22     // I2C Serial Data Pin
#define SCL 23     // I2C Serial Clock Pin

//EPD Tanımlar
unsigned char BlackImage[EPD_ARRAY];

//DS18B20 Tanımlar
float ds18_data;
char ds18_temp[10];
OneWire oneWire(DS18_PIN);
DallasTemperature ds18(&oneWire);

//TH09C Tanımlar
float th09c_temp_data, th09c_hum_data;
char th09c_temp[10]; // Ekran için karakter dizisi
char th09c_hum[10]; // Ekran için karakter dizisi
TH09C TH09C;

//MAX31865 Tanımlar
#define max31865_rref  4000.0
#define max31865_rnom  1000.0
uint16_t max31865_rtd_data;
char max31865_temp[10];
float max31865_ratio;
float max31865_data;
uint8_t max31865_fault;
Adafruit_MAX31865 thermo = Adafruit_MAX31865(MAX_CS);


//Aktif SPI CS Seçim Fonksiyonu
void setActiveSPI(int activePin) {

  digitalWrite(EPD_CS, HIGH);
  digitalWrite(MAX_CS, HIGH);
  
  if (activePin != -1) {
    digitalWrite(activePin, LOW);
  }
}

//WiFİ Bağlanma Fonksiyonu
void connectWifi()
{
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20)
  {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nConnected");
  }
  else
  {
    Serial.println("\nOffline");
  }
}


//WiFi Data Gönderme Fonksiyonu
void sendData()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi yok");
    return;
  }

  HTTPClient http;
  http.setTimeout(10000);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"ds18\":" + String(ds18_data) + ",";
  json += "\"th09_temp\":" + String(th09c_temp_data) + ",";
  json += "\"th09_hum\":" + String(th09c_hum_data) + ",";
  json += "\"pt1000\":" + String(max31865_data);
  json += "}";

  int code = http.POST(json);

  Serial.print("HTTP Code: ");
  Serial.println(code);

  http.end();
}


//Deep Sleep Uyku Aktivasyon Fonksiyonu
void goToSleep()
{
  Serial.println("Sleeping 60 sec...");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKE_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);

  esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);

  delay(100);
  esp_deep_sleep_start();
}


//Wake Nedeni Yazdırma Fonksiyonu
void printWakeReason()
{
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();

  switch(reason)
  {
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wake: TIMER"); break;
    case ESP_SLEEP_WAKEUP_GPIO:  Serial.println("Wake: GPIO"); break;
    default: Serial.println("Wake: OTHER"); break;
  }
}

//DS18B20 Sıcaklık Alma Fonksiyonu
void ds18b20_read(){

  ds18.requestTemperatures();
  delay(100);
  ds18_data = ds18.getTempCByIndex(0);
  

  if (ds18_data < -120.00) sprintf(ds18_temp, "err"); 
  else sprintf(ds18_temp, "%.1f", ds18_data);

  Serial.print("DS18B20 Sicaklik: "); Serial.print(ds18_temp); Serial.println(" °C ");
}

//TH09C Sıcaklık - Nem Alma Fonksiyonu
void th09c_read(){

  TH09C.readTempHum(th09c_temp_data, th09c_hum_data);

if (th09c_temp_data == 0 && th09c_hum_data == 0) {
        sprintf(th09c_temp, "ERR"); 
        sprintf(th09c_hum, "ERR"); 

    } else {
        sprintf(th09c_temp, "%.1f", th09c_temp_data); 
        sprintf(th09c_hum, "%.1f", th09c_hum_data); 

    }
    
    Serial.print("TH09C Sicaklik: "); Serial.print(th09c_temp); Serial.print(" °C ");
    Serial.print("TH09C Nem: % "); Serial.println(th09c_hum); 
    }



//MAX31865 Sıcaklık Alma Fonksiyonu
    void max31865_read(){

          setActiveSPI(MAX_CS);
          
          max31865_rtd_data = thermo.readRTD();

          max31865_ratio = (float) max31865_rtd_data  / 32768;
          
          max31865_data = thermo.temperature(max31865_rnom, max31865_rref);


          if (max31865_data < -120.00) sprintf(max31865_temp, "ERR"); 
          else sprintf(max31865_temp, "%.1f", max31865_data);


          Serial.print("Max31865 Sicaklik: "); Serial.print(max31865_data); Serial.println(" °C ");

           max31865_fault = thermo.readFault();
          if (max31865_fault) {
            Serial.print("Fault detected 0x"); 
            Serial.println(max31865_fault, HEX);
            
            if (max31865_fault & MAX31865_FAULT_HIGHTHRESH) {
              Serial.println("Error: RTD High Threshold"); 
            }
            if (max31865_fault & MAX31865_FAULT_LOWTHRESH) {
              Serial.println("Error: RTD Low Threshold"); 
            }
            if (max31865_fault & MAX31865_FAULT_REFINLOW) {
              Serial.println("Error: REFIN- > 0.85 x Bias"); 
            }
            if (max31865_fault & MAX31865_FAULT_REFINHIGH) {
              Serial.println("Error: REFIN- < 0.85 x Bias - FORCE- open"); 
            }
            if (max31865_fault & MAX31865_FAULT_RTDINLOW) {
              Serial.println("Error: RTDIN- < 0.85 x Bias - FORCE- open"); 
            }
            if (max31865_fault & MAX31865_FAULT_OVUV) {
              Serial.println("Error: Under/Over voltage"); 
            }
            
            thermo.clearFault();
          }
          
          setActiveSPI(-1);

      }

// Ekran Tam Güncelleme Fonksiyonu
/*
void epd_update_data(){

    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    setActiveSPI(EPD_CS);
  
    EPD_init(); 
    Paint_SelectImage(BlackImage);
     
      Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 270, WHITE0); 
      Paint_SetScale(4); 
      Paint_SelectImage(BlackImage); 
      Paint_Clear(WHITE0); 

      Paint_DrawString_EN(5, 10, "FAYDAM", &Font16, RED0, WHITE0);       
      Paint_DrawString_EN(5, 25, "TEMPERATURE LOGGER", &Font16, BLACK0, WHITE0);  
      Paint_DrawLine(0, 50, 250, 50, BLACK0, LINE_STYLE_SOLID, DOT_PIXEL_1X1); 

      Paint_DrawString_EN(5,   60, "TH09C", &Font12, BLACK0, WHITE0); 
      Paint_DrawString_EN(85,  60, "DS18B20", &Font12, BLACK0, WHITE0); 
      Paint_DrawString_EN(175, 60, "PT1000", &Font12, BLACK0, WHITE0); 
  
      Paint_DrawRectangle(5, 80, 240, 110, WHITE0, DRAW_FILL_FULL, DOT_PIXEL_1X1);
    
      Paint_DrawString_EN(5,   80, th09c_temp, &Font16, BLACK0, WHITE0);  
      Paint_DrawString_EN(85,  80, ds18_temp,   &Font16, BLACK0, WHITE0); 
      Paint_DrawString_EN(175, 80, max31865_temp, &Font16, BLACK0, WHITE0);  

      PIC_display(BlackImage); 
    
    
      EPD_DeepSleep(); 
      Serial.println("Ekran uykuda. ");


    setActiveSPI(-1);
    SPI.endTransaction();
    }
*/
    //Setup - Döngü Kısmı
    void setup() {

      //GPIO Hold Durumu Deaktif 
      gpio_hold_dis(GPIO_NUM_0);
      gpio_hold_dis(GPIO_NUM_1);

      //Regülatör Aktif , LDO Deaktif
      pinMode(REG_CTL,OUTPUT);
      pinMode(LDO_CTL,OUTPUT);
      digitalWrite(REG_CTL,HIGH);
      delay(300);
      digitalWrite(LDO_CTL,LOW);

      //SPI CS Pinleri Default Deaktif
      pinMode(MAX_CS, OUTPUT); digitalWrite(MAX_CS, HIGH);
      pinMode(EPD_CS, OUTPUT); digitalWrite(EPD_CS, HIGH);

      Serial.begin(115200);
      Serial.println("Initializing...");
      printWakeReason();

      //Deep Sleep Wake Butonu - Active High
      pinMode(4, INPUT_PULLDOWN);
      Serial.println("Boot Edildi");

      pinMode(EPD_BUSY, INPUT);   
      pinMode(EPD_RES, OUTPUT);  
      pinMode(EPD_DC, OUTPUT);   

      //SPI Haberleşmesi tek bir yerde başlatılıyor
      SPI.begin(SPI_CLK, MISO, MOSI); 

      Wire.begin(SDA, SCL); 

      TH09C.begin();

      //ds18.begin();

      //MAX31865 4-WIRE Modu
      //thermo.begin(MAX31865_4WIRE); 

      //Sensör Okuma - Ekran Güncelleme - WiFİ veri gönderme
      //ds18b20_read();
      th09c_read();
      //max31865_read();
      //epd_update_data();
      
      //connectWifi();
      //sendData();

      //delay(15000);                                                     
      
      Serial.println("Kurulum Tamam.");
      delay(2000); 

      //LDO Aktif, Buck Regülatör Deaktif
      digitalWrite(LDO_CTL,HIGH);
      delay(300);
      digitalWrite(REG_CTL,LOW);

      //GPIO Hold Özelliği Aktif
      gpio_hold_en(GPIO_NUM_0);
      gpio_hold_en(GPIO_NUM_1);

      //Uyku Durumundaki Pinler Aktif
      gpio_sleep_sel_en(GPIO_NUM_0);
      gpio_sleep_sel_en(GPIO_NUM_1);

      //Deep Sleep Uyku Modu
      goToSleep();

    
}

void loop() {

}