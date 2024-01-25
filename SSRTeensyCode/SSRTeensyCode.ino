/********************************************************************************************************
* This code allow a teensy micro controller to be remote controlled through an HC-06 bluetooth module and
* set the duty cycle of Solid State Relay (SSR) via pulse width modulation.
* The SSR can have its duty cycle set directly or set via a PID loop using 
* tempurature as error parremeter.
* File name: MAGIS_SSR_PWM_16_Channel_Jan03_2023
**********************************************************************************************************/
#include <vector>
#include <tuple>
#include <string>
#include <sstream>
#include <iomanip>
#include <math.h>
#include <stdio.h>
#include <i2c_driver.h>
#include <i2c_driver_wire.h>
//#include <SoftwareSerial.h>


#define USING_MICROS_RESOLUTION       true  //false
#include "Teensy_Slow_PWM.h"
// #include <SimpleTimer.h>   // https://urldefense.proofpoint.com/v2/url?u=https-3A__github.com_jfturcot_SimpleTimer&d=DwIGAg&c=gRgGjJ3BkIsb5y6s49QqsA&r=TJ9aEByS9_NO7MQhkCEpqWQyQawKWPYAeJgamfBbX0A&m=nh0jcz0LzrAXE0M1gxpVxeOtlCtCoKXpaDdsmlDy6arnknitdBYzc5WMF5MEw_90&s=nFWlTIp1-UZ4jbI-mINEazQhmzbxpvtFaT7doIhNyqU&e= 

// Used to indicate errors in function returns.
#define ERROR_TEMP -6666.0
#define EARLIEST_TIME 1577836800

/////////////////////////////////////
//Start lines for Slow PWM objects
#if defined(__IMXRT1062__)
  // For Teensy 4.0 and 4.1
  // Don't change these numbers to make higher Timer freq. System can hang
  #define HW_TIMER_INTERVAL_MS        0.01f
  #define HW_TIMER_INTERVAL_FREQ      100000L
#else
  // Don't change these numbers to make higher Timer freq. System can hang
  #define HW_TIMER_INTERVAL_MS        0.1f
  #define HW_TIMER_INTERVAL_FREQ      10000L
#endif
#define USING_HW_TIMER_INTERVAL_MS        true
volatile uint32_t startMicros = 0;

//these two lines are to fix an error in the linker
//of C, which is pretty dependent on (1) which teensyduino
//ide you are user, whether in "Tools > Optimize" you are set
//to "Faster", or other factors. It is related to copy constructor
//of vectors and strings in STL. 
unsigned __exidx_start;
unsigned __exidx_end;

// Create a buffer for the i2c serial line
String outputBuffer[256]; //array of strings, dynamically allocated memory here could cause an issue
//flags for message request packetization, which at 32 byte packets (32 chars)
bool doneWritingPackets = true; // goes false when a request for packets is made. 
int buffer_write_counter = 0; // counter for packetizing the write buffer
const char* startChar = "#"; // messages from the RPi (commands) must start with this char 
String endPacket = "DEADBEEF"; //flag the end of a message
String teensyID = "Teensy #0"; //when installing on Teensy's, give them ID numbers here
//this is a 7 bit address. 
const int i2cAddress = 0; //address of this teensy on the i2c bus

//Timer object, selects TEENSY_TIMER_1 from hardware definitions.
//Don't change that number unless you read the manual of TeensyTimer
TeensyTimer ITimer(TEENSY_TIMER_1);

// Init Teensy_SLOW_PWM, each can service 16 different ISR-based PWM channels
Teensy_SLOW_PWM ISR_PWM;
float PWM_Freq = 5.0f; //Hz
int loop_interval = 300; //milliseconds, in the loop() function for delay
float numberOfSecondsWithoutMessage = 0;
int led = LED_BUILTIN;

void TimerHandler()
{ 
  ISR_PWM.run();
}
//End Slow PWM object setup
////////////////////////////////////

// Indicates which pins of the Teensy are connected to the SSR control pins/lines.
const int numberOfRelays = 16;

