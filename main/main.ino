#include <TimerOne.h>
#include <SR04.h>

#define LEFT_TURN_SIGNAL  26
#define RIGHT_TURN_SIGNAL 45
#define LEFT_TRIG_PIN     12
#define LEFT_ECHO_PIN     11
#define RIGHT_TRIG_PIN    10 
#define RIGHT_ECHO_PIN    9
#define RED_LEFT          8
#define GREEN_LEFT        7
#define BLUE_LEFT         6
#define RED_RIGHT         5
#define GREEN_RIGHT       4
#define BLUE_RIGHT        3
#define BUZZER            2

int LeftTurnEnabled;
int RightTurnEnabled;
SR04 LeftSensor = SR04(LEFT_ECHO_PIN, LEFT_TRIG_PIN);
SR04 RightSensor = SR04(RIGHT_ECHO_PIN, RIGHT_TRIG_PIN);
long LeftSensorDistance;
long RightSensorDistance;

// Computes GCD of two numbers
unsigned long gcd(unsigned long a, unsigned long b) {
  if (b == 0) return a;
  return gcd(b, a % b);
}

// Used for timer interrupts
volatile unsigned char timerFlag = 0;
void TimerISR() { 
  timerFlag = 1;
}

typedef struct task {
  int state;                    // Task's current state
  unsigned long period;         // Task's period in ms
  unsigned long elapsedTime;    // Time elapsed since last tick
  int (*TickFct)(int);          // Address of tick function
} task;

static task task1, task2, task3, task4, task5;
task* tasks[] = { &task1, &task2, &task3, &task4, &task5 };
const unsigned char numTasks = sizeof(tasks) / sizeof(tasks[0]);
const char startState = 0;    // Refers to first state enum
unsigned long GCD = 0;        // For timer period

// Sets color of left LED
void setColorLeft(unsigned int red, unsigned int green, unsigned int blue) {
  analogWrite(RED_LEFT, red);
  analogWrite(GREEN_LEFT, green);
  analogWrite(BLUE_LEFT, blue);
}

// Sets color of right LED
void setColorRight(unsigned int red, unsigned int green, unsigned int blue) {
  analogWrite(RED_RIGHT, red);
  analogWrite(GREEN_RIGHT, green);
  analogWrite(BLUE_RIGHT, blue);
}

// Task 1 (Checks if a turn signal is active)
enum US_States { US_SMStart };
int TickFct_UpdateSignals(int state) {
  LeftTurnEnabled = digitalRead(LEFT_TURN_SIGNAL);
  RightTurnEnabled = digitalRead(RIGHT_TURN_SIGNAL);
  return state;
}

// Task 2 (Outputs distance from left ultrasonic sensor)
enum LS_States { LS_SMStart };
int TickFct_LeftSensor(int state) {
  long sensorDist = LeftSensor.Distance();
  if (sensorDist <= 400) {
    LeftSensorDistance = sensorDist;
  }
  return LS_SMStart;
}

// Task 3 (Outputs distance from right ultrasonic sensor)
enum RS_States { RS_SMStart };
int TickFct_RightSensor(int state) {
  long sensorDist = RightSensor.Distance();
  if (sensorDist <= 400) {
    RightSensorDistance = sensorDist;
  }
  return RS_SMStart;
}

// Task 4 (Updates speakers and LEDs based on sensor distance)
enum UL_States { UL_SMStart };
int TickFct_UpdateLEDs(int state) {
  
  if (RightTurnEnabled) {
    if (RightSensorDistance >= 200) setColorRight(0, 255, 0);         // Green
    else if (RightSensorDistance >= 100) setColorRight(0, 0, 255);    // Blue
    else setColorRight(255, 0, 0);                                    // Red
  }
  else setColorRight(0, 0, 0);

  if (LeftTurnEnabled) {
    if (LeftSensorDistance >= 200) setColorLeft(0, 255, 0);           // Green
    else if (LeftSensorDistance >= 100) setColorLeft(0, 0, 255);      // Blue
    else setColorLeft(255, 0, 0);                                     // Red
  } 
  else setColorLeft(0, 0, 0);

  return UL_SMStart;
}

