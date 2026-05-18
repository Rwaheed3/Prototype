#include <Servo.h>
Servo myservo;

int Echo = A4;  
int Trig = A5; 

#define ENA 5
#define ENB 6
#define IN1 7
#define IN2 8
#define IN3 9
#define IN4 11

#define carSpeed 255          // POWER 
#define triggerDistance 40    // How close to a wall before it hits the brakes (cm)
#define reverseTime 1200      // How long it backs up when completely trapped
#define turnTime 1000         // turning wider!
// ==========================================
// ==========================================

int rightDistance = 0, leftDistance = 0, middleDistance = 0;

bool engineStarted = false; 
bool hacked = false; 
bool sensorBypass = false; 

unsigned long lastSerialUpdate = 0;
const int serialInterval = 100; 

void setup() { 
  myservo.attach(3,700,2400);
  Serial.begin(9600);     
  pinMode(Echo, INPUT);    
  pinMode(Trig, OUTPUT);  
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  stop();
} 

void loop() { 
    // --- PI COMMAND HANDLING ---
    if (Serial.available() > 0) {
      char cmd = Serial.read();
      
      if (cmd == 'G') {
        engineStarted = true;
        hacked = false;
        sensorBypass = false; 
        Serial.println("ENGINE STARTED - SENSORS ON");
      }

      if (cmd == 'B') {      
        engineStarted = true;
        sensorBypass = true;  
        Serial.println("SENSOR BYPASS ACTIVE");
      }
      
      if (cmd == 'S' && !hacked) {
        hacked = true; 
        engineStarted = false;
        sensorBypass = false;
        gradualStop();
      }
    }

    // --- DISTANCE SENSING & TRANSMISSION ---
    middleDistance = Distance_test();

    if (millis() - lastSerialUpdate > serialInterval) {
      Serial.println(middleDistance); 
      lastSerialUpdate = millis();
    }

    if (!engineStarted || hacked) {
      stop();
      return; 
    }

    // --- AUTONOMOUS DRIVING LOGIC ---
    myservo.write(90);  
    
    if(sensorBypass) {
        forward(); 
    } 
    else {
        // If there is an object within our trigger distance...
        if(middleDistance <= triggerDistance && middleDistance > 0) {      
          stop(500);                                  
          myservo.write(10);          
          delay(1000);      
          rightDistance = Distance_test();
          delay(500);
          myservo.write(90);              
          delay(1000);                                                                                          
          myservo.write(180);              
          delay(1000); 
          leftDistance = Distance_test();
          delay(500);
          myservo.write(90);              
          delay(1000);
          
          // ESCAPE LOGIC
          if((rightDistance <= triggerDistance) && (leftDistance <= triggerDistance)) {
            // 1. DEAD END (Both sides blocked)
            back(carSpeed, reverseTime); 
            stop(200);            
            left(carSpeed, turnTime); 
            stop(200);
          }
          //  THE FIX: Using ">=" means if both sides are 999 (open), it will default to turning right!
          else if(rightDistance >= leftDistance) {
            // 2. RIGHT IS CLEAR
            back(carSpeed, 600); // Momentum bump to un-stick the tires
            stop(200);
            right(carSpeed, turnTime);
            stop(200);
          }
          else {
            // 3. LEFT IS CLEAR
            back(carSpeed, 600); // Momentum bump to un-stick the tires
            stop(200);
            left(carSpeed, turnTime);
            stop(200);
          }
        }  
        else {
            // Path is clear! Keep driving.
            forward();
        } 
    }               
}

// ==========================================
// MOTOR CONTROL FUNCTIONS
// ==========================================

void gradualStop() {
  for (int i = carSpeed; i >= 0; i--) {
    analogWrite(ENA, i);
    analogWrite(ENB, i);
    delay(20); 
  }
  stop();
}

void forward(){ 
  analogWrite(ENA, carSpeed);
  analogWrite(ENB, carSpeed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void back(int speed, int delayTime) {
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  delay(delayTime);
}

void left(int speed, int delayTime) {
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH); 
  delay(delayTime);
}

void right(int speed, int delayTime) {
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  delay(delayTime);
}

void stop() {
  digitalWrite(ENA, LOW);
  digitalWrite(ENB, LOW);
} 

void stop(int delayTime) {
  digitalWrite(ENA, LOW);
  digitalWrite(ENB, LOW);
  delay(delayTime);
} 

// ==========================================
// SENSOR FUNCTION
// ==========================================
int Distance_test() {
  digitalWrite(Trig, LOW);   
  delayMicroseconds(2);
  digitalWrite(Trig, HIGH);  
  delayMicroseconds(20);
  digitalWrite(Trig, LOW);   
  float time = pulseIn(Echo, HIGH, 30000); 
  float Fdistance = time / 58;
  int dist = (int)Fdistance;
  
  // If the path is perfectly clear, pulseIn times out and returns 0. 
  // We change it to 999 so the car logic knows it is wide open!
  if (dist == 0) {
    return 999;
  }
  
  return dist;
}
