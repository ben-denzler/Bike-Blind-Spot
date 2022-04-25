#include <TimerOne.h>
#include <SR04.h>

// Serial.print("GCD is: "); Serial.print(GCD); Serial.print('\n');   // DEBUGGING

#define LEFT_ECHO_PIN 11
#define LEFT_TRIG_PIN 12

SR04 LeftSensor = SR04(LEFT_ECHO_PIN, LEFT_TRIG_PIN);
long LeftSensorDistance;

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

static task task1, task2;
task* tasks[] = { &task1, &task2 };
const unsigned char numTasks = sizeof(tasks) / sizeof(tasks[0]);
const char startState = 0;    // Refers to first state enum
unsigned long GCD = 0;        // For timer period

// Task 1 (Blinks the onboard LED)
enum BL_States { BL_SMStart, BL_On, BL_Off };
int TickFct_BlinkLED(int state);

// Task 2 (Outputs distance from left ultrasonic sensor)
enum LS_States { LS_SMStart };
int TickFct_LeftSensor(int state) {
  LeftSensorDistance = LeftSensor.Distance();
  Serial.print(LeftSensorDistance);
  Serial.print("cm\n");
  return LS_SMStart;
}

void setup() {
  unsigned char j = 0;

  // Task 1 (Blinks the onboard LED)
  tasks[j]->state = startState;
  tasks[j]->period = 1000000;
  tasks[j]->elapsedTime = tasks[j]->period;
  tasks[j]->TickFct = &TickFct_BlinkLED;
  ++j;

  // Task 2 (Outputs distance from left ultrasonic sensor)
  tasks[j]->state = startState;
  tasks[j]->period = 1000000;
  tasks[j]->elapsedTime = tasks[j]->period;
  tasks[j]->TickFct = &TickFct_LeftSensor;
  ++j;

  // Find GCD for timer's period
  GCD = tasks[0]->period;
  for (unsigned char i = 1; i < numTasks; ++i) {
    GCD = gcd(GCD, tasks[i]->period); 
  }

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

int TickFct_BlinkLED(int state) {
  switch (state) {
    case BL_SMStart:
      state = BL_On;
      break;

    case BL_On:
      state = BL_Off;
      break;

    case BL_Off:
      state = BL_On;
      break;

    default:
      break;
  }
  switch(state) {
    case BL_SMStart:
      break;
    
    case BL_On:
      digitalWrite(LED_BUILTIN, HIGH);
      break;

    case BL_Off:
      digitalWrite(LED_BUILTIN, LOW);
      break;

    default:
      break;
  }
  return state;
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