//for the PCB version "ssr-v1b" which is a single Teensy, 16 SSR controller prototype board with terminal block connector
//const int relayPinList[numberOfRelays] = {3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 14, 12, 18, 15, 22, 19};

//for the PCB version "ssr-v2b" that is meant for 4 Teensy's
const int relayPinList[numberOfRelays] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 18, 19, 22};

char print_buf[250]; // For printing errors easily


// This is class which creates a PID controller for one SSR output
// for details on how a PID loop works read this wiki article: https://urldefense.proofpoint.com/v2/url?u=https-3A__en.wikipedia.org_wiki_PID-5Fcontroller&d=DwIGAg&c=gRgGjJ3BkIsb5y6s49QqsA&r=TJ9aEByS9_NO7MQhkCEpqWQyQawKWPYAeJgamfBbX0A&m=nh0jcz0LzrAXE0M1gxpVxeOtlCtCoKXpaDdsmlDy6arnknitdBYzc5WMF5MEw_90&s=xrfppQZv9nH49JQMQmunMUHKPnsYxJJ4qcO5PVkA0ns&e= 
class SSRController {
  public:
  
    SSRController(){}
    SSRController(int SSRPin)
    {
      _pin = SSRPin;
      setupPWM();
      setDutyCycle(0.0);
    }
    ~SSRController(){}

    // State indicates the type of set point function the PID controller should use.
    // CONST indicates a constant tempurature that the controller aims for
    // RAMP indicates a linear increase or decrease in tempureture
    // POWER_OFF sets the duty cycle to 0
    enum State { CONST, RAMP, POWER_OFF, MANUAL };
    State current_state = State::POWER_OFF;
    
    float getDutyCycle()
    {
      return duty_cycle;
    }
    
    void enterNewPIDParameters(float newKP, float newKI, float newKD)
    {
      K_p = newKP;
      K_i = newKI;
      K_d = newKD;
    }
    
    void enterNewSetPoint(State newState, float newTarget, float newRamp, long long newStartTime, float newStartTemp)
    {
      if (newState == State::POWER_OFF)
      {
        setDutyCycle(0.0);
      }

      if (newState == State::MANUAL)
      {
        target_temp = newTarget;
        setDutyCycle(newTarget);
      }
      
      current_state = newState;
      target_temp = newTarget;
      ramp = newRamp;
      start_time = newStartTime;
      start_temp = newStartTemp;
    }
    
