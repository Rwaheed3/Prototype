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
#define carSpeed 150 
int rightDistance = 0, leftDistance = 0, middleDistance = 0;

bool engineStarted = false; 
bool hacked = false; 
bool sensorBypass = false; // 

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
        sensorBypass = false; // 
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
        forward(); // this makes car keep going 
    } 
    else {
        // Standard Safety Logic
        if(middleDistance <= 40 && middleDistance > 0) {      
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
          
          if((rightDistance <= 40) && (leftDistance <= 40)) {
            back(carSpeed, 180);
          }
          else if(rightDistance > leftDistance) {
            right(carSpeed, 360);
          }
          else if(rightDistance < leftDistance) {
            left(carSpeed, 360);
          }
          else {
            forward();
          }
        }  
        else {
            forward();
        } 
    }               
}

// ... (Rest of your functions: gradualStop, forward, back, left, right, stop, Distance_test remain exactly the same)
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

int Distance_test() {
  digitalWrite(Trig, LOW);   
  delayMicroseconds(2);
  digitalWrite(Trig, HIGH);  
  delayMicroseconds(20);
  digitalWrite(Trig, LOW);   
  float time = pulseIn(Echo, HIGH, 30000); 
  float Fdistance = time / 58;
  return (int)Fdistance;
}