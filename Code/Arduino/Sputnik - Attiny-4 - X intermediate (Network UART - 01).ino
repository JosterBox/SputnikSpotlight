/* 
  23-2-2018
  Sputnik Spotlight

  Attiny85 @ internal 8mhz clock

  X Stepper UART
  Intermediate ring, controlling rotation


------------Pinout map--------------------
              ____
Reset      5-|°   |-Vcc
Reed    A3=3-|    |-2   Tx       [SCK ]
Stepper2   4-|    |-1   Stepper1 [MISO]
         Gnd-|____|-0   Rx       [MOSI]


*/

#define F_CPU 8000000UL  // 8 MHz

#include <Stepper.h>
#include <SoftwareSerial.h>

// Device identity
int thisDevice = 4;


// Define pins for Serial communication
#define rxPin 0
#define txPin 2

// Start serial
SoftwareSerial sputnikSerial(rxPin, txPin);


// Define steppers outputs
// Ministepper, 5v, is be controlled by L293D driver
#define stepper_COILS_1_2    1   //pin controlling 2 coils of X rotation
#define stepper_COILS_3_4    4
#define MINI_STEPSREVOLUTION    200
#define MINI_SPEED       50     //(RPM)
int stepAngle = 1;     // Adjust this: single steps
float minAngle = 360 / MINI_STEPSREVOLUTION;     

// Start stepper control
Stepper XStepper(MINI_STEPSREVOLUTION, stepper_COILS_1_2, stepper_COILS_3_4);


const int stepperOutput1Pin = stepper_COILS_1_2;
const int stepperOutput2Pin = stepper_COILS_3_4;
const int reedSensorPin = 3;
                 
int i, j, device, lastAngle;

const int maxNumberDevices = 7;
volatile int data[maxNumberDevices];

const int tinyBufferLength = 35;  //7 devices * 5 char ( "n:xxx" command )
char tinyBuffer[tinyBufferLength];
String message;

int rythm = 300;
bool newDataHasArrived;
bool messageIsMine;
bool jobToBeDone;
bool angleIsKnown;
int unknownAngle = 0;
bool reedSensorReading;

unsigned long nowMillis, thenMillis;

void setup()
{
  sputnikSerial.begin(2400);
  
  pinMode(stepperOutput1Pin, OUTPUT);
  pinMode(stepperOutput2Pin, OUTPUT);
  pinMode(reedSensorPin, INPUT);

  XStepper.setSpeed(MINI_SPEED);

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
    if (angleIsKnown == false)
    {
      findXzero();
    }
    if (data[i] < lastAngle)   // Rotate clockwise
    {
      XStepper.step(stepAngle);
      lastAngle = lastAngle - minAngle;
    }
    else                         // Rotate counterclockwise
    {
      XStepper.step(-stepAngle);
      lastAngle = lastAngle + minAngle;
    }
    delay(10); 
  }
}


// Rotate clockwise until reed sensor reveals zero position
void findXzero()
{
  while (angleIsKnown == false)
  {
    reedSensorReading = digitalRead(reedSensorPin);
    if (reedSensorReading == true)
    {
      angleIsKnown = true;
      lastAngle = unknownAngle;
    }
    else
    {
      unknownAngle++;
      XStepper.step(stepAngle);
      delay(10);
    }
  }
}

