/* 
  17-2-2018
  Sputnik Spotlight

  ATmega328P @ 8mhz internal clock

  Webpage from ESP8266-01
  Controlling 4 Attinys on long wire

Get values from ESP8266-01 via Serial:

1) 90     1st big stepper position, cm
2) 80     2nd big stepper position, cm
3) 180    3rd big stepper position, cm
4) 0      4th Attiny, x position (0-359), degrees
5) 0      Atmega,  y position (0-359), degrees
6) 7      focus microstepper (0-100)
7) 0      dimmer (0-255)  , PWM pin to lamp ground

Stored in array:
data[device]
lastData[device]

Send commands to Attinys via UART code:
1:90-2:80-3:180-4:255

to do:
- check minimum and maximum
- ask reading position

*/

//---------------------------------
// Libraries
//---------------------------------

#include <SoftwareSerial.h>  //To give tasks to Attinys
#include <Stepper.h>         //For 2 steppers control (Y-rotation and focus-slider)
#include <stdio.h>           //for Serial string splitting
#include <string.h>

// Define and start sputnikSerial communication with distant Attinies
#define rxPin 2
#define txPin 3
SoftwareSerial sputnikSerial(rxPin, txPin);
#define INPUT_SIZE 10  //reserve Serial memory


// Ministepper, 5v, is be controlled by L293D driver
#define MINI_STEPSREVOLUTION    200
#define MINI_SPEED               50     //(RPM)
#define Ystepper_COILS_1  17   //17 = A3
#define Ystepper_COILS_2   2
#define Ystepper_COILS_3   3
#define Ystepper_COILS_4   4
int stepAngle  = 1;     // Adjust this: single steps
float minAngle = 360 / MINI_STEPSREVOLUTION;

Stepper YStepper(MINI_STEPSREVOLUTION, Ystepper_COILS_1, Ystepper_COILS_2, Ystepper_COILS_3, Ystepper_COILS_4);

// Microstepper slider could be controlled directly by pins because of low operation current (15-25mA per coil)
#define SLIDER_STEPSREVOLUTION  120   //test this
#define SLIDER_SPEED             20
#define SLIDER_COIL_1       8
#define SLIDER_COIL_2       7
#define SLIDER_COIL_3       6
#define SLIDER_COIL_4       5

int stepFocus = 10;    // Adjust this
Stepper focusStepper(SLIDER_STEPSREVOLUTION, SLIDER_COIL_1, SLIDER_COIL_2, SLIDER_COIL_3, SLIDER_COIL_4);


//---------------------------------
// Settings
//---------------------------------
int rythm = 300;   //cycle frequency

const int reedSensorPin = A2;
const int sliderEndPin = 10;    //simple end-of-run switch
const int dimmerPin = 9;        //PWM
const int maxNumberDevices = 7;

//                             Wire1 Wire2 Wire1 Rotation XY  Focus  Dimmer
//                               cm    cm    cm     degrees, width, luminosity
volatile int data[] =      { 0, 300,  300,  300,    0,   90,   10,    40 };
volatile int lastData[] =  { 0, 300,  300,  300,    0,   90,   10,    40 };
volatile int minLength[] = { 0,  40,   40,   40,    0,    0,    0,    0  };
volatile int maxLength[] = { 0, 450,  450,  450,  359,  359,  100,  255  };

//---------------------------------
// Counters and routine variables
//---------------------------------
String message;
int i, device;

volatile byte msg;

bool newDataHasArrived;
bool jobsToBeDone[] = { false, false, false, false, false, false, false };
bool angleIsKnown;
bool focusIsKnown;
int unknownAngle = 0;
int unknownFocus = 0;
int slowDimmer = 1;     // To change light intensity slowly.
                        /* If > 1, insert a tolerance code because otherwise
                           it will never stop adjusting!
                         */
bool reedSensorReading;
bool sliderEndReading;

volatile float nowMillis, thenMillis;

void setup() {
  Serial.begin(9600);           //Communication with ESP8266-01
  sputnikSerial.begin(2400);    //Communication with Attiny85s

  pinMode(reedSensorPin, INPUT);
  pinMode(sliderEndPin, OUTPUT);
  pinMode(dimmerPin, OUTPUT);

  YStepper.setSpeed(MINI_SPEED);
  focusStepper.setSpeed(SLIDER_SPEED);
}