    void enterNewTemp(float temp, long long time_ms)
    {
      if(temp_record.size() > MAX_RECORD_SIZE)
      {
        temp_record.erase(temp_record.begin());
        error_record.erase(error_record.begin());
      }
        
      temp_record.push_back(std::tuple<float, long long>(temp, time_ms));
      //        Serial.print("New temp added ");
      //        Serial.println(temp);
      if(current_state != State::POWER_OFF){
        float set_point_temp = setPointFunc(time_ms);
        float error = set_point_temp - temp;
        error_record.push_back(std::tuple<float, long long>(error, time_ms));
      //          sprintf(print_buf, "New Record\terror: %f\ttemp: %f\tset point: %f", error, temp, set_point_temp);
      //          Serial.println(print_buf);
      } else {
        error_record.push_back(std::tuple<float, long long>(0.0, time_ms));
      }
    }

    
    void updatePID()
    {
      if(current_state == State::POWER_OFF)
      {
        setDutyCycle(0.0);
        return;
      }

      if(current_state == State::MANUAL)
      {
        setDutyCycle(target_temp);
        return;
      }
    
      int record_length = error_record.size();
    
      if(record_length < 2)
      {
        return;
      } 
    
      float proportional = std::get<0>(error_record[record_length-1]);
    
      float integral = 0;
      for(int i = 1; i < record_length; i++)
      {
        std::tuple<float, long long> prev_error = error_record[i-1];
        std::tuple<float, long long> curr_error = error_record[i];
        float average_temp = (std::get<0>(curr_error) + std::get<0>(prev_error))/2.0;
        long long delta_t = std::get<1>(curr_error) - std::get<1>(prev_error);
        //        sprintf(print_buf, "Average error: %f, delta t: %lld", average_temp, delta_t);
        //        Serial.println(print_buf);
        if(delta_t > 60*1000 || delta_t < 0)
        {
          Serial.print("ERROR: Invalid time difference: ");
          //Serial.println(delta_t);
          //Serial.println(std::get<1>(curr_error));
          //Serial.println(std::get<1>(prev_error));
          error_record.erase(error_record.begin() + i - 1);
          temp_record.erase(temp_record.begin() + i - 1);
          error_record.erase(error_record.begin() + i);
          temp_record.erase(temp_record.begin() + i);
          i--;
          delta_t = 0;
        }
        integral += average_temp*delta_t;
      }
    
      //      sprintf(print_buf, "Total integral: %f Total time: %lld", integral, std::get<1>(error_record[record_length-1]) - std::get<1>(error_record[0]));
      //      Serial.println(print_buf);
      integral = integral/(std::get<1>(error_record[record_length-1]) - std::get<1>(error_record[0]));
    
      std::tuple<float, long long> prev_error = error_record[record_length - 2];
      std::tuple<float, long long> curr_error = error_record[record_length - 1];
    
      int time_delta = std::get<1>(curr_error) - std::get<1>(prev_error);
      float derivative = 0;
      if(time_delta > 0)
      {
        derivative = (std::get<0>(curr_error) - std::get<0>(prev_error))/time_delta;  
      }
      
    
      float u = K_p*proportional + K_i*integral + K_d*derivative;
      if(u > 100.0)
      {
        u = 100.0;
      }
      
      else if (u < 0.0)
      {
        u = 0.0;
      }
      
    
      sprintf(print_buf, "PID out put was : %f\n\nType\tError\tScaled\nP:\t%f\t%f\nI:\t%f\t%f\nD:\t%f\t%f", u, proportional, K_p*proportional, integral, K_i*integral, derivative, K_d*derivative);
      Serial.println(print_buf);
      setDutyCycle(u);
    }

    
  private:
    // Constants
    const unsigned int MAX_RECORD_SIZE = 60*2; // 2 minutes of record time
  
    // Basic control
    // The pin of the SSR
    int _pin;
    int _isrChan; //the isr channel number referenced by Slow_PWM object
    
    float duty_cycle = 0.0; //from 0 to 100

    
    // Vectors containing prior tempuratures and how far they were from the set function
    std::vector<std::tuple<float, long long>> error_record;
    std::vector<std::tuple<float, long long>> temp_record;

    long long start_time; // In millisecond from Jan 1, 1979 GMT
    float ramp; // In Celcius per millisecond
    float target_temp; // In Celcius
    float start_temp; // In Celcius

    // Parameters for the PID loop
    float THERMAL_MASS = 1.0; // Celcius / Jule 
    float K_p = 30.0; // 11 * Watts per Centigrad = 11 * J/(C*s)
    float K_i = 1.0;  // 11 * Watts per Centigrad Milliseconds = 0.011 * J/(C*s^2)
    float K_d = 1000*2*sqrt(THERMAL_MASS*K_p);    // 11 * Watts Milliseconds per Cenrigrad = 11,000 * J/C

    

    void setupPWM()
    {
      _isrChan = ISR_PWM.setPWM(_pin, PWM_Freq, 0); //initial duty cycle is 0, don't power yet
    }
    
    void setDutyCycle(float percentage)
    {
      duty_cycle = percentage;
      Serial.print(_isrChan);
      Serial.print(": Set duty cycle to new value: ");
      Serial.println(getDutyCycle());
      //the line in the if statement modifies the duty cycle.  
      if(!ISR_PWM.modifyPWMChannel(_isrChan, _pin, PWM_Freq, duty_cycle))
      {
        Serial.print(F("modifyPWMChannel error on channel: "));
        Serial.println(_pin);
      }
    }
    
