// ESP32 ADC/DAC Version 6.0 //

// bring in required libraries 
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <driver/dac.h>
#include <ezButton.h>
#include <esp_dsp.h>

// initialize variables
static esp_adc_cal_characteristics_t adc1_chars;
const int max_dig_volt = 157;
const int mid_dig_volt = 79;
float curr_pitch_val = 0; 
float fin_pitch_val = 0;
float pre_pitch_val = 0;
float tri_val = 0; 
float Q = 1;
float curr_freq = 1000;
float samp_freq = 22100;
float gain_low = 0;
float gain_mid = 0;
float gain_high = 0;
float f0_low = 100;
float f0_mid = 1000;
float f0_high = 2000;
float w_low[2] = {0, 0};
float w_mid[2] = {0, 0};
float w_high[2] = {0, 0};
float coef_low[5];
float coef_mid[5];
float coef_high[5];
boolean Wave_Up = true;
boolean SquareWave_State = false;
boolean TriangleWave_State = false;
boolean Harmonics_State = false;
boolean Equilizer_State = false;
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

// HARMONICS FUNCTION //
float AddHarmonics(float signal, float fund_freq, int num_harm){
  float harm_signal = signal;

  // generate sine wave using fundamental freq and harmonic number
  // add it to current signal 
  harm_signal += (signal / num_harm) * sin(2 * PI * num_harm * fund_freq);

  // make sure value never surpasses threshold 
  harm_signal = constrain(harm_signal, 0, 255);

  return harm_signal;
}// end harmonics function 

// EQUALIZER COEFFICIENTS FUNCTION //
void set_biquad_coefs(float f0_low, float f0_mid, float f0_high, float Fs, float qFactor, float db_low, float db_mid, float db_high){
  // calculate the amplification factor for each band using gain in db 
  float A_low = pow(10.0, db_low / 40.0);
  float A_mid = pow(10.0, db_mid / 40.0);
  float A_high = pow(10.0, db_high / 40.0);
  
  // normalize filter cut off frequnecies to range 0 - 0.5 
  float f_low = f0_low / (Fs / 2.0);
  float f_mid = f0_mid / (Fs / 2.0);
  float f_high = f0_high / (Fs / 2.0);

  // calculate filter coefficients 
  dsps_biquad_gen_lpf_f32(coef_low, f_low, qFactor);
  dsps_biquad_gen_bpf_f32(coef_mid, f_mid, qFactor); 
  dsps_biquad_gen_hpf_f32(coef_high, f_high, qFactor);

  // Apply gain for each filter
  for (int i = 0; i < 3; i++) {
    coef_low[i] *= A_low;
    coef_mid[i] *= A_mid;
    coef_high[i] *= A_high;
  }
}// end equilizer coefficient function

// EQAULIZER FUNCTION //
float EQfunction(float in_signal){
  float out_low = 0;
  float out_mid = 0;
  float out_high = 0;
  float out_signal = 0;

  // use biquad iir filter for each band 
  dsps_biquad_f32_ae32(&in_signal, &out_low, 1, coef_low, w_low);
  dsps_biquad_f32_ae32(&in_signal, &out_mid, 1, coef_mid, w_mid);
  dsps_biquad_f32_ae32(&in_signal, &out_high, 1, coef_high, w_high);
  
  // combine bands back together for output
  out_signal = out_low + out_mid + out_high;
  return out_signal;
}// end equilizer function 


// OBJECT DECLARAIONS //
ezButton RedButton(16);
ezButton YellowButton(17);  
ezButton BlueButton(5);
ezButton GreenButton(18);


void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 ADC/DAC Version 6.0");
  // ADC characteristics, channel 1 ADC, Attenuated to max 3.9 V,  12 bit precision
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 0, &adc1_chars);
  // ADC attenuation for pin 33
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_12); 
  dac_output_enable(DAC_CHANNEL_2);
  // set debounce time to 50 milliseconds for buttons
  RedButton.setDebounceTime(50); 
  YellowButton.setDebounceTime(50);
  BlueButton.setDebounceTime(50);
  GreenButton.setDebounceTime(50);
}// end void set up 

void loop() {
  // declare needed loops for inputs 
  RedButton.loop(); 
  YellowButton.loop();
  BlueButton.loop();
  GreenButton.loop();

  // Test temp values for equilizer modification (will be user input in final ver.)
  float gain_low = 3;
  float gain_mid = -1.5;
  float gain_high = 1;

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
    Harmonics_State = false;
    Equilizer_State = false;
    SquareWave_State = !SquareWave_State; 
  }

  if(YellowButton.isPressed()){
    SquareWave_State = false;
    Harmonics_State = false;
    Equilizer_State = false;
    TriangleWave_State = !TriangleWave_State;
    tri_val = curr_pitch_val;
  }

  if(BlueButton.isPressed()){
    SquareWave_State = false;
    TriangleWave_State = false;
    Equilizer_State = false;
    Harmonics_State = !Harmonics_State;
  }

  if(GreenButton.isPressed()){
    SquareWave_State = false;
    TriangleWave_State = false;
    Harmonics_State = false; 
    Equilizer_State = !Equilizer_State;
  }

  // Function calls
  if(SquareWave_State){
    fin_pitch_val = SquareWave(curr_pitch_val);
  } 

  if(TriangleWave_State){
    fin_pitch_val = TriangleWave(curr_pitch_val, pre_pitch_val, curr_period, tri_val);
  }

  if(Harmonics_State){
    fin_pitch_val = AddHarmonics(curr_pitch_val, curr_freq, 2);
  }

  if(Equilizer_State){
    // set coefficinet for biquad iir filter 
    set_biquad_coefs(f0_low, f0_mid, f0_high, samp_freq, Q, gain_low, gain_mid, gain_high);

    // run filter 
    fin_pitch_val = EQfunction(curr_pitch_val);
  }

  if(!SquareWave_State && !TriangleWave_State & !Harmonics_State & !Equilizer_State){
    fin_pitch_val = curr_pitch_val;
  }

  // Output corresponding analogue value, one shot mode
  dac_output_voltage(DAC_CHANNEL_2, fin_pitch_val);
  pre_pitch_val = curr_pitch_val;
  
}// end main loop
