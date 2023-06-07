// Wire Slave Sender
// by Nicholas Zambetti <http://www.zambetti.com>
// Modified by Richard Gemmell Oct 2019

// Demonstrates use of the Wire library
// Sends data as an I2C/TWI slave device
// Refer to the "Wire Master Reader" example for use with this
// To use it, connect a master to the Teensy on pins 16 and 17.
//
// Consider using the I2CRegisterSlave class instead of Wire to
// create an I2C slave.

// Created 29 March 2006

// This example code is in the public domain.

#include <Arduino.h>
#include <i2c_driver.h>
#include <i2c_driver_wire.h>
#include <string.h>

void requestEvent(); //when RPi requests data from arduino, call this function to send duty cycles to RPi
void receiveEvent(int _); //when RPi sends data to this device, call this function to parse. the int arg is required by Wire library

const int N = 16; //number of SSRs
float duty_cycles[N];

//flags for message request packetization. 
bool done_writing_packets = true; // goes false when a request is made. 
String write_buffer[256]; // a write buffer which is an array of Strings, at most 256 elements long (very long message), each 32 bytes
int buffer_write_counter = 0;



int led = LED_BUILTIN;

void setup()
{
  pinMode(led, OUTPUT);
  //Wire1 comes from i2c driver library
  Wire1.begin(8);        // join i2c bus with address #8
  Wire1.onRequest(requestEvent); // register event
  Wire1.onReceive(receiveEvent); // register receive event
  Serial.begin(9600);
}

void loop()
{
  delay(100);
}


void createDataMessage()
{
  //temporarily generate random duty cycles, as this is simulated firmware
  for(int i=0; i < N; i++)
  {
    duty_cycles[i] = random(0, 100)/100.0;
  }
  String message = "A1:{"; //make whatever format you like. 
  for(int i=0; i < N; i++)
  {
    message += String(duty_cycles[i]);
    if(i != N-1)
    {
      message += ",";  
    }
  }
  message += "}";
  Serial.println(message);
  int buffer_counter = 0;
  for(int i = 0; i < message.length(); i+=31)
  {
    String substr;
    if(i+31 > message.length())
    {
      substr = message.substring(i);
    }
    else
    {
      substr = message.substring(i, i+31);
    }
    write_buffer[buffer_counter] = substr;
    buffer_counter += 1;
  }
  //end of message code
  write_buffer[buffer_counter] = "DEADBEEF";

}

//The node-red requests data from the arduino, but it can only
//get 32 bytes at a time. So, this requestEvent function is called
//for each 32 byte packet, but the packets themselves are buffered
//to include the SSR data until it "finishes" and sends an "end of message"
//byte code. At which point, global variables are switched to reform a new message
//at the next request. 
void requestEvent()
{
  digitalWrite(led, HIGH);      // briefly flash the LED
  //form message out of current duty cycles, with 0.0 representing "off" 

  //if this is the first time getting a request, packetize
  //new data to send out of a buffer. 
  if(done_writing_packets == true)
  {
    done_writing_packets = false;
    createDataMessage(); //stores message in a global variable
  }

  String message = write_buffer[buffer_write_counter];
  char mes[message.length()+1];
  message.toCharArray(mes, message.length()+1);
  buffer_write_counter += 1;
  
  Wire1.write(mes);    
  Serial.print("Sent: ");
  Serial.print(mes);
  Serial.println();
  digitalWrite(led, LOW);

  if(message == "DEADBEEF")
  {
    done_writing_packets = true;
    buffer_write_counter = 0;
  }
}

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int _)
{
  digitalWrite(led, HIGH);       // briefly flash the LED
  int count = 0;
  while(Wire1.available() > 0) {  // loop through all but the last
    char c = Wire1.read();        // receive byte as a character
    count += 1;
    Serial.print(c);             // print the character
  }
  Serial.print(": num bytes was ");
  Serial.println(count);             // print the integer
  digitalWrite(led, LOW);
}