void loop()
{
  nowMillis=millis();
  
  //every 300ms
  if (nowMillis - thenMillis > rythm)
  {
    //Update memory when new request arrives, and send
    //1:100-2:180-3:300-4:360-
    while (newDataHasArrived)
    {
      // Prepare and send message for Attinies
      message = "";
      for (i = 1; i<=4; i++)
      {
        lastData[i] = data[i];
        message = message + String(i)+":"+String(data[i])+"-";
      }
      sendMessage(message);

      // Respond to ATmega328P's tasks: Y rotation, focus, dimmer
      jobsToBeDone[5] = true;
      jobsToBeDone[6] = true;
      jobsToBeDone[7] = true;
      // Data arrived is now done
      newDataHasArrived = false;
    }    
    
    thenMillis = millis();    
  } // end of every 300ms loop

  for (i = 5; i<=7; i++)
  {
    if (jobsToBeDone[i])
    {
      if (lastData[i] == data[i])   // Check every job
      {
        jobsToBeDone[i] = false;
      }
      else                          // Change!
      {
        switch (i)
        {
          case 5: // RotateY
            if (angleIsKnown == false)
            {
              findYzero();
            }
            if (data[i] < lastData[i])   // Rotate clockwise
            {
              YStepper.step(stepAngle);
              lastData[i] = lastData[i] - minAngle;
            }
            else                         // Rotate counterclockwise
            {
              YStepper.step(-stepAngle);
              lastData[i] = lastData[i] + minAngle;
            }
            delay(10);
            break;
          case 6: // Focus amplitude
            if (focusIsKnown == false)
            {
              findFocusZero();
            }
            if (data[i] < lastData[i])   // Move +
            {
              focusStepper.step(stepFocus);
              lastData[i] = lastData[i] - stepFocus;
            }
            else                         // Move -
            {
              focusStepper.step(-stepFocus);
              lastData[i] = lastData[i] + stepFocus;
            }
            delay(10);
            break;
          case 7: // Dimmer
            if (data[i] < lastData[i])   // More
            {
              data[i] = data[i] + slowDimmer;
            }
            else                         // Less
            {
              data[i] = data[i] - slowDimmer;
            }
            analogWrite(dimmerPin, data[i]);
            delay(10);
            break;
         }
       }
    }
  }
}

/*
  SerialEvent default function is checking every loop() cycle
  if new data comes in Rx
 */
void serialEvent()
{
  while (Serial.available())
  {
    // command ex. 1:190-2:180-3:180-4:250-
    newDataHasArrived = true;
    // from https://arduino.stackexchange.com/questions/1013/how-do-i-split-an-incoming-string
    char input[INPUT_SIZE + 1];
    byte size = Serial.readBytes(input, INPUT_SIZE);
    input[size] = 0;
    char* command = strtok(input, "-");
  
    while (command != 0)
    {
      char* separator = strchr(command, ':');
      if (separator != 0)
      {
        *separator = 0;
        device = atoi(command);
        ++separator;
        data[device] = atoi(separator);
      }
      command = strtok(0, "-");
    }
  }
}

// Function to send message to Attinies
void sendMessage(String msg)
{
  sputnikSerial.println(msg);
}

// Rotate clockwise until reed sensor reveals zero position
void findYzero()
{
  while (angleIsKnown == false)
  {
    reedSensorReading = digitalRead(reedSensorPin);
    if (reedSensorReading == true)
    {
      angleIsKnown = true;
      lastData[5] = unknownAngle;
    }
    else
    {
      unknownAngle++;
      YStepper.step(stepAngle);
      delay(10);
    }
  }
}

void findFocusZero()
{
  while (focusIsKnown == false)
  {
    sliderEndReading = digitalRead(sliderEndPin);
    if (sliderEndReading == true)
    {
      focusIsKnown = true;
      lastData[6] = unknownFocus;
    }
    else
    {
      unknownFocus++;
      focusStepper.step(stepFocus);
      delay(10);
    }
  }
}
