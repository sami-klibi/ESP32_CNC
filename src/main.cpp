#include <Arduino.h>
#include <ESP32Servo.h>
#include "BasicStepperDriver.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

WiFiManager wifiManager;
AsyncWebServer server(80);
DNSServer dns;

#define API_KEY "AIzaSyCfk4dGfTuJ3hWHwOIgvfYvV4Uwjn9Kz7I"
#define DATABASE_URL "https://djin-78525-default-rtdb.firebaseio.com"


FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;


#define step_pin_X 26
#define dir_pin_X 16
#define home_switch_stepperX 13

#define step_pin_Y 25
#define dir_pin_Y 27
#define home_switch_stepperY 5 

#define SLP 12
#define MOTOR_STEPS 200
#define Home_X 50
#define Home_Y 50
#define RPM 400
#define MICROSTEPS 32                       // or 16

#define Servo_Z_Pin 17     // pin Z STEP
#define home_switch_Z 23 
#define Servo_G_Pin 14     // pin Z DIR

#define Max_X 4300   //76 cm
#define Max_Y 7800   //173 cm

#define GRIPPER_IN_ANGLE 10                         // Define the gripper closed angle
#define GRIPPER_OUT_ANGLE 175    

long CurrentPosition_X = Home_X ;
long CurrentPosition_Y = Home_Y;

// servo pin for open/close gripper

void Home();
void GoTo_X();
void GoTo_Y();
void GoTo_Z();
void GoTo_XYZ();
void Bring();
void z_in();
void z_out();
void processData(String receivedData);

BasicStepperDriver stepper_X(MOTOR_STEPS, dir_pin_X, step_pin_X);
BasicStepperDriver stepper_Y(MOTOR_STEPS, dir_pin_Y, step_pin_Y);

Servo Servo_Z ;
Servo Servo_Gripper ;


String Data;
String previousData;

const char delimiter = 'g';
const char delimiter_bring = 'b';
int speed = 60;
int homing_speed = 40;

