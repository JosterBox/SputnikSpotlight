/* 23-2-2018

 Sputnik Spotlight - ESP8266-01

Interactions:

1) Alexa device by voice commands:
  "Alexa, ask Sputnik to turn on full light"
  "Alexa set Sputnik to light diffuse"
  "Alexa, ask Sputnik number two"
  "Alexa, ask Sputnik to set lights  on the table"


2) Web page with simple inputs to control Lamp on 192.168.1.116
 Sends commands to ATmega328P via UART in format:
1:80-2:40-3:180-4:200-

- length1     1st big stepper position, cm
- length2     2nd big stepper position, cm
- length3     3rd big stepper position, cm
- rotationX   4th Attiny, x position (0-359), degrees
- rotationY   Atmega,  y position (0-359), degrees
- focus       focus microstepper (0-100)
- dimmer      dimmer (0-255)  , PWM pin to lamp ground


3) Terminal bash command to change position:

curl -s 'http://192.168.1.116/saved_position_1 >> /dev/null
curl -s --max-time 30 --retry 20 --retry-delay 10 --retry-max-time 60 'http://192.168.1.116/change?length1=300&length2=300&length3=0&rotationX=90&rotationY=10&focus=10&dimmer=100' >> /dev/null || echo "Error Connection!"



TO DO:
read_sensors() function
EEPROM saved settings and saved position,
save actual position function


*/

//---------------------------------
// Libraries
//---------------------------------

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h> 
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h>
#include <Hash.h>

//---------------------------------
// Settings
//---------------------------------

int delayRepeat = 100;    // Time (ms) between repeated commands
int refreshRate = 1000;   // Loop speed: how fast server client update

const int numberOfDevices = 7;
//                             Wire1 Wire2 Wire1 Rotation XY  Focus  Dimmer
//                               cm    cm    cm     degrees, width, luminosity
volatile int data[] =      { 0, 300,  300,  300,    0,   90,   10,    40 };
volatile int minLength[] = { 0,  40,   40,   40,    0,    0,    0,    0 };
volatile int maxLength[] = { 0, 450,  450,  450,  359,  359,  100,  255 };


//---------------------------------
// Server settings
//---------------------------------

// INSERT YOUR OWN ROUTER SSID and PASSWORD HERE!

const char* ssid = "";
const char* password = "";

// Usual settings, change for specific router (e.g. 192.168.0.x)
IPAddress ip(192,168,1,116);    // Request of static IP: 192.168.1.116
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

ESP8266WebServer server(80);    // Setup of a webserver, port 80 listening

//-------------------------------------------------
// Heroku settings
char host[] = "sputnik-controller.herokuapp.com";
int port = 80;
char path[] = "/ws"; 
ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

//---------------------------------
// Counters and routine variables
//---------------------------------

int device, value;
int lastData[numberOfDevices];
bool WiFiIsGood, WiFiWasGood;
String webhtml, answer, endAnswer, message;
DynamicJsonBuffer jsonBuffer; 
String currState;
int pingCount = 0;
String triggerName ="";
String triggerVal ="";
int triggerEnabled = 0;

unsigned long nowMillis, thenMillis;



//---------------------------------
// Setup
//---------------------------------
void setup()
{
  Serial.begin(9600);           // This establish UART communication with ATmega328P

  WiFi.begin(ssid, password);   // This establish WiFi communication with router
  WiFi.config(ip, gateway, subnet);

  WiFiMulti.addAP(ssid, password);
  delay(700);
  
  webSocket.begin(host, port, path);  // This establish Wifi communication with Heroku
  webSocket.onEvent(webSocketEvent);


// These are the functions that will be called
// as requests to this webserver

// Open a browser and look for: 192.168.1.116
  
  server.on("/", webPage);
  server.on("/change", manual_change);
  server.on("/read", read_sensors);
  server.on("/save", save_position);
  server.on("/stop", manual_stop);
  server.on("/saved_position_1", saved_position_1);
  server.on("/saved_position_2", saved_position_2);
  server.on("/saved_position_3", saved_position_3);
  server.on("/saved_position_4", saved_position_4);

  server.begin();

}

//---------------------------------
// Loop
//---------------------------------
void loop(void)
{
  checkWiFi();                 // Checks connection and, if changed, tells ATmega
  server.handleClient();       // Manage the webserver
  delay(refreshRate);          // Refresh rate (every second)
}

