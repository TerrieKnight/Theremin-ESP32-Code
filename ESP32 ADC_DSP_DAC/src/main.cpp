// ESP32 ADC/DAC Version 5.5 //

// bring in required libraries 
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <driver/dac.h>
#include <ezButton.h>

// initialize variables
static esp_adc_cal_characteristics_t adc1_chars;
const int max_dig_volt = 157;
const int mid_dig_volt = 79;
float curr_pitch_val = 0; 
float fin_pitch_val = 0;
float pre_pitch_val =0;
float tri_val = 0; 
float curr_freq = 1000;
boolean Wave_Up = true;
boolean SquareWave_State = false;
boolean TriangleWave_State = false;
unsigned long pre_zero_cross = 0; 
unsigned long curr_period = 1000; 


// SQAURE WAVE FUNCTION //
float SquareWave(float signal){
  // convert current sample to high or low based on treshold and return
  signal = (signal > mid_dig_volt) ? max_dig_volt : 0;  
  return signal;
}// end sqaure function 

// TRIANGLE WAVE FUNCTION //
float TriangleWave(float signal, float pre_signal, unsigned long curr_period, float &tri_val){
  // determine step sized based off frequency with 100 steps per rise/fall
  int tri_steps = (curr_period / 2) / 100;  
  tri_steps = max(1, max_dig_volt / tri_steps);

  // increment and decrement triangle output based on sine wave
  if (signal > pre_signal) {
    if (tri_val < max_dig_volt) {
        tri_val += tri_steps;
    }
  }
  else if (signal < pre_signal) {
    if (tri_val > 0) {
        tri_val -= tri_steps;
    }
  }

  // make sure value never surpasses threshold
  tri_val = constrain(tri_val, 0, 255);
  return tri_val; 
}// end triangle function 

// HARMONICS FUNCTION

// OBJECT DECLARAIONS //
ezButton RedButton(16);
ezButton YellowButton(17);  

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 ADC/DAC Version 5.5");
  // ADC characteristics, channel 1 ADC, Attenuated to max 3.9 V,  12 bit precision
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 0, &adc1_chars);
  // ADC attenuation for pin 33
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_12); 
  dac_output_enable(DAC_CHANNEL_2);
  // set debounce time to 50 milliseconds for red button
  RedButton.setDebounceTime(50); 
  YellowButton.setDebounceTime(50);
}// end void set up 

void loop() {
  // declare needed loops for inputs 
  RedButton.loop(); 
  YellowButton.loop();

  // Recieve pitch value from pin 33 and convert to 8 bit resolution, one shot mode
  curr_pitch_val = adc1_get_raw(ADC1_CHANNEL_5)>>4;

  // Determine the current period and frequency 
  if (pre_pitch_val < mid_dig_volt && curr_pitch_val >= mid_dig_volt){
    // get time between current crossing (in microseonds) and previous 
    curr_period = micros() - pre_zero_cross; 
    pre_zero_cross = micros();

    if (curr_period > 0){
      // convert from micro to seconds and caluclate frequency 
      curr_freq = 1000000 / curr_period; 
    }
  }

  // Check buttons for function calls 
  if (RedButton.isPressed()) {
    TriangleWave_State = false;
    SquareWave_State = !SquareWave_State; 
  }

  if(YellowButton.isPressed()){
    SquareWave_State = false;
    TriangleWave_State = !TriangleWave_State;
    tri_val = curr_pitch_val;
  }

  // Function calls
  if(SquareWave_State){
    fin_pitch_val = SquareWave(curr_pitch_val);
  } 

  if(TriangleWave_State){
    fin_pitch_val = TriangleWave(curr_pitch_val, pre_pitch_val, curr_period, tri_val);
  }

  if(!SquareWave_State && !TriangleWave_State){
    fin_pitch_val = curr_pitch_val;
  }

  // Output corresponding analogue value, one shot mode
  dac_output_voltage(DAC_CHANNEL_2, fin_pitch_val);
  pre_pitch_val = curr_pitch_val;
  
}// end main loop
