// DTMFDetector implements a detector for DTMF tones.
//  Copyright (C) 2014/2016 Nicola Cimmino
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see http://www.gnu.org/licenses/.
//

// Marker for invalid symbol detected.
#define NO_SYMBOL 255

// Pin powering the mic pre-amp
#define AMPLI_PWR_PIN A3

// DTMF has in total 8 tones, the max being 1633Hz
// this means that 4KHz sample rate are enough (2KHz Nyquist)
#define TONES 8
#define GOERTZEL_N 128
#define SAMPLING_RATE 4000.0

// DTMF tones in Hz.
const int tones_freq[TONES] = { 697, 770, 852, 941, 1209, 1336, 1477, 1633};

// Precalculated coeffs to apply Goertzel
int coeff[TONES];
float Q1[TONES];
float Q2[TONES];

// The samples taken from the A/D.
int sampledData[GOERTZEL_N];

byte last_symbol = NO_SYMBOL;

void setup() {
  
  Serial.begin(115200);
 
  // Set ADC prescaler to 16
  _SFR_BYTE(ADCSRA) |=  _BV(ADPS2); // Set ADPS2
  _SFR_BYTE(ADCSRA) &= ~_BV(ADPS1); // Clear ADPS1
  _SFR_BYTE(ADCSRA) &= ~_BV(ADPS0); // Clear ADPS0
  
  // Power up the amplifier.
  pinMode(AMPLI_PWR_PIN, OUTPUT);
  digitalWrite(AMPLI_PWR_PIN,HIGH);
  
  // Precalculate the Goertzel coeffs.
  // See http://en.wikipedia.org/wiki/Goertzel_algorithm
  for(int i = 0;i < TONES;i++) {
    float omega = (2.0 * PI * tones_freq[i]) / SAMPLING_RATE;
    coeff[i] = 16000 * 2.0 * cos(omega);
  }

}

void loop() {
  
  // Sample at 4KHz 
  long dcoffset = 0;
  for (int ix = 0; ix < GOERTZEL_N; ix++)
  {
    sampledData[ix] = analogRead(A0); // 16uS
    dcoffset += sampledData[ix];
    delayMicroseconds(234); // total 250uS -> 4KHz
  }
  dcoffset = dcoffset / GOERTZEL_N;

  for (int ix = 0; ix < GOERTZEL_N; ix++)
  {
    sampledData[ix] = sampledData[ix] - dcoffset;
  }
    
  // Clear prevous calculated Qs
  for(int i=0; i<TONES ; i++) 
  {
    Q2[i] = 0;
    Q1[i] = 0;
  }
  
  for (int ix = 0; ix < GOERTZEL_N; ix++)
  {
    for(int ix_s=0;ix_s < TONES;ix_s++) {
      float Q0 = (coeff[ix_s]/16000.0f) * Q1[ix_s] - Q2[ix_s] + (float) (sampledData[ix]);
      Q2[ix_s] = Q1[ix_s];
      Q1[ix_s] = Q0;
    }
  }
  
  
  // Calculate the magnitude of each tone.
  // We have here a double peak detector, one is acting
  // on tones 0-3 and the other on tones 4-7 since
  // all DTMF symbols are made up of one tone from each
  // of  the two groups.
  //
  int max_magnitude_a = 100;
  int max_magnitude_b = 100;
  byte max_symbol_a = NO_SYMBOL;
  byte max_symbol_b = NO_SYMBOL;
  
  for(byte i=0;i < TONES;i++) {
    int magnitude = sqrt(Q1[i]*Q1[i] + Q2[i]*Q2[i] - (coeff[i]/16000.0f)*Q1[i]*Q2[i]);
    if(i<4 && magnitude > max_magnitude_a)
    {
      max_magnitude_a = magnitude;
      max_symbol_a = i;
    }
    
    if(i>3 && magnitude > max_magnitude_b)
    {
      max_magnitude_b = magnitude;
      max_symbol_b = i;
    }
  }

  // There is a valid symbol
  if(max_symbol_a != NO_SYMBOL && max_symbol_b != NO_SYMBOL)
  {
     // See http://en.wikipedia.org/wiki/Dual-tone_multi-frequency_signaling
     // for how the tones are disposed in the symbols matrix.
     byte dtmf_symbol = (max_symbol_a*4)+(max_symbol_b-4);
     if(last_symbol != dtmf_symbol)
     {
       Serial.println("123A456B789C*0#D"[dtmf_symbol]);
       Serial.println(max_magnitude_a-max_magnitude_b);
       last_symbol=dtmf_symbol;
     }
  }
  
  // No valid tone this is a break between tones.
  if(max_symbol_a == NO_SYMBOL && max_symbol_b == NO_SYMBOL)
  {
    last_symbol=NO_SYMBOL; 
  }
}









