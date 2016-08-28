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
// I referred to https://github.com/jacobrosenthal/Goertzel for the Goertzel
//  implementation.

// Marker for invalid symbol detected.
#define NO_SYMBOL 255

#define STATUS_WAITING_SYMBOL 1
#define STATUS_WAITING_SYMBOL_REPEAT 2
#define STATUS_WAITING_SYMBOL_REPEAT2 3
#define STATUS_WAITING_INTERSYMBOL 4
byte currentStatus = STATUS_WAITING_SYMBOL;

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
float coeff[TONES];

byte last_symbol = NO_SYMBOL;

void setup() {

  Serial.begin(115200);

  // Set ADC prescaler to 16
  _SFR_BYTE(ADCSRA) |=  _BV(ADPS2); // Set ADPS2
  _SFR_BYTE(ADCSRA) &= ~_BV(ADPS1); // Clear ADPS1
  _SFR_BYTE(ADCSRA) &= ~_BV(ADPS0); // Clear ADPS0

  // Power up the amplifier.
  pinMode(AMPLI_PWR_PIN, OUTPUT);
  digitalWrite(AMPLI_PWR_PIN, HIGH);

  // Precalculate the Goertzel coeffs.
  // See http://en.wikipedia.org/wiki/Goertzel_algorithm
  for (int i = 0; i < TONES; i++) {
    float omega = (2.0 * PI * tones_freq[i]) / SAMPLING_RATE;
    coeff[i] = 2.0 * cos(omega);
  }

  delay(1000);
}

void loop() {

  float Q1;
  float Q2;

  // The samples taken from the A/D.
  int sampledData[GOERTZEL_N];
  
  // Sample at 4KHz
  long dcoffset = 0;
  for (int ixSample = 0; ixSample < GOERTZEL_N; ixSample++)
  {
    sampledData[ixSample] = analogRead(A0); // 16uS
    dcoffset += sampledData[ixSample];
    delayMicroseconds(234); // total 250uS -> 4KHz
  }
  dcoffset = dcoffset / GOERTZEL_N;

  // Remove DC offset so we are left with a signal
  // around zero.
  for (int ixSample = 0; ixSample < GOERTZEL_N; ixSample++)
  {
    sampledData[ixSample] = sampledData[ixSample] - dcoffset;
  }

  int maxMagnitudeL = 100;
  int maxMagnitudeH = 100;
  byte maxSymbolL = NO_SYMBOL;
  byte maxSymbolH = NO_SYMBOL;

  // Calculate Qs
  // (See the wikipedia article and https://github.com/jacobrosenthal/Goertzel)
  // Calculate the magnitude of each tone.
  // We have here a double peak detector, one is acting on tones 0-3 and the other on tones 4-7 since
  // all DTMF symbols are made up of one tone from each of  the two groups.

  unsigned long snrL = 0;
  for (int ixTone = 0; ixTone < TONES; ixTone++) {
    Q2 = 0;
    Q1 = 0;
    for (int ixSample = 0; ixSample < GOERTZEL_N; ixSample++)
    {
      float Q0 = coeff[ixTone] * Q1 - Q2 + (float) (sampledData[ixSample]);
      Q2 = Q1;
      Q1 = Q0;
    }

    int magnitude = sqrt(Q1 * Q1 + Q2 * Q2 - coeff[ixTone] * Q1 * Q2);

    if (ixTone < 4)
    {
      snrL = snrL + magnitude;
      if (magnitude > maxMagnitudeL)
      {
        maxMagnitudeL = magnitude;
        maxSymbolL = ixTone;
      }
    }
    else
    {
      if (magnitude > maxMagnitudeH)
      {
        maxMagnitudeH = magnitude;
        maxSymbolH = ixTone;
      }
    }
  }

  snrL = (snrL - maxMagnitudeL) / (TONES / 2 - 1);

  // See http://en.wikipedia.org/wiki/Dual-tone_multi-frequency_signaling
  // for how the tones are arranged in the symbols matrix.
  byte dtmf_symbol = NO_SYMBOL;
  if (maxSymbolL != NO_SYMBOL && maxSymbolH != NO_SYMBOL && snrL > 100)
  {
    dtmf_symbol = (maxSymbolL * 4) + (maxSymbolH - 4);
  }

  if (currentStatus == STATUS_WAITING_SYMBOL)
  {
    // There is a valid symbol
    if (dtmf_symbol != NO_SYMBOL)
    {
      last_symbol = dtmf_symbol;
      currentStatus = STATUS_WAITING_SYMBOL_REPEAT;
      delay(30);
    }
  }
  else if (currentStatus == STATUS_WAITING_SYMBOL_REPEAT)
  {
    // There is a valid symbol
    if (last_symbol == dtmf_symbol)
    {
      last_symbol = dtmf_symbol;
      currentStatus = STATUS_WAITING_SYMBOL_REPEAT2;
      delay(30);
    }
    else
    {
      currentStatus = STATUS_WAITING_SYMBOL;
    }
  }
  else if (currentStatus == STATUS_WAITING_SYMBOL_REPEAT2)
  {
    if (last_symbol == dtmf_symbol)
    {
      Serial.print("123A456B789C*0#D"[dtmf_symbol]);
      currentStatus = STATUS_WAITING_INTERSYMBOL;
    }
    else
    {
      currentStatus = STATUS_WAITING_SYMBOL;
    }
  }
  else if (currentStatus == STATUS_WAITING_INTERSYMBOL)
  {
    if (dtmf_symbol == NO_SYMBOL)
    {
      currentStatus = STATUS_WAITING_SYMBOL;
      delay(100);
    }
  }
}