// Task 5 (Outputs frequency on speaker)
enum SS_States { SS_SMStart, SS_Wait, SS_MakeSound };
int TickFct_SoundSpeaker(int state) {
  switch (state) {  // State transitions
    case SS_SMStart:
      state = SS_Wait;
      break;

    case SS_Wait:
      if ((LeftSensorDistance < 100 && LeftTurnEnabled) || (RightSensorDistance < 100 && RightTurnEnabled)) {
        state = SS_MakeSound;
      } else {
        state = SS_Wait;
      }
      break;
    
    case SS_MakeSound:
      if (!((LeftSensorDistance < 100 && LeftTurnEnabled) || (RightSensorDistance < 100 && RightTurnEnabled))) {
        state = SS_Wait;
      } else {
        state = SS_MakeSound;
      }
      break;

    default:
      state = SS_SMStart;
      break;
  }
  switch (state) {  // State actions
    case SS_Wait:
      noTone(BUZZER);
      break;

    case SS_MakeSound:
      tone(BUZZER, 2000);
      break;

    default:
      break;
  }
  return state;
}

void setup() {
  unsigned char j = 0;

  // Task 1 (Checks if a turn signal is active)
  tasks[j]->state = startState;
  tasks[j]->period = 100000;
  tasks[j]->elapsedTime = tasks[j]->period;
  tasks[j]->TickFct = &TickFct_UpdateSignals;
  ++j;

  // Task 2 (Outputs distance from left ultrasonic sensor)
  tasks[j]->state = startState;
  tasks[j]->period = 500000;
  tasks[j]->elapsedTime = tasks[j]->period;
  tasks[j]->TickFct = &TickFct_LeftSensor;
  ++j;

  // Task 3 (Outputs distance from right ultrasonic sensor)
  tasks[j]->state = startState;
  tasks[j]->period = 500000;
  tasks[j]->elapsedTime = tasks[j]->period;
  tasks[j]->TickFct = &TickFct_RightSensor;
  ++j;

 // Task 4 (Updates LEDs based on sensor distance)
  tasks[j]->state = startState;
  tasks[j]->period = 500000;
  tasks[j]->elapsedTime = tasks[j]->period;
  tasks[j]->TickFct = &TickFct_UpdateLEDs;
  ++j;

 // Task 5 (Outputs frequency on speaker)
  tasks[j]->state = startState;
  tasks[j]->period = 500000;
  tasks[j]->elapsedTime = tasks[j]->period;
  tasks[j]->TickFct = &TickFct_SoundSpeaker;
  ++j;

  // Find GCD for timer's period
  GCD = tasks[0]->period;
  for (unsigned char i = 1; i < numTasks; ++i) {
    GCD = gcd(GCD, tasks[i]->period); 
  }

  pinMode(LEFT_TURN_SIGNAL, INPUT);   // Input from Ashley's board
  pinMode(RIGHT_TURN_SIGNAL, INPUT);  // Input from Ashley's board
  pinMode(RED_LEFT, OUTPUT);          // Left RGB red
  pinMode(GREEN_LEFT, OUTPUT);        // Left RGB green
  pinMode(BLUE_LEFT, OUTPUT);         // Left RGB blue
  pinMode(RED_RIGHT, OUTPUT);         // Right RGB red
  pinMode(GREEN_RIGHT, OUTPUT);       // Right RGB green
  pinMode(BLUE_RIGHT, OUTPUT);        // Right RGB blue
  pinMode(BUZZER, OUTPUT);            // Buzzer pin
  pinMode(LED_BUILTIN, OUTPUT);       // Testing
  Serial.begin(9600);                 // Baud rate is 9600 (serial output)
  Timer1.initialize(GCD);             // GCD is in microseconds
  Timer1.attachInterrupt(TimerISR);   // TimerISR runs on interrupt
}

void loop() {
  for (unsigned char i = 0; i < numTasks; ++i) {    // Task scheduler
    if (tasks[i]->elapsedTime >= tasks[i]->period) {
      tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
      tasks[i]->elapsedTime = 0;
    }
    tasks[i]->elapsedTime += GCD;
  }
  while (!timerFlag);
  timerFlag = 0;
}

/* WORKS CITED
-------------------------------------
Learned how to use sizeof():
https://en.cppreference.com/w/cpp/language/sizeof

Learned how to use gcd():
https://en.cppreference.com/w/cpp/numeric/gcd

Using the TimerOne library for interrupts:
https://www.arduino.cc/reference/en/libraries/timerone/ 

GCD function (Euclid's algorithm):
https://www.tutorialspoint.com/c-program-to-implement-euclid-s-algorithm
*/