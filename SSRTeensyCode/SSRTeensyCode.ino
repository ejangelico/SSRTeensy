/********************************************************************************************************
* This code allow a teensy micro controller to be remote controlled through an HC-06 bluetooth module and
* set the duty cycle of Solid State Relay (SSR) via pulse width modulation.
* The SSR can have its duty cycle set directly or set via a PID loop using 
* tempurature as error parremeter.
**********************************************************************************************************/
#include <vector>
#include <tuple>
#include <string>
#include <sstream>
#include <math.h>
#include <stdio.h>
#include <SoftwareSerial.h>


#define USING_MICROS_RESOLUTION       true  //false
#include "Teensy_Slow_PWM.h"
#include <SimpleTimer.h>   // https://github.com/jfturcot/SimpleTimer

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


//Timer object, selects TEENSY_TIMER_1 from hardware definitions.
//Don't change that number unless you read the manual of TeensyTimer
TeensyTimer ITimer(TEENSY_TIMER_1);

// Init Teensy_SLOW_PWM, each can service 16 different ISR-based PWM channels
Teensy_SLOW_PWM ISR_PWM;
float PWM_Freq = 5.0f; //Hz

void TimerHandler()
{ 
  ISR_PWM.run();
}
//End Slow PWM object setup
////////////////////////////////////



// Indicates which pins of the Teensy are connected to the serial pins of the HC-06
const int blueRx = 0;
const int blueTx = 1;
SoftwareSerial hc(blueRx, blueTx);

// Indicates which pins of the Teensy are connected to the SSR control pins/lines.
const int numberOfRelays = 16;
const int relayPinList[numberOfRelays] = {3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 14, 12, 18, 15, 22, 19};

const String teensyID = "HenrysSSRTeensy";
String bluetoothBuffer = "";

char print_buf[250]; // For printing errors easily




// This is class which creates a PID controller for one SSR output
// for details on how a PID loop works read this wiki article: https://en.wikipedia.org/wiki/PID_controller
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
    enum State { CONST, RAMP, POWER_OFF };
    State current_state = State::POWER_OFF;
    float duty_cycle = 0.0;

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
        float set_point_temp = set_point_func(time_ms);
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
    int isrChan; //the isr channel number referenced by Slow_PWM object
  
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
      isrChan = ISR_PWM.setPWM(_pin, PWM_Freq, 0); //initial duty cycle is 0, don't power yet
    }
    
    void setDutyCycle(float percentage)
    {
      duty_cycle = percentage;
      //the line in the if statement modifies the duty cycle.  
      if(!ISR_PWM.modifyPWMChannel(isrChan, _pin, PWM_Freq, duty_cycle))
      {
        Serial.print(F("modifyPWMChannel error on channel: "));
        Serial.println(_pin);
      }
    }
    
    float set_point_func(long long time_ms)
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
        default:
        return ERROR_TEMP;
      }
    }  
};



// A vector of SSR controller to allow the teensy to control multiple channels.
std::vector<SSRController> Controllers;

std::string firstHalfOfSplitMessage(""); // HC06 will send data at random time intervals so sometimes messages will be 
// cutoff at the end of one buffer and continue at the start of the next.  This string stores the cut off message so it can be recombined.



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

MessageType parseChannelMessage(std::string msg, SSRController thisController);

MessageType parseChannelMessage(std::string msg, SSRController thisController) {
  if(count_substring(msg, std::string("TEMP_SET=")) + count_substring(msg, std::string("TEMP_CUR=")) > 1){
      sprintf(print_buf, "ERROR: received frankenstien message: %s<NEWLINETEST>", msg.c_str());
      Serial.println(print_buf);
      return MessageType::NONE;
    }
  
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
      float start_temp = ERROR_TEMP;
      
      Serial.println(state.c_str());
      if (state.compare("POWER_OFF") == 0) {
        newState = SSRController::State::POWER_OFF;
        Serial.println("POWER_OFF: Shutting down SSR.");
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

      thisController.enterNewSetPoint(newState, target_temp, ramp, start_time, start_temp);
      
      return MessageType::TEMP_SET;
    } 

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

      thisController.enterNewPIDParameters(K_p, K_i, K_d);
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
      thisController.enterNewTemp(temp, time_ms);
      thisController.updatePID();
      return MessageType::TEMP_CUR; 
    }

    return MessageType::NONE;
}

MessageType parseBluetoothMessage(std::string msg);

MessageType parseBluetoothMessage(std::string msg){
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
    
    if(channel_index >= (int) Controllers.size()){
      sprintf(print_buf, "ERROR: received message with channel out of range 0 to %d: %s", Controllers.size()-1, channel_str.c_str());
      Serial.println(print_buf);
      return MessageType::NONE;
    }
    
    if(channel_index == -1){
      sprintf(print_buf, "ERROR: received message without valid channel: %s", channel_str.c_str());
      Serial.println(print_buf);
      return MessageType::NONE;
    }
    
    SSRController thisController = Controllers.at(channel_index);
    return parseChannelMessage(msg.substr(colon_pos+1), thisController);
}


void parseBluetoothBuffer(String bluetoothBuffer){
  std::string buf(bluetoothBuffer.c_str());
  std::vector<std::string> lines; 

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
      firstHalfOfSplitMessage = buf.substr(old_found+1, buf.length()-old_found-1);
    }
    
    if(old_found == (const size_t)-1 && firstHalfOfSplitMessage.length() != 0){ // recombine split messages.
      message = firstHalfOfSplitMessage + message;
      firstHalfOfSplitMessage = "";
    }

    if(message.length() > 1) {
       parseBluetoothMessage(message); 
    }
      
    old_found = found;
  }
}


// Initialize all of pins need, the serial channel for connecting to the HC-06 bluetooth module,
// and initialize a normal serial output for debuging
void setup()   {  
  //setup bluetooth communication
  hc.begin(9600);
  while(hc.available())
  {
    hc.read();
  }

  Serial.begin(9600);
  Serial.println("Testing Magis SSR");
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
    Controllers.push_back(SSRController(relayPinList[i]));              
  }

  
}

bool heartbeat = true;

extern float tempmonGetTemp(void);

void sendHeartBeatPacket() {
  std::ostringstream buf;
  buf << '?' << heartbeat << '&';
  for (int i = 0; i < (int) Controllers.size(); i++) {
    buf << Controllers.at(i).duty_cycle << '&';
  }
  buf << tempmonGetTemp() << '\n';
  hc.write(buf.str().c_str());
  Serial.write(buf.str().c_str());
  heartbeat = !heartbeat;
}


int numberOfSecondsWithoutMessage = 0;

void loop() {
  numberOfSecondsWithoutMessage++;
  
  while(hc.available()) {
    bluetoothBuffer = hc.readString();
//    Serial.println(bluetoothBuffer);  
    parseBluetoothBuffer(bluetoothBuffer);
    numberOfSecondsWithoutMessage = 0;
  }
//  sprintf(print_buf, "%d seconds without a message.", numberOfSecondsWithoutMessage);
//  Serial.println(print_buf);

  if(numberOfSecondsWithoutMessage > 60) {
    Serial.println("ERROR: No bluetooth messages for over a minute. Shuting down Solid State Relays to avoid uncontrolled heating.");
    for (int i = 0; i < (int) Controllers.size(); i++) {
      Controllers.at(i).enterNewSetPoint(SSRController::State::POWER_OFF, 0.0, 0.0, 0.0, 0.0); 
    }
    numberOfSecondsWithoutMessage = 0;
  }
  sendHeartBeatPacket();
  delay(1000);
}