    float setPointFunc(long long time_ms)
    {
      switch(current_state) 
      {
        case State::CONST:
        return target_temp;
        case State::RAMP:
        {
          if(start_time > time_ms) 
          {
            return start_temp;
          }
          int delta_t = (int)(time_ms - start_time);
          float possible_temp = ramp*delta_t + start_temp;
          if((possible_temp > target_temp && ramp > 0) || (possible_temp < target_temp && ramp < 0))
          {
            return target_temp;
          }
          else
          {
            return possible_temp;
          }
        }
        case State::POWER_OFF:
        case State::MANUAL:
        default:
        return ERROR_TEMP;
      }
    }  
};



// An array of SSR controller pointers to allow 
//the teensy to control multiple channels.
SSRController *Controllers[numberOfRelays]; //initialized in setup

std::string firstHalfOfSplitMessage(""); // This variable may be phased out in time, but is a utility for when messages get cut into multiple pieces. 
// cutoff at the end of one buffer and continue at the start of the next.  This string stores the cut off message so it can be recombined.


//find the number of occurrances in string "s" of target
// string "target"
int count_substring(std::string s, std::string target){
  int occurrences = 0;
  std::string::size_type pos = 0;
  while ((pos = s.find(target, pos )) != std::string::npos) {
    occurrences++;
    pos++;
  }
  return occurrences;
}


// Message Type indicates the type of message sent from the bluetooth module
// NONE indicates some sort of error
// TEMP_SET indicates a comand to set the PID controller set point function for one SSR
// TEMP_CUR indicates the tempurature recorded and the time it was recorded.
enum MessageType { NONE, TEMP_SET, TEMP_CUR };

MessageType parseChannelMessage(std::string msg, SSRController *thisController);

