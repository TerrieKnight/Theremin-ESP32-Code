// ESP32 ADC/DAC Version 4.0 //

// bring in required libraries 
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <driver/dac.h>
#include <ezButton.h>

// initialize variables
static esp_adc_cal_characteristics_t adc1_chars;
float pitch_val; 
int led_state = LOW;

// SQAURE WAVE FUNCTION //
void SqaureWave(){
  Serial.println("Your in the SqaureWave function!");
  Serial.println("The red button has been pressed");

  // toggle state of LED
  led_state = !led_state;

  // control LED arccoding to the toggleed sate
  digitalWrite(0, led_state);
}// end sqaure function 

// OBJECT DECLARAIONS //
ezButton RedButton(16);  

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 ADC/DAC Version 4.0");
  // ADC characteristics, channel 1 ADC, Attenuated to max 3.9 V,  12 bit precision
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 0, &adc1_chars);
  // ADC attenuation for pin 33
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_12); 
  dac_output_enable(DAC_CHANNEL_2);
  // set pin 0 to output mode to power test led
  pinMode(0, OUTPUT);   
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
    SqaureWave();
  }

  // Output corresponding analogue value, one shot mode
  dac_output_voltage(DAC_CHANNEL_2, pitch_val);

  //Serial.print("Pitch : "); 
  //Serial.println(pitch_val); 
  //delayMicroseconds(25);
}// end main loop
