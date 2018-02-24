/* 
  23-2-2018
  Sputnik Spotlight

  Attiny85 @ internal 8mhz clock

  Wall device - UART
  
  To adapt code on the 3 wall devices, change only variable thisDevice


------------Pinout map--------------------
              ____
Reset      5-|°   |-Vcc
Sensor  A3=3-|    |-2   Tx       [SCK ]
Stepper2   4-|    |-1   Stepper1 [MISO]
         Gnd-|____|-0   Rx       [MOSI]


to do:
- calibrate reading position
- stopper

*/

#define F_CPU 8000000UL  // 8 MHz

#include <Stepper.h>
#include <SoftwareSerial.h>

// Device identity
int thisDevice = 3;


// Define pins for Serial communication
#define rxPin 0
#define txPin 2

// Start serial
SoftwareSerial sputnikSerial(rxPin, txPin);


// Define steppers outputs
//NEMA-17 does 200 steps per revolution, with 1.8° per step
#define stepper_COILS_1_2    1   //pin controlling 2 coils
#define stepper_COILS_3_4    4
#define STEPSREVOLUTION    200
#define STEPPER_SPEED       80     //(RPM)

// Start stepper control
Stepper stepper(STEPSREVOLUTION, stepper_COILS_1_2, stepper_COILS_3_4);


const int stepperOutput1Pin = stepper_COILS_1_2;
const int stepperOutput2Pin = stepper_COILS_3_4;
const int sensorPin = 3;
//const int stopperPin = 5;  //by disabling reset pin
                             //Be aware of consequences!

int steps = 20;  // Adjust this
                 //20 = 1/10 revolution every loop
                 
int i, j, device, readLength;

const int maxNumberDevices = 7;
volatile int data[maxNumberDevices];

const int tinyBufferLength = 35;  //7 devices * 5 char ( "n:xxx" command )
char tinyBuffer[tinyBufferLength];
String message;

int rythm = 300;
bool newDataHasArrived;
bool messageIsMine;
bool jobToBeDone;

unsigned long nowMillis, thenMillis;

void setup()
{
  sputnikSerial.begin(2400);
  pinMode(stepperOutput1Pin, OUTPUT);
  pinMode(stepperOutput2Pin, OUTPUT);
  pinMode(sensorPin, INPUT);

  stepper.setSpeed(STEPPER_SPEED);
// First reading
// Estimate wire length from variable resistor value
// check gears 20:1 reduction and correct
  readLength = map(analogRead(sensorPin), 0, 1023, 0, 450);
}

void loop()
{
  nowMillis = millis();
  // Check new requests
  if (sputnikSerial.available())
  { 
    tinyBuffer[i] = sputnikSerial.read();
    if (int(tinyBuffer[i])==13 || int(tinyBuffer[i])==10 )
    { //If Carriage return has been reached
       i= 0;
      for (j=0; j<=tinyBufferLength; j++)
      {
        //  Check if the message is for this device
        // if (int(tinyBuffer[j]) == 48 and int(tinyBuffer[j+1]) == 58)
        // 49 = "1" 58 = ":"
        if (int(tinyBuffer[j]) == (48+thisDevice) and int(tinyBuffer[j+1]) == 58);
        {
          // Ok, this is for me
          messageIsMine = true;
          j++;  // To jump device number
          j++;  // To jump ":" character
        }
        if (messageIsMine)
        {          
          if (int(tinyBuffer[j]) == 45     // 45 = "-"
              or j == 16                   // end of buffer
              or tinyBuffer[j] == (char) 0 // empty char
              or tinyBuffer[j] == ' '      // emptied char
              or int(tinyBuffer[j])==13    // carriage
              or int(tinyBuffer[j])==10)   // carriage, eol
          {
            messageIsMine = false;
            newDataHasArrived = true;
          }
          else
          {
            message.concat(String(tinyBuffer[j]));
          }
        }
        tinyBuffer[j] = ' ';
      }      
    }
    i++;
  }  //end new request

  // Execute commands
  if (newDataHasArrived)
  {
    jobToBeDone = true;
    message = "";
    data[thisDevice] = message.toInt();
    newDataHasArrived = false;
  }

  if (jobToBeDone)
  {
    // Estimate wire length from variable resistor value
    // check gears 20:1 reduction and correct
    readLength = map(analogRead(sensorPin), 0, 1023, 0, 450);
    if (readLength == data[thisDevice])
    {
      jobToBeDone = false;
    }
    else
    {
      if (data[thisDevice] < readLength)   // Less wire, rotate counterclockwise
      {
        stepper.step(steps);
      }
      else   // More wire, rotate clockwise
      {
        stepper.step(-steps);
      }
    }
  }
}

