// ESP32 ADC/DAC Version 5.0 //

// bring in required libraries 
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <driver/dac.h>
#include <ezButton.h>

// initialize variables
static esp_adc_cal_characteristics_t adc1_chars;
float pitch_val; 
boolean SqaureWave_State = false; 

// SQAURE WAVE FUNCTION //
float SqaureWave(float signal){
  // convert current sample to high or low based on treshold and return
  signal = (signal > 64) ? 255 : 0;  
  return signal;
}// end sqaure function 

// OBJECT DECLARAIONS //
ezButton RedButton(16);  

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 ADC/DAC Version 5.0");
  // ADC characteristics, channel 1 ADC, Attenuated to max 3.9 V,  12 bit precision
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 0, &adc1_chars);
  // ADC attenuation for pin 33
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_12); 
  dac_output_enable(DAC_CHANNEL_2);
  // set debounce time to 50 milliseconds for red button
  RedButton.setDebounceTime(50); 
}// end void set up 

void loop() {
  // declare needed loops for inputs 
  RedButton.loop(); 

  // Recieve pitch value from pin 33 and convert to 8 bit resolution, one shot mode
  pitch_val = adc1_get_raw(ADC1_CHANNEL_5)>>4;

  // Call square wave function using red button 34pin for led output test
  if (RedButton.isPressed()) {
    SqaureWave_State = !SqaureWave_State; 
  }

  if (SqaureWave_State){
    pitch_val = SqaureWave(pitch_val);
  } 

  // Output corresponding analogue value, one shot mode
  dac_output_voltage(DAC_CHANNEL_2, pitch_val);
}// end main loop
