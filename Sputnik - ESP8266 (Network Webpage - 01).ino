/* 15-2-2018

 ESP8266-01

 Web page with simple inputs to control Lamp and send to ATmega328P via UART messages like:
1:80-2:40-3:180-4:200-

meaning: device 1, value 80
         device 2, value 40
         ecc.

same command type, only in range 1-4, parsed by each device to get its own specific command
Commands 5,6,7 are for Atmega328P

1) 90     1st big stepper position, cm
2) 80     2nd big stepper position, cm
3) 180    3rd big stepper position, cm
4) 0      4th Attiny, x position (0-359), degrees
5) 0      Atmega,  y position (0-359), degrees
6) 7      focus microstepper (0-100)
7) 0      dimmer (0-255)  , PWM pin to lamp ground


ESP8266-01 ------ Webpage on 192.168.1.116
ESP8266-01  ----  Atmega328P
connected via UART standard serial
rx-tx
tx-rx

Terminal bash commmand to change position:
curl -s --max-time 30 --retry 20 --retry-delay 10 --retry-max-time 60 'http://192.168.1.116/change?data1=300&data2=300&data3=0&data4=90&data5=10&data6=10&data7=100' >> /dev/null || echo "Error Connection!"
 


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

const char* ssid     = "-------------------------------";
const char* password = "-------------------------------";


// Usual settings, change for specific router (e.g. 192.168.0.x)
IPAddress ip(192,168,1,116);    // Request of static IP: 192.168.1.116
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

ESP8266WebServer server(80);    // Setup of a webserver, port 80 listening


//---------------------------------
// Counters and routine variables
//---------------------------------

int device, value;
int lastData[numberOfDevices];
bool WiFiIsGood, WiFiWasGood;
String webhtml, answer, endAnswer, message;
unsigned long nowMillis, thenMillis;


//---------------------------------
// Setup
//---------------------------------
void setup()
{
  Serial.begin(9600);           // This establish UART communication with ATmega328P

  WiFi.begin(ssid, password);   // This establish WiFi communication with router
  WiFi.config(ip, gateway, subnet);

// These are the functions that will be called
// as requests to this webserver

// Open a browser and look for: 192.168.1.116
  
  server.on("/", webPage);
  server.on("/change", manual_change);
  server.on("/read", read_sensors);
  server.on("/save", save_position);
  server.on("/stop", manual_stop);
  server.on("/fixed_change_1", fixed_change_1);
  server.on("/fixed_change_2", fixed_change_2);
  server.on("/fixed_change_3", fixed_change_3);
  server.on("/fixed_change_4", fixed_change_4);

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
// Main page
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
                  <p>Length 1 &nbsp;&nbsp;= <input type='text' size='1' name='data1' value='"+String(data[1])+"'> (cm)    \n\
                  <p>Length 2 &nbsp;&nbsp;= <input type='text' size='1' name='data2' value='"+String(data[2])+"'> (cm)   \n\
                  <p>Length 3 &nbsp;&nbsp;= <input type='text' size='1' name='data3' value='"+String(data[3])+"'> (cm)    \n\
                  <p>Rotation X = <input type='text' size='1' name='data4' value='"+String(data[4])+"'> (degrees)    \n\
                  <p>Rotation Y = <input type='text' size='1' name='data5' value='"+String(data[5])+"'> (degrees)    \n\
                  <p>Focus &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;= <input type='text' size='1' name='data6' value='"+String(data[6])+"'> (0-100%)    \n\
                  <p>Dimmer &nbsp;&nbsp;&nbsp;= <input type='text' size='1' name='data7' value='"+String(data[7])+"'> (0-255)    \n\
                  <input type='submit' formaction='/change' value='Change'>     \n\
                </form>                                 \n\
                <hr>                                    \n\
                <form>                                  \n\
                  <input type='submit' formaction='/read' value='Read'>         \n\
                  <input type='submit' formaction='/save' value='Save'>         \n\
                  <input type='submit' formaction='/stop' value='STOP'>         \n\
                </form>                                \n\
              <hr>                                     \n\
              <form>                                   \n\
                  <input type='submit' formaction='/fixed_change_1' value='Saved_1'>    \n\
                  <input type='submit' formaction='/fixed_change_2' value='Saved_2'>    \n\
                  <input type='submit' formaction='/fixed_change_3' value='Saved_3'>    \n\
                  <input type='submit' formaction='/fixed_change_4' value='Saved_4'>    \n\
              </form>                                  \n\
              <hr>                                     \n\
              </body>                                  \n\
            </html>                                    \n\
    ";
    
    //Note this "; that ends webhtml String. Every " inside it is an inserted variable.

  server.send(200, "text/html", webhtml);  //Send html code!
}

//---------------------------------
// Manual change
//---------------------------------
void manual_change()
{
  // Transport data from server (html) to array (C)
  data[1] = server.arg("data1").toInt();
  data[2] = server.arg("data2").toInt();
  data[3] = server.arg("data3").toInt();
  data[4] = server.arg("data4").toInt();
  data[5] = server.arg("data5").toInt();
  data[6] = server.arg("data6").toInt();
  data[7] = server.arg("data7").toInt();
  
  checkSendData(); // Send commands to ATmega328P
  webPage();  // Go back into web front page
}

//---------------------------------
// Emergency stop...
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
// Read sensors
//---------------------------------
void read_sensors()
{
  message = "*:READ";
  Serial.println(message);
  delay(delayRepeat);
  Serial.println(message);
  
  //Wait for answer
  webPage();
}

//---------------------------------
// Save position
//---------------------------------
void save_position()
{
  //save position in EEPROM
  
  webPage();
}



//---------------------------------
// Fixed position change
//---------------------------------
void fixed_change_1()
{
// How to store position's title? EEPROM isn't good with characters

  data[1] = 250;
  data[2] = 125;
  data[3] = 180;
  data[4] = 30;
  data[5] = 60;
  data[6] = 20;
  data[7] = 254;
  
  checkSendData();  
  webPage();  // Go back into web front page
}

void fixed_change_2()
{
  data[1] = 125;
  data[2] = 320;
  data[3] = 190;
  data[4] = 0;
  data[5] = 180;
  data[6] = 100;
  data[7] = 150;
  
  checkSendData();  
  webPage();
}

void fixed_change_3()
{
  data[1] = 300;
  data[2] = 300;
  data[3] = 400;
  data[4] = 35;
  data[5] = 45;
  data[6] = 90;
  data[7] = 40;
    
  checkSendData();
  webPage();
}

void fixed_change_4()
{
  data[1] = 400;
  data[2] = 400;
  data[3] = 400;
  data[4] = 270;
  data[5] = 90;
  data[6] = 100;
  data[7] = 10;
    
  checkSendData();
  webPage();
}


//--------------------------------
// Tools
//--------------------------------
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

