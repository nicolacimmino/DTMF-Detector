
This is a first attempt at a DTMF decoder using the Goertzel alghorithm. The hardware can be seen below, it's an Arduino Nano with a small microphone capsule and a pre-amplifier.

![Proto](documentation/proto.png)

The current iteration works fine and properly recognizes DTMF tones on a quite large volume range. It also detects tones in noies though, so there are many false positives. I am currently investigating how to readuce this since DTMF is meant to be resilient to noise.
