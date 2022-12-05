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
    - MANUAL
- TEMP_CUR
- PIDP_SET



# To do for devs
- We want to change the ramp functionality inputs so that the UI just presses "start" or "stop" and the linear ramp curve is executed automatically (no timing arguments). Target temp and ramp rate are still args, but even ramp rate can have a default of 1 C/min. 

- The heartbeat message is never retreiving actual values of the duty_cycle or state, for some reason it grabs totally stale/default/constructor values even if the teensies hardware values have been changed. 