MessageType parseChannelMessage(std::string msg, SSRController *thisController) {
  //check if there are any occurrances of "TEMP_SET=" or "TEMP_CUR=" in the string. If not, print an error. 
  if(count_substring(msg, std::string("TEMP_SET=")) + count_substring(msg, std::string("TEMP_CUR=")) > 1){
      sprintf(print_buf, "ERROR: received frankenstien message: %s<NEWLINETEST>", msg.c_str());
      Serial.println(print_buf);
      return MessageType::NONE;
    }
    // if the first few chars of the message are TEMP_SET.
    // Note that Evan is not so happy with all the hard coded indices
    // in this code. but it seems to work, so not changing it at the moment. 
    if (msg.substr(0,9) == "TEMP_SET=") {
      size_t commas[4];
      size_t last_comma = 9;
      for(int i = 0; i < 4; i++) {
        commas[i] = msg.find_first_of(",", last_comma+1);
        if(commas[i] == std::string::npos){
          sprintf(print_buf, "ERROR: TEMP_SET message failed to parse due to absense of a comma. \n See message: %s", msg.c_str());
          Serial.println(print_buf);
          return MessageType::NONE;
        }
        last_comma = commas[i];
      }
      
      std::string state = msg.substr(9, commas[0] - 9);
      SSRController::State newState = SSRController::State::POWER_OFF;
      float target_temp = ERROR_TEMP;
      float ramp = ERROR_TEMP;
      long long start_time = 0;
      float start_temp = -1 * ERROR_TEMP; // This needs to be positive and large so that at no point does the PID think
      // that the current temperature is very far below the setpoint, which would turn the heater on full blast momentarily
      // which is not desired
      
      Serial.println(state.c_str());
      if (state.compare("POWER_OFF") == 0) {
        newState = SSRController::State::POWER_OFF;
        Serial.println("POWER_OFF: Shutting down SSR.");
      } else if (state.compare("MANUAL") == 0) {
        newState = SSRController::State::MANUAL;
        target_temp = atof(msg.substr(commas[0]+1, commas[1]-commas[0]).c_str());
        sprintf(print_buf, "Manually setting duty cycle to %f", target_temp);
        Serial.println(print_buf);
      } else if (state.compare("CONST") == 0) {
        newState = SSRController::State::CONST;
        target_temp = atof(msg.substr(commas[0]+1, commas[1]-commas[0]).c_str());
        sprintf(print_buf, "Setting constant tempurature to %f", target_temp);
        Serial.println(print_buf);
      } else if (state.compare("RAMP") == 0) {
        newState = SSRController::State::RAMP;
        target_temp = atof(msg.substr(commas[0]+1, commas[1]-commas[0]).c_str());
        ramp = atof(msg.substr(commas[1]+1, commas[2] - commas[1]).c_str());
        start_time = strtoll(msg.substr(commas[2]+1, commas[3] - commas[2]).c_str(), NULL, 0);
        start_temp = atof(msg.substr(commas[3]+1).c_str());
        sprintf(print_buf, "Starting ramp: %f, %f, %lld, %f", target_temp, ramp, start_time, start_temp);
        Serial.println(print_buf);
      } else {
        sprintf(print_buf, "ERROR: TEMP_SET had invalid state. \n See message: %s", msg.c_str());
        Serial.println(print_buf);
        return MessageType::NONE; 
      }

      thisController->enterNewSetPoint(newState, target_temp, ramp, start_time, start_temp);
      return MessageType::TEMP_SET;
    } 

    //set parameters of the PID loop, p, i, and d
    if(msg.substr(0,9) == "PIDP_SET=") {
      size_t commas[2];
      size_t last_comma = 9;
      for(int i = 0; i < 2; i++) {
        commas[i] = msg.find_first_of(",", last_comma+1);
        if(commas[i] == std::string::npos){
          sprintf(print_buf, "ERROR: TEMP_SET message failed to parse due to absense of a comma. \n See message: %s", msg.c_str());
          Serial.println(print_buf);
          return MessageType::NONE;
        }
        last_comma = commas[i];
      }
      float K_p = atof(msg.substr(9, commas[0]).c_str());
      float K_i = atof(msg.substr(commas[0]+1, commas[1] - commas[0]).c_str());
      float K_d = atof(msg.substr(commas[1]+1).c_str());
      sprintf(print_buf, "Setting PID parameters to: %f, %f, %f", K_p, K_i, K_d);
      Serial.println(print_buf);

      thisController->enterNewPIDParameters(K_p, K_i, K_d);
    }
    
    if (msg.substr(0,9) == "TEMP_CUR=") {
      size_t comma_loc = msg.find(",", 9);
      if(comma_loc == std::string::npos){
        sprintf(print_buf, "ERROR: TEMP_CUR message failed to parse due to absense of a comma. \n See message: %s", msg.c_str());
        Serial.println(print_buf);
        return MessageType::NONE;
      }
      float temp = atof(msg.substr(9, comma_loc - 9).c_str());
      long long time_ms = strtoll(msg.substr(comma_loc+1).c_str(), NULL, 0);
      if (time_ms < EARLIEST_TIME) {
        sprintf(print_buf, "Recieved invalid time: %lld from message %s<NEWLINETEST>", time_ms, msg.c_str());
        Serial.println(print_buf);
        return MessageType:: NONE;
      }
//      sprintf(print_buf, "Recieved current tempurature: %f at time %lld\n", temp, time_ms);
//      Serial.println(print_buf);
      thisController->enterNewTemp(temp, time_ms);
      thisController->updatePID();
      return MessageType::TEMP_CUR; 
    }

    return MessageType::NONE;
}

MessageType parseCommand(std::string msg);

//used to be called parseBluetoothMessage. 
//this function separates particles of the message
//to determine the channel that is being addressed, and 
//apply the command. 
MessageType parseCommand(std::string msg){
    if (msg[0] != '#'){
      sprintf(print_buf, "ERROR: received message without opening hashtag: %s", msg.c_str());
      Serial.println(print_buf);
      return MessageType::NONE;
    }

    uint colon_pos = msg.find_first_of(":");

    if(colon_pos == std::string::npos) {
      sprintf(print_buf, "ERROR: received message without seperating colon: %s", msg.c_str());
      Serial.println(print_buf);
      return MessageType::NONE;
    }
    std::string channel_str = msg.substr(1,colon_pos);

    int channel_index = atoi(channel_str.c_str());
    
    if(channel_index >= numberOfRelays){
      sprintf(print_buf, "ERROR: received message with channel out of range 0 to %d: %s", numberOfRelays-1, channel_str.c_str());
      Serial.println(print_buf);
      return MessageType::NONE;
    }
    
    if(channel_index == -1){
      sprintf(print_buf, "ERROR: received message without valid channel: %s", channel_str.c_str());
      Serial.println(print_buf);
      return MessageType::NONE;
    }
    
    SSRController *thisController = Controllers[channel_index];
    return parseChannelMessage(msg.substr(colon_pos+1), thisController);
}


