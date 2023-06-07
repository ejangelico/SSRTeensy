# SSRTeensy

## Libraries
Arduino 1.8.15
Teensydino 1.54



# Communication interface guide (documentation in progress)

There are 16 pulse width modulation (PWM) channels active and available for functions related to setting the temperature of heaters using solid state relays and thermocouple feedback. Each of the channels initializes an "SSRController" object. The object contains a state enumerator for the presently active function of the channel. This section describes how to interface with each channel by sending messages over a serial connection. 

### Available commands

The general structure of a message to the Teensy is in the following form: `#<ch>:<command>=<arg1>,<arg2>,<arg3>,<arg4>,<arg5>` . 

Depending on the command type, there may be a different number of required arguments, and argument parsing stops once the required arguments are parsed. Commands are their arguments are described in the below list. 

- TEMP_SET : command subset for changing the SSR duty cycles to match a setpoint temperature or manual input
    - POWER_OFF : turn SSR relays to 0 duty cycle
        - `#3:TEMP_SET=POWER_OFF,,,`
    - RAMP : linear ramp in temperature up to a setpoint
    - CONST : reach a constant temperature (with no controlled slope ramp) and hold at that temperature
    - MANUAL : manually set the duty cycle of SSRs to some fraction from 0% to 100%
        - `#3:TEMP_SET=MANUAL,<duty cycle 0 to 100>,,,`, `#3:TEMP_SET=MANUAL,43.5,,,`
- TEMP_CUR : provide the Teenys with information about the present temperature associated with a particular SSR channel; this is feedback for a PID loop or any of the TEMP_SET functions
    - `#<ch>:TEMP_CUR=<temp in C>,<timestamp>`, `#3:TEMP_CUR=32.5,1656444270`
- PIDP_SET : set the P, I, and D parameters for the PID control equation



# To do for devs
- We want to change the ramp functionality inputs so that the UI just presses "start" or "stop" and the linear ramp curve is executed automatically (no timing arguments). Target temp and ramp rate are still args, but even ramp rate can have a default of 1 C/min. 


