# SSRTeensy

## Libraries
Arduino 1.8.15
Teensydino 1.54



# Communication interface guide (documentation in progress)

There are 16 pulse width modulation (PWM) channels active and available for functions related to setting the temperature of heaters using solid state relays and thermocouple feedback. Each of the channels initializes an "SSRController" object. The object contains a state enumerator for the presently active function of the channel. This section describes how to interface with each channel by sending messages over a serial connection. 

### Available commands
- TEMP_SET 
    - POWER_OFF
    - RAMP
    - CONST
- TEMP_CUR
- PIDP_SET