//this function parses a message received by the RPi and determines if it
//is split into two pieces or has an error before passing it to the parseBluetoothMessage function. 
//Evan is convinced that this function will begin to be not necessary now that we
//are moving to i2c option. It used to be called parseBluetoothBuffer. 
//So I am putting a few print statements in Serial so that if as we debug,
//we never see it happening, we can remove this stage. 
void checkCommandFormat(std::string buf){

  // Loop through the bluetooth buffer and seperate lines into individual messages
  size_t old_found = -1;
  size_t found = 0;
  std::string message = "";
  while(found != std::string::npos){
    found = buf.find_first_of("\n", old_found+1);

    message = "";
    if(found != std::string::npos){
      message = buf.substr(old_found+1, found-old_found-1);
    } 
    else if(found+1 < buf.length()) { // This condition means that the buffer did not end in a newline.
      // Therefore, the message is cutoff and we need to store the cutoff half.
      Serial.println("ERROR: Message was not terminated with a newline. Storing the cutoff half of the message.");
      firstHalfOfSplitMessage = buf.substr(old_found+1, buf.length()-old_found-1);
    }
    
    if(old_found == (const size_t)-1 && firstHalfOfSplitMessage.length() != 0){ // recombine split messages.
      Serial.println("Found a split message. Re-combining it with the first half of the previous message.");
      message = firstHalfOfSplitMessage + message;
      firstHalfOfSplitMessage = "";
    }

    if(message.length() > 1) {
       parseCommand(message); 
    }
      
    old_found = found;
  }
}


// Initialize all of pins need, the serial channel for connecting to the HC-06 bluetooth module,
// and initialize a normal serial output for debuging
void setup()   {  
  //setup i2c communication
  //Wire1 comes from i2c driver library
  Wire1.begin(i2cAddress);        // join i2c bus with address defined at top of the script
  Wire1.onRequest(requestEvent); // RPi requests data from Teensy, such as SSR current duty cylcles and heartbeat. 
  Wire1.onReceive(receiveEvent); // Teensy receives data from RPi that tells it to change its SSR values



  Serial.begin(9600);
  Serial.print("Testing Magis SSR with ID: ");
  Serial.println(teensyID);

  ////////////////////////////////
  //Setup associated with Slow PWM
  #if USING_HW_TIMER_INTERVAL_MS
    // Interval in microsecs
    if (ITimer.attachInterruptInterval(HW_TIMER_INTERVAL_MS * 1000, TimerHandler))
    {
      startMicros = micros();
      Serial.print(F("Starting ITimer OK, micros() = ")); Serial.println(startMicros);
    }
    else
      Serial.println(F("Can't set ITimer correctly. Select another freq. or interval"));
      
  #else
  
    if (ITimer.attachInterrupt(HW_TIMER_INTERVAL_FREQ, TimerHandler))
    {
      Serial.print(F("Starting  ITimer OK, micros() = ")); Serial.println(micros());
    }
    else
      Serial.println(F("Can't set ITimer. Select another freq. or timer"));
      
  #endif
  //End Slow PWM setup
  ////////////////////////////////

  //this SSRController construction initializes the ISR_PWM channels, calling setPWM. 
  //(thus must come after the ISR_PWM setup above). 
  for(int i = 0; i < numberOfRelays; i++) {
    Controllers[i] = new SSRController(relayPinList[i]);              
  }

  
}

//this is just a flip-flop bit in case you think variables are frozen
bool heartbeat = true; 

extern float tempmonGetTemp(void);