//---------------------------------
// Web page
//---------------------------------
void webPage()
{

  /*
   This is a simple web page in html code! Code needs it as
   a String variable I'm calling "webhtml", and sends it by command
      server.send(200, "text/html", webhtml);
   Carriage return \n\ is used for easy reading; blank lines won't work.
   &nbsp; is an extra space, because html won't read multiple spaces.
   Arduino IDE won't help in formatting or highlighting commands!
   For better html pages use an external editor and embed it here.
   Note how to insert a variable:   " + String(variable)+ "
   CSS style is embedded.
   Other "pages" are called to perform actions like change position.
  */
  
  webhtml = "                                          \n\
             <!DOCTYPE html>                           \n\
            <html>                                     \n\
              <head>                                   \n\
                <title>Sputnik Lamp Control</title>    \n\
              </head>                                  \n\
                                                       \n\
              <style>                                  \n\
                body {  margin:0; padding:0; border:0; width:100%; font-family: 'Verdana', Verdana, serif; font-size: 0.75em; color: #111111; }    \n\
                h1   {  display: block; text-align: center;  margin-top: 0.3em; margin-bottom: 0.3em; margin-left: 0; margin-right: 0;  padding-left: 50pt; padding-right: 50pt;  font-family: 'Verdana', Verdana, serif; font-size: 1.25em; color: #1111CC; }    \n\
                p    {  display: block; width: 65%;  margin-top: 0.3em; margin-bottom: 0.3em; margin-right: 0; margin-left: 30%;  padding-left: 10pt; padding-right: 10pt;  }    \n\
                input[type=text]   {  text-align: right; font-size: 1em; color: #555; margin-left: 10pt;  }    \n\
                input[type=submit] {  position: relative; left: 10%; text-align: right; margin-left: 5%;  font-size: 1em; color: #777;  }      \n\
              </style>                                 \n\
                                                       \n\              
              <body>                                   \n\
                <h1>  Sputnik Lamp web-control  </h1>  \n\
                <p> Data transmitted from router to devices by UART \n\
                <hr>                                   \n\
                <form action='/change'>                \n\
                  <p>Length 1 &nbsp;&nbsp;= <input type='text' size='1' name='length1' value='"+String(data[1])+"'> (cm)    \n\
                  <p>Length 2 &nbsp;&nbsp;= <input type='text' size='1' name='length2' value='"+String(data[2])+"'> (cm)   \n\
                  <p>Length 3 &nbsp;&nbsp;= <input type='text' size='1' name='length3' value='"+String(data[3])+"'> (cm)    \n\
                  <p>Rotation X = <input type='text' size='1' name='rotationX' value='"+String(data[4])+"'> (degrees)    \n\
                  <p>Rotation Y = <input type='text' size='1' name='rotationY' value='"+String(data[5])+"'> (degrees)    \n\
                  <p>Focus &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;= <input type='text' size='1' name='focus' value='"+String(data[6])+"'> (0-100%)    \n\
                  <p>Dimmer &nbsp;&nbsp;&nbsp;= <input type='text' size='1' name='dimmer' value='"+String(data[7])+"'> (0-255)    \n\
                  <input type='submit' formaction='/change' value='Change'>     \n\
                </form>                                 \n\
                <hr>                                    \n\
                <form>                                  \n\
                Commands (work in progress):            \n\
                  <input type='submit' formaction='/read' value='Read'>         \n\
                  <input type='submit' formaction='/save' value='Save'>         \n\
                  <input type='submit' formaction='/stop' value='STOP'>         \n\
                </form>                                \n\
              <hr>                                     \n\
              <form>                                   \n\
                  Saved positions:                     \n\
                  <input type='submit' formaction='/saved_position_1' value='Full'>    \n\
                  <input type='submit' formaction='/saved_position_2' value='Diffuse'>    \n\
                  <input type='submit' formaction='/saved_position_3' value='Angle'>    \n\
                  <input type='submit' formaction='/saved_position_4' value='On the table'>    \n\
              </form>                                  \n\
              <hr>                                     \n\
              </body>                                  \n\
            </html>                                    \n\
    ";
    
    //Note this "; that ends webhtml String. Every " inside it is an inserted variable.

  server.send(200, "text/html", webhtml);  //Send html code!
}

//---------------------------------
// Request: manual change
//---------------------------------
void manual_change()
{
  // Transport data from server (html) to array (C)
  data[1] = server.arg("length1").toInt();
  data[2] = server.arg("length2").toInt();
  data[3] = server.arg("length3").toInt();
  data[4] = server.arg("rotationX").toInt();
  data[5] = server.arg("rotationY").toInt();
  data[6] = server.arg("focus").toInt();
  data[7] = server.arg("dimmer").toInt();
  
  checkSendData(); // Send commands to ATmega328P
  webPage();  // Go back into web front page
}

//---------------------------------
// Request: emergency stop...
//---------------------------------

void manual_stop()
{
  message = "*:STOP";
  Serial.println(message);
  delay(delayRepeat);
  Serial.println(message);
  
  // Should now ask for actual position, by sensors reading

  webPage();
}

//---------------------------------
// Request: read sensors
//---------------------------------
void read_sensors()
{
  message = "*:READ";
  Serial.println(message);
  delay(delayRepeat);
  Serial.println(message);
  
  //Ask for position
  webPage();
}

//---------------------------------
// Request: save position
//---------------------------------
void save_position()
{
  //save position in EEPROM
  
  webPage();
}



