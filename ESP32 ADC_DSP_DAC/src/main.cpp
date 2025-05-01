// ESP32 ADC/DAC Version 8.75 //

// bring in required libraries 
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <driver/dac.h>
#include <ezButton.h>
#include <esp_dsp.h>
#include <WIFI.h>
#include <esp_now.h>

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
float samp_freq = 22500;
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
boolean SquareWave_State;
boolean TriangleWave_State;
boolean SineWave_State = true; 
unsigned long pre_zero_cross = 0; 
unsigned long curr_period = 1000; 
static int last_low_val = -999;
static int last_low_mid = -999;
static int last_low_high = -999;
int low;
int mid;
int high;
bool harm_data[10];
bool wave_data[3];

struct settings_struct {
  int l;
  int m;
  int h;
  bool h_data[10];
  bool w_data[3];
};

settings_struct settings;

void OnDataRecv (const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&settings, incomingData, sizeof(settings));
  Serial.print("Bytes Received: ");
  Serial.println(len);
  Serial.println("---Data Received---");
  low = settings.l;
  mid = settings.m;
  high = settings.h;
  Serial.printf("Equalizer: %d %d %d\n", low, mid, high);
  Serial.print("Harmonics Data:");
  for (int i = 0; i < 10; i++) {
    harm_data[i] = settings.h_data[i];
    Serial.printf("%d ", harm_data[i]);
  }
  Serial.printf("\nWave Data: ");
  for (int i = 0; i < 3; i++) {
    wave_data[i] = settings.w_data[i];
    Serial.printf("%d ", wave_data[i]);
  }
}

// SQAURE WAVE FUNCTION //
float SquareWave(float signal){
  // convert current sample to high or low based on treshold and return
  signal = (signal > mid_dig_volt) ? max_dig_volt : 0;  
  return signal;
}// end sqaure function 

// TRIANGLE WAVE FUNCTION //
float TriangleWave(float signal, float pre_signal, unsigned long curr_period, float &tri_val){
  // determine step sized based off period with 100 steps per rise/fall
  int tri_steps = max(1, (int)(curr_period / 2) / 100);  
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
  float t = micros() / 1e6; 

  // generate sine wave using fundamental freq and harmonic number
  // add it to current signal 
  harm_signal += (signal / num_harm) * sin(2 * PI * num_harm * fund_freq * t);

  // half wave rectification 
  harm_signal = max(harm_signal, 0.0f);

  // make sure value never surpasses threshold 
  harm_signal = constrain(harm_signal, 0, 255);

  return harm_signal;
}// end harmonics function 

// EQUALIZER COEFFICIENTS FUNCTION //
void set_biquad_coefs(float f0_low, float f0_mid, float f0_high, float Fs, float qFactor){
  // normalize filter cut off frequnecies to range 0 - 0.5 
  float f_low = f0_low / (Fs / 2.0);
  float f_mid = f0_mid / (Fs / 2.0);
  float f_high = f0_high / (Fs / 2.0);

  // calculate filter coefficients 
  dsps_biquad_gen_lpf_f32(coef_low, f_low, qFactor);
  dsps_biquad_gen_bpf_f32(coef_mid, f_mid, qFactor); 
  dsps_biquad_gen_hpf_f32(coef_high, f_high, qFactor);
}// end equilizer coefficient function

// EQAULIZER FUNCTION //
float EQfunction(float in_signal, float db_low, float db_mid, float db_high){
  float out_low = 0;
  float out_mid = 0;
  float out_high = 0;
  float out_signal = 0;

  // calculate amplitude from db gain 
  float amp_low = pow(10.0, db_low / 20.0);
  float amp_mid = pow(10.0, db_mid / 20.0);
  float amp_high = pow(10.0, db_high / 20.0);

  // use biquad iir filter for each band 
  dsps_biquad_f32_ae32(&in_signal, &out_low, 1, coef_low, w_low);
  dsps_biquad_f32_ae32(&in_signal, &out_mid, 1, coef_mid, w_mid);
  dsps_biquad_f32_ae32(&in_signal, &out_high, 1, coef_high, w_high);
  
  // combine bands back together for output
  out_signal = amp_low*out_low + amp_mid*out_mid + amp_high*out_high;
  return out_signal;
}// end equilizer function 

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 ADC/DAC Version 8.75");
  // Station (STA) MAC Address
  Serial.println(WiFi.macAddress());
  WiFi.mode(WIFI_STA);
  esp_err_t esp_wifi_set_channel(1);
  // ADC characteristics, channel 1 ADC, Attenuated to max 3.9 V,  12 bit precision
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 0, &adc1_chars);
  // ADC attenuation for pin 33
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_12); 
  dac_output_enable(DAC_CHANNEL_2);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }  
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}// end void set up 

void loop() {

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
    else{
      curr_freq = 0;
    }
  }

  // Get states from touch screen
  SquareWave_State = wave_data[2];
  TriangleWave_State = wave_data[1];
  SineWave_State = wave_data[0];

  // Equilizer modification
  gain_low = low;
  gain_mid = mid;
  gain_high = high;

  // Waveform selection
  if (SquareWave_State) {
    fin_pitch_val = SquareWave(curr_pitch_val);
  } else if (TriangleWave_State) {
    fin_pitch_val = TriangleWave(curr_pitch_val, pre_pitch_val, curr_period, tri_val);
  } else if (SineWave_State) {
    fin_pitch_val = curr_pitch_val;
  } else {
    fin_pitch_val = curr_pitch_val;
  }

  // Harmonics
  for (int i = 0; i < 10; i++) {
    if (harm_data[i]) {
      fin_pitch_val = AddHarmonics(fin_pitch_val, curr_freq, i + 1);
    }
  }

  // Equilizer
  if (low != last_low_val || mid != last_low_mid || high != last_low_high) {
    set_biquad_coefs(f0_low, f0_mid, f0_high, samp_freq, Q);
    last_low_val = low;
    last_low_mid = mid;
    last_low_high = high;
  }

  if (gain_low != 0 || gain_mid != 0 || gain_high != 0) {
    fin_pitch_val = EQfunction(fin_pitch_val, gain_low, gain_mid, gain_high);
  }

  // Output corresponding analogue value, one shot mode
  dac_output_voltage(DAC_CHANNEL_2, fin_pitch_val);
  pre_pitch_val = curr_pitch_val;
  
}// end main loop