#include <Servo.h>
Servo myservo;

// --- ULTRASONIC PINS ---
int Echo = A4;  
int Trig = A5; 

// --- MOTOR PINS ---
#define ENA 5
#define ENB 6
#define IN1 7
#define IN2 8
#define IN3 9
#define IN4 11

// --- LINE TRACKING PINS ---
#define LT_L 2
#define LT_M 4
#define LT_R 10

// --- BUTTON PIN ---
#define BUTTON_PIN 12

// --- SETTINGS ---
#define carSpeed 255          // POWER 
#define trackingSpeed 150     // Slower speed for line tracking accuracy
#define triggerDistance 40    // How close to a wall before it hits the brakes (cm)
#define reverseTime 1200      // How long it backs up when completely trapped
#define turnTime 1000         // turning wider!

// --- STATE VARIABLES ---
int currentMode = 0; // 0: Standby, 1: Self-Driving, 2: Autopilot, 3: Rogue Hijack
bool hacked = false; 
bool sensorBypass = false; 

// Button Debouncing
bool lastButtonState = HIGH; 
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200; 

int rightDistance = 0, leftDistance = 0, middleDistance = 0;
unsigned long lastSerialUpdate = 0;
const int serialInterval = 100; 

void setup() { 
  myservo.attach(3, 700, 2400);
  Serial.begin(9600);     
  
  pinMode(Echo, INPUT);    
  pinMode(Trig, OUTPUT);  
  
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);

  // Line Sensors
  pinMode(LT_L, INPUT);
  pinMode(LT_M, INPUT);
  pinMode(LT_R, INPUT);

  // Mode Button (Internal Pullup means no resistor needed)
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  stop();
} 

void loop() { 
    // ==========================================
    // 1. HARDWARE BUTTON HANDLING
    // ==========================================
    bool reading = digitalRead(BUTTON_PIN);
    if (reading == LOW && lastButtonState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
      if(!hacked) { // Disable physical button if system is compromised
        currentMode++; 
        if (currentMode > 2) currentMode = 0; 
        lastDebounceTime = millis();
        stop(); // Stop safely when switching
        delay(300);
      }
    }
    lastButtonState = reading;

    // ==========================================
    // 2. PI SERIAL COMMAND HANDLING
    // ==========================================
    if (Serial.available() > 0) {
      char cmd = Serial.read();
      
      if (cmd == 'G') {
        currentMode = 1; // Self-Driving
        hacked = false;
        sensorBypass = false; 
        Serial.println("MODE: SELF-DRIVING");
      }
      else if (cmd == 'A') {
        currentMode = 2; // Autopilot (Line Tracking)
        hacked = false;
        sensorBypass = false;
        Serial.println("MODE: AUTOPILOT");
      }
      else if (cmd == 'B') {      
        currentMode = 1;
        sensorBypass = true;  
        Serial.println("SENSOR BYPASS ACTIVE");
      }
      else if (cmd == 'S' && !hacked) {
        currentMode = 0; // Standby
        hacked = true; 
        sensorBypass = false;
        gradualStop();
      }
      // === TWO-PART NATIVE HARDWARE HIJACK EXECUTOR ===
      else if (cmd == 'R') {
        currentMode = 3;       // Lock state container to mode 3
        sensorBypass = true;   // Part 1: Force blindness on ultrasonic logic paths
        hacked = false;        // Override safety trap variable
        Serial.println("CRITICAL: ROGUE RIGHT HIJACK");
      }
    }

    // ==========================================
    // 3. SENSOR TRANSMISSION
    // ==========================================
    middleDistance = Distance_test();

    if (millis() - lastSerialUpdate > serialInterval) {
      Serial.println(middleDistance); 
      lastSerialUpdate = millis();
    }

    // Only force stop container logic if standard hack mode is true (ignores mode 3)
    if (hacked && currentMode != 3) {
      stop();
      return; 
    }

    // ==========================================
    // 4. EXECUTE CURRENT MODE
    // ==========================================
    switch(currentMode) {
      case 0:
        stop();
        break;
      case 1:
        selfDrivingLogic();
        break;
      case 2:
        lineTrackingLogic();
        break;
      case 3:
        // Part 2: Force hard right deflection at complete duty speed
        right(255, 0); 
        break;
    }
}

// ==========================================
// MODE LOGIC FUNCTIONS
// ==========================================
void lineTrackingLogic() {
  bool leftSee = digitalRead(LT_L);
  bool midSee = digitalRead(LT_M);
  bool rightSee = digitalRead(LT_R);

  if (midSee) {
    forward(trackingSpeed);
  } else if (rightSee) {
    right(trackingSpeed, 0); 
    while(digitalRead(LT_R)); 
  } else if (leftSee) {
    left(trackingSpeed, 0);
    while(digitalRead(LT_L));
  } else {
    stop(); 
  }
}

void selfDrivingLogic() {
  myservo.write(90);  
    
  if(sensorBypass) {
      forward(carSpeed); 
  } 
  else {
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
        
        if((rightDistance <= triggerDistance) && (leftDistance <= triggerDistance)) {
          back(carSpeed, reverseTime); 
          stop(200);            
          left(carSpeed, turnTime); 
          stop(200);
        }
        else if(rightDistance >= leftDistance) {
          back(carSpeed, 600); 
          stop(200);
          right(carSpeed, turnTime);
          stop(200);
        }
        else {
          back(carSpeed, 600); 
          stop(200);
          left(carSpeed, turnTime);
          stop(200);
        }
      }  
      else {
          forward(carSpeed);
      } 
  }
}

// ==========================================
// MOTOR CONTROL FUNCTIONS
// ==========================================
void gradualStop() {
  for (int i = carSpeed; i >= 0; i--) {
    analogWrite(ENA, i); analogWrite(ENB, i);
    delay(20); 
  }
  stop();
}

void forward(int speed){ 
  analogWrite(ENA, speed); analogWrite(ENB, speed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void back(int speed, int delayTime) {
  analogWrite(ENA, speed); analogWrite(ENB, speed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  if(delayTime > 0) delay(delayTime);
}

void left(int speed, int delayTime) {
  analogWrite(ENA, speed); analogWrite(ENB, speed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
  if(delayTime > 0) delay(delayTime);
}

void right(int speed, int delayTime) {
  analogWrite(ENA, speed); analogWrite(ENB, speed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  if(delayTime > 0) delay(delayTime);
}

void stop() {
  digitalWrite(ENA, LOW); digitalWrite(ENB, LOW);
} 

void stop(int delayTime) {
  digitalWrite(ENA, LOW); digitalWrite(ENB, LOW);
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
  
  if (dist == 0) return 999;
  return dist;
}