//---------------------------------
// Request: fixed position change
//---------------------------------
void saved_position_1()    //Full
{
  data[1] = 240;
  data[2] = 180;
  data[3] = 120;
  data[4] = 0;
  data[5] = 270;
  data[6] = 100;
  data[7] = 254;
  
  checkSendData();  
  webPage();  // Go back into web front page
}

void saved_position_2()    //Diffuse
{
  data[1] = 240;
  data[2] = 180;
  data[3] = 120;
  data[4] = 0;
  data[5] = 90;
  data[6] = 20;
  data[7] = 150;
  
  checkSendData();  
  webPage();
}

void saved_position_3()   //Angle
{
  data[1] = 100;
  data[2] = 350;
  data[3] = 200;
  data[4] = 135;
  data[5] = 45;
  data[6] = 80;
  data[7] = 200;
    
  checkSendData();
  webPage();
}

void saved_position_4()    //On the table
{
  data[1] = 200;
  data[2] = 200;
  data[3] = 200;
  data[4] = 270;
  data[5] = 90;
  data[6] = 50;
  data[7] = 254;

  checkSendData();
  webPage();
}


//--------------------------------
// Tools
//--------------------------------






//---------------------------------------
// Web communication
//---------------------------------------
void checkWiFi()
{
  if ( WiFi.status() == WL_CONNECTED )  {    WiFiIsGood = true;  }
  else                                  {    WiFiIsGood = false; }
  if ( WiFiIsGood != WiFiWasGood)
  {
  answer = String((WiFiIsGood)?"ON":"Off");
  Serial.println("Wifi is "+ answer);
  WiFiWasGood = WiFiIsGood;
  }
}


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length)
{
  // Thanks to: Ruchir Sharma
  // https://www.hackster.io/ruchir1674/voice-controlled-switch-using-arduino-and-alexa-0669a5
  switch(type) {
    case WStype_DISCONNECTED:
      webSocket.begin(host, port, path);
      webSocket.onEvent(webSocketEvent);
      break;
    case WStype_CONNECTED:
      // Tell Heroku server we're connected
      webSocket.sendTXT("Connected");
      break;
    case WStype_TEXT:
      // Receive data from Heroku
      processWebSocketRequest((char*)payload);
      break;
    case WStype_BIN:
      hexdump(payload, length);
      // Send data to Heroku
      webSocket.sendBIN(payload, length);
      break;
  }
}

void processWebSocketRequest(String data)
{
  // Again, thanks to: Ruchir Sharma
  // https://www.hackster.io/ruchir1674/voice-controlled-switch-using-arduino-and-alexa-0669a5

   String jsonResponse = "{\"version\": \"1.0\",\"sessionAttributes\": {},\"response\": {\"outputSpeech\": {\"type\": \"PlainText\",\"text\": \"<text>\"},\"shouldEndSession\": true}}";
   JsonObject& root = jsonBuffer.parseObject(data);
   String query = root["query"];
   String message="";
   Serial.println(data);
            
   if(query == "light")
   { //if query check state
     String value = root["value"];  
     Serial.println("Received command!");
     if(value=="full")
     {
       message = "{\"full\":\"light\"}";
       saved_position_1();
     }
     else if (value=="diffuse")
     {
       message = "{\"diffuse\":\"light\"}";
       saved_position_2();
     }
     else if (value=="angle")
     {
       message = "{\"angle\":\"light\"}";
       saved_position_3();
     }
     else if (value=="table")
     {
       message = "{\"table\":\"light\"}";
       saved_position_4();
     }
     else
     {
       String object = root["object"];
     }
     jsonResponse.replace("<text>", "It is done");

   }else if(query == "help")
   {
     message = "Sputnik spotlight is a remote controlled lamp. Can change position, rotate and dimmer.";
   }
   else
   {//can not recognized the command
     Serial.println("Command is not recognized!");
   }
   // send message to server
   webSocket.sendTXT(jsonResponse);
   if(query == "cmd" || query == "?")
   {
     webSocket.sendTXT(jsonResponse);
   }
}



//---------------------------------------
// Lamp communication
//---------------------------------------
void checkSendData()
{
  // This check if the request are inside default range
  for (int i = 1; i<numberOfDevices; i++)
  {
    if (data[i] < minLength[i]) { data[i] = minLength[i]; }
    if (data[i] > maxLength[i]) { data[i] = maxLength[i]; }
  }
  
  // Here should be a better check,
  // e.g. controlling if wire1 + wire2 perform an impossible position.
  
  message = "";
  message = message + "1:"+ data[1]+"-";
  message = message + "2:"+ data[2]+"-";
  message = message + "3:"+ data[3]+"-";
  message = message + "4:"+ data[4]+"-";
  message = message + "5:"+ data[5]+"-";
  message = message + "6:"+ data[6]+"-";
  message = message + "7:"+ data[7]+"-";
  Serial.println(message);
  delay(delayRepeat);
  Serial.println(message);
}

