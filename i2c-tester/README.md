
Here are some things to note about this code:

- The i2c package used is “Teensy4 I2C” which is custom but well used. Installation instructions (and the code base) can be found here: https://github.com/Richard-Gemmell/teensy4_i2c/blob/master/documentation/installation/arduino_installation.md 

- The test code in the “i2c-tester” folder has the Teensy as secondary and RPi as primary in the i2c protocol. The i2c “Wire1” object runs a function “requestEvent” when the RPi requests information. It forms a set of 16 random numbers as duty cycles representing present duty cycles of SSRs, packetizes the string and sends it to the RPi. The i2c object runs a function “receiveEvent” when it gets a message from the RPi, parsing it into a string and printing to the standard Serial readout. An end-of-message word is required, which I believe already may exist in our SSRTeensy code as well, or may be easy to implement when we integrate this scheme. 

- The node-red flow is included as a .json object for importing. It uses nodes from https://flows.nodered.org/node/node-red-contrib-i2c . It also has a loop with a sub-function that packetizes and concatenates packets into strings just like the Teensy code does. This will need to be integrated into the existing messaging scheme. 

- Wiring is as follows:
	- The ground of the RPi and the Teensy must be connector (with a wire)
	- The i2c is a 2 wire protocol: SCL (clock) and SDA (data). On the Raspberry Pi 4, this is GPIO pins labeled 3 and 5 referencing the pinout diagram attached (GPIO 2 and 3). On the Teensy 4 (and our new boards) this is pins 16 and 17. The Teensy code requires this and has a comment mentioning it. SDA goes to SDA, SCL goes to SCL between devices. 
	- If you have issues with sending messages, it may be worth trying to power both Teensy and RPi from the same 5V line in addition to them having the same ground. Though I don’t necessarily remember doing this in my test. 

 