void setup() 
{
  Serial.begin(115200);
  
  stepper_X.begin(RPM, MICROSTEPS);
  stepper_Y.begin(RPM, MICROSTEPS);
  
  pinMode (dir_pin_X, OUTPUT);
  pinMode (step_pin_X, OUTPUT);
  pinMode (dir_pin_Y, OUTPUT);
  pinMode (step_pin_Y, OUTPUT);
  pinMode (home_switch_stepperX, INPUT_PULLUP);
  pinMode (home_switch_stepperY, INPUT_PULLUP);
  pinMode (home_switch_Z, INPUT_PULLUP);
  pinMode (SLP, OUTPUT);
  digitalWrite (SLP, LOW);  

  Servo_Z.attach(Servo_Z_Pin);
  Servo_Gripper.attach(Servo_G_Pin);

  Data ='h';
  
  
  // Initialize Firebase configuration
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Setup WiFiManager
  WiFiManager wifiManager;
  wifiManager.setConnectTimeout(15);
  if (!wifiManager.autoConnect("Gripper-Setup")) { // "ESP32-Setup" is the AP name if no saved credentials are found
      Serial.println("Failed to connect to WiFi and hit timeout. Restarting...");
      ESP.restart();
  }

  // Firebase setup after connecting to Wi-Fi
   if (Firebase.signUp(&config, &auth, "", "")) {
      Serial.println("Connected to Firebase");
      signupOK = true;
  } else {
      Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Starting:");
  Firebase.RTDB.setString(&fbdo, "/Gripper", "homing");

  Serial.println("Starting : ");
  Serial.println("example 'gX,Y,Z,Speed' : g1000,3000,100,200"); 
}
    

void loop(){
  static unsigned long lastCheck = 0;
  unsigned long now = millis();

  if (now - lastCheck > 2000) { // Check Firebase every 2 seconds
    lastCheck = now;
  

    if (Firebase.RTDB.getString(&fbdo, "/Gripper")) {
      Data = fbdo.stringData();
      Data.trim();
      if (Data != previousData) {
        Serial.println("New data received: ");
        Serial.println(Data);
        previousData = Data;
        processData(Data);
      }
    } else {
      Serial.println(fbdo.errorReason());
    }
  }
  if (Serial.available()) {
    Data = Serial.readString();
    Data.trim();
    Serial.print("USB Serial Message: ");
    Serial.println(Data);
    processData(Data);
  }
}
 

void processData(String receivedData) {
    receivedData.trim(); // Remove leading/trailing whitespace
  
    Firebase.RTDB.setString(&fbdo, "/Gripper", "received");
  
    Serial.print("Data to process: ");
    Serial.println(receivedData);
  //Serial.println(digitalRead(home_switch_Z));
  switch (Data[0]) {
    case 'h': Home(); break;
    case 'x': GoTo_X(); break;
    case 'y': GoTo_Y(); break;
    case 'z': GoTo_Z(); break; 
    case 'i': z_in(); break; 
    case 'o': z_out(); break;      
    case 'g': GoTo_XYZ(); break; 
    case 'b': Bring(); break;
      
    default:
      // Handle invalid command or data here
      break;
  }
}

void Home()
{
  if (Data[0]=='h')
  {
  Serial.println("Homing...");
  stepper_Y.setRPM(homing_speed);
  stepper_X.setRPM(homing_speed);
  Servo_Z.write(0);
  Servo_Gripper.write(40);
  Serial.println("Z home");
  
  while (digitalRead(home_switch_stepperX) == HIGH){
    stepper_X.rotate(-20) ;
  }
   
   while (digitalRead(home_switch_stepperY) == HIGH){
    stepper_Y.rotate(-20);
    }
    delay (100);
    
    stepper_Y.setRPM(10);
    stepper_X.setRPM(10);
    stepper_X.rotate(Home_X);
    stepper_Y.rotate(Home_Y);
     

  CurrentPosition_X = Home_X ;
  CurrentPosition_Y = Home_Y;

    Serial.print("the updated current position X: ");
    Serial.println(CurrentPosition_X);
    Serial.print("the updated current position Y: ");
    Serial.println(CurrentPosition_Y);    
    Serial.print("the updated current position Z: ");
    Serial.println(0);
    Serial.println("Homing Done");
  Data = "";
  }
  
}

void Bring() {
  int x = 0, y = 0, close_angle = 0; // Parsed from input
  int dela = 500; // Base delay duration (adjust if needed)

  // Check if the command starts with 'b'
  if (Data.length() > 1 && Data[0] == 'b') {
      String parameters = Data.substring(1); // Get parameters: "x,y,close_angle"

      // Find the commas
      int comma1 = parameters.indexOf(',');
      int comma2 = parameters.indexOf(',', comma1 + 1); // Find the second comma

      // Check if both commas were found (meaning 3 parameters exist)
      if (comma1 != -1 && comma2 != -1) {
          // Parse the values
          x = parameters.substring(0, comma1).toInt();
          y = parameters.substring(comma1 + 1, comma2).toInt();
          close_angle = parameters.substring(comma2 + 1).toInt();

          // --- Start New Bring Sequence ---
          Serial.println("Starting Custom Bring sequence...");
          Serial.print("Target X:"); Serial.print(x);
          Serial.print(", Y:"); Serial.print(y);
          Serial.print(", Close Angle:"); Serial.println(close_angle);

          // 1. Go to X, Y position
          // (Using the current global 'speed')
          Serial.println("Step 1: Moving to X, Y...");
          Data = String('x') + String(x); // Prepare Data for GoTo_X
          GoTo_X();
          Data = String('y') + String(y); // Prepare Data for GoTo_Y
          GoTo_Y();
          delay(dela); // Wait after reaching position

          // 2. Open gripper fully (fixed angle 140)
          Serial.println("Step 2: Opening Gripper...");
          Data = String('z') + String(140); // Use 'z' command for GoTo_Z
          GoTo_Z(); // Call GoTo_Z to control the gripper servo
          delay(dela * 2); // Allow time for gripper servo

          // 3. Move Z-axis "out" (Lower Z-axis)
          Serial.println("Step 3: Lowering Z-Axis...");
          Data = "o"; // Prepare Data for z_out()
          z_out();    // Call function to move Servo_Z
          delay(dela * 3); // Allow time for Z movement

          // 4. Close gripper to the specified angle
          Serial.println("Step 4: Closing Gripper...");
          Data = String('z') + String(close_angle); // Prepare Data for GoTo_Z
          GoTo_Z(); // Call GoTo_Z to control the gripper servo
          delay(dela * 2); // Allow time for gripper servo

          // 5. Move Y-axis "down" 10 steps (Increase Y coordinate)
          Serial.println("Step 5: Moving Y axis down (+10)...");
          // IMPORTANT: Use CurrentPosition_Y which is updated inside GoTo_Y
          int new_y = CurrentPosition_Y + 10;
          // Optional: Add check to ensure new_y doesn't exceed Max_Y
          if (new_y > Max_Y) {
              new_y = Max_Y;
              Serial.println("Warning: Y+10 exceeds Max_Y. Moving to Max_Y instead.");
          }
          Data = String('y') + String(new_y); // Prepare Data for GoTo_Y
          GoTo_Y();
          delay(dela); // Wait after Y adjustment

          // 6. Move Z-axis "in" (Raise Z-axis)
          Serial.println("Step 6: Raising Z-Axis...");
          Data = "i"; // Prepare Data for z_in()
          z_in();     // Call function to move Servo_Z
          delay(dela * 3); // Allow time for Z movement

          // --- End New Bring Sequence ---

      } else {
          Serial.println("Invalid format for 'b' command. Use: bx,y,close_angle");
      }

      Serial.println("Bring Sequence Done");
      Data = ""; // Clear global Data variable after finishing

  } else if (Data[0] == 'b') {
       Serial.println("Invalid format for 'b' command. Use: bx,y,close_angle");
       Data = ""; // Clear Data if format is wrong
  }
}



void GoTo_XYZ(){
  
  int x = 0, y = 0, z = 0, Speed = 0;
  
  // Assuming data is in the format "gx,y,z,speed"
  if (Data[0]=='g') {
    Data.remove(0, 1);  
    
    
    int comma1 = Data.indexOf(",");
    int comma2 = Data.indexOf(",", comma1 + 1);
    int comma3 = Data.indexOf(",", comma2 + 1);
    
    if (comma3 != -1) {  
      x = Data.substring(0, comma1).toInt();
      y = Data.substring(comma1 + 1, comma2).toInt();
      z = Data.substring(comma2 + 1, comma3).toInt();
      Speed = Data.substring(comma3 + 1).toInt();
      Speed = constrain(Speed, 0, 200);
      speed = Speed;

      char myChar = 'x';
      Data =String(myChar) + String(x);
      GoTo_X();

      myChar = 'y';
      Data =String(myChar) + String(y);
      GoTo_Y();

      myChar = 'z';
      Data =String(myChar) + String(z);
      GoTo_Z();
    }
    Serial.println("GoTo_XYZ Done");
    Data = ""; 
  }
}


void GoTo_Z()
 {
   if (Data[0]=='z')
   {
    
    int z = Data.substring(1).toInt();
    if (z >= 20 && z <= 150) {  
    Servo_Gripper.write(z);
    delay(100);
    Serial.print("the updated current position Z: ");
    Serial.println(z);
    }
     Data = ""; 
   }
 }

 void z_in() {
  if (Data == "i"){
  Servo_Z.write(GRIPPER_IN_ANGLE);
  Serial.println("done_in");
  }
  Data = "";
}

void z_out() {
  if (Data == "o"){
  Servo_Z.write(GRIPPER_OUT_ANGLE);
  Serial.println("done_out");
  }
  Data = "";
}

 void GoTo_X()
 {
   if (Data[0]=='x')
   {
    int x = Data.substring(1).toInt();
    if (x >= Home_X && x <= Max_X) {  
    Serial.print("x = ");
    Serial.println(x);

    int x_steps = x - CurrentPosition_X ;
    Serial.print("steps to add = ");
    Serial.println(x_steps);

    stepper_X.setRPM(speed);
    stepper_X.rotate(x_steps);
    CurrentPosition_X = x;
    Serial.print("the updated current position X: ");
    Serial.println(CurrentPosition_X);
    Data = "";
   } 
  }
 }

 void GoTo_Y()
 {
   if (Data[0]=='y')
   {
    int y = Data.substring(1).toInt();
    if (y >= Home_Y && y <= Max_Y) {  
    Serial.print("y = ");
    Serial.println(y);

    int y_steps = y - CurrentPosition_Y ;
    Serial.print("steps to add = ");
    Serial.println(y_steps);

    stepper_Y.setRPM(speed);
    stepper_Y.rotate(y_steps);
    CurrentPosition_Y = y;
    Serial.print("the updated current position Y: ");
    Serial.println(CurrentPosition_Y);
    Data = "";
   } 
  }
 } 
