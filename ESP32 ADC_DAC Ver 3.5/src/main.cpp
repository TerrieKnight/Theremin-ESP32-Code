// ESP32 ADC/DAC Version 3.5 //

// bring in required libraries 
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <driver/dac.h>

// initialize variables
static esp_adc_cal_characteristics_t adc1_chars;
int pitch_val; 

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 ADC/DAC Version 3.5");
  // ADC characteristics, channel 1 ADC, Attenuated to max 3.9 V,  12 bit precision
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 0, &adc1_chars);
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_12); 
}

void loop() {
  // Recieve pitch value from pin 33 and convert to 8 bit resolution
  pitch_val = adc1_get_raw(ADC1_CHANNEL_5)>>4;

  Serial.print("Pitch : "); 
  Serial.println(pitch_val); 
  delayMicroseconds(25);
}