void loop() {
  //this gets reset in the requestEvent and receiveEvent functions of the i2c event handler. 
  numberOfSecondsWithoutMessage += float(loop_interval)/1000; //units of seconds, as loop interval is milliseconds
  
  if(numberOfSecondsWithoutMessage > 60) {
    Serial.println("ERROR: No bluetooth messages for over a minute. Shuting down Solid State Relays to avoid uncontrolled heating.");
    for (int i = 0; i < numberOfRelays; i++) {
      Controllers[i]->enterNewSetPoint(SSRController::State::POWER_OFF, 0.0, 0.0, 0.0, 0.0); 
    }
    numberOfSecondsWithoutMessage = 0;
  }
  
  delay(loop_interval);
}

std::string getHeartBeatPacket() {
  std::ostringstream buf;
  buf << '?' << heartbeat << '&' << std::fixed << std::setprecision(2);
  for (int i = 0; i < numberOfRelays; i++) {
    buf << Controllers[i]->getDutyCycle() << '&';
  }
  buf << tempmonGetTemp() << '\n';
  heartbeat = !heartbeat;
  return std::string(buf.str().c_str());
}

//takes an input message and packetizes it into the outputBuffer string array
void packetizeMessage(std::string m_input)
{
  String message = String(m_input.c_str());
  int buffer_counter = 0; //indexes the array of strings in outputBuffer
  for(int i = 0; i < int(message.length()); i+=31)
  {
    String substr; //substring 32 byte packet
    if(i+31 > int(message.length()))
    {
      substr = message.substring(i);
    }
    else
    {
      substr = message.substring(i, i+31);
    }
    outputBuffer[buffer_counter] = substr.c_str();
    buffer_counter += 1;
  }
  //end of message code
  outputBuffer[buffer_counter] = endPacket;
}

//The RPi requests a 32 byte packet from the teensy.
// There is a flag marking whether the teensy needs
//to form a new many-packet response or whether the request
//corresponds to one of the packets in a many-packet response. 
void requestEvent()
{
  numberOfSecondsWithoutMessage = 0; //reset the counter for how long it has been since a message was received.
  digitalWrite(led, HIGH); // briefly flash the LED

  //form message out of current duty cycles, with 0.0 representing "off" 

  //If this request corresponds to a resquest for a fully
  //formed new message (and is not requesting a 32 byte packet 
  //that is a sub-packet of a message), then form the new message
  //(the heartbeat with duty cycles) and packetize it. 
  if(doneWritingPackets == true)
  {
    doneWritingPackets = false;
    packetizeMessage(getHeartBeatPacket()); //stores the output message in the outputBuffer array of strings
  }

  //one by one, send the 32 byte packets up the RPi as it requests
  //them in a loop. 
  String message = outputBuffer[buffer_write_counter];
  char mes[message.length()+1];
  message.toCharArray(mes, message.length()+1);
  buffer_write_counter += 1;
  
  Wire1.write(mes);    
  Serial.print("Sent: ");
  Serial.print(mes);
  Serial.println();
  digitalWrite(led, LOW);

  //if this is the final packet in the message
  //then reset the message-request flag and buffer counter. 
  if(message == endPacket)
  {
    doneWritingPackets = true;
    buffer_write_counter = 0;
  }
}

// function that executes whenever a command message is
// sent from the raspberry pi to this addressed i2c port. 
void receiveEvent(int _)
{
  numberOfSecondsWithoutMessage = 0; //reset the counter for how long it has been since a message was received.
  digitalWrite(led, HIGH);       // briefly flash the LED
  int count = 0;
  String message = "";
  while(Wire1.available() > 0) {  // loop through all but the last
    char c = Wire1.read();        // receive byte as a character
    count += 1;
    message.concat(c);
  }
  Serial.print("Got message: ");
  Serial.print(message);
  Serial.print("With ");
  Serial.print(count);
  Serial.println(" number of bytes");
  digitalWrite(led, LOW);

  //sends the message up the chain of command processing
  //starting with checking if the command has serious parsing errors. 
  checkCommandFormat(std::string(message.c_str()));
}
