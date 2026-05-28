#ifndef STEPPER_INTERFACE_H
#define STEPPER_INTERFACE_H

#include <Arduino.h>
#include <FastAccelStepper.h> // For FastAccelStepper type

// Pin Definitions
//prototype
// #define stepPinStepper0 15 
// #define dirPinStepper0 14
// #define stepPinStepper1 32
// #define dirPinStepper1 33 

//production
#define stepPinStepper0 32 //CFG
#define dirPinStepper0 33  //485_EN
#define stepPinStepper1 15
#define dirPinStepper1 14 


#define DRIVER_MCPWM_PCNT 0


// ── Stepper-driver auto-disable (idle power saving) ──────────────────────────
// Releases both stepper drivers (motors go limp) after they have been idle for
// DRIVER_IDLE_DISABLE_MS, and re-enables them automatically on the next motion.
// Saves power / reduces driver + motor heating while the blinds are parked.
//
// Wire ENA+ on both drivers to 3.3V (through ~1kΩ), ENA- on both to GPIO 13.
// When GPIO 13 = HIGH the optocoupler conducts → drivers disabled (current cut).
// When GPIO 13 = LOW  the optocoupler is off  → drivers enabled (normal running).
#define DRIVER_ENABLE_PINS

#ifdef DRIVER_ENABLE_PINS
  #define enablePinStepper0          13    // shared GPIO for both driver ENABLE pins
  #define enablePinStepper1          13
  #define DRIVER_ENABLE_ACTIVE_LOW   true                  // LOW = enabled (optocoupler off)
  #define DRIVER_IDLE_DISABLE_MS     (2UL * 60UL * 1000UL) // 2 minutes idle → cut current
#endif


//make defines for default stepper values
#define defaultSpeed 30000
#define defaultAcceleration 30000
#define defaultHomingSpeed 10000
#define defaultCalibrationSpeed 15000
#define defaultCalibrationAcceleration 800000
#define defaultSafetyMargin 500

// Struct Definitions
struct stepper_config_s {
  uint8_t step;
  uint8_t direction;
  uint32_t speed;
  uint32_t homingSpeed;
  uint32_t acceleration;
  uint32_t calibrationSpeed;
  uint32_t calibrationAcceleration;
  uint32_t safetyMargin;
};

// Extern Variable Declarations for Stepper Control
extern long m1Max; // Max position for motor 1 (derived from calibration)
extern long m2Max; // Max position for motor 2 (derived from calibration)


extern bool currentlyHomingStepper[2];
extern bool sinceStartupHomedStepper[2];
extern bool currentlyCalibratingStepper[2];
extern bool calibratedStepper[2];

extern long stepsTraveledStepper[2];
extern uint32_t minPositionStepper[2];
extern long maxPositionStepper[2];

extern struct stepper_config_s stepper_config;
extern FastAccelStepper *stepper[2];
extern FastAccelStepperEngine engine;

// External variables from configLoader.h used in stepper configuration
extern bool reverseStepper0;
extern bool reverseStepper1;

// Function Prototypes for Stepper Control
void startHomingSteppers(bool force); // No default argument
void startCalibration();
void startCalibrationStepper(uint8_t stepperId); // Calibrate one screen (find travel range)
void startHomingStepper(uint8_t stepperId, bool force); // Home one screen
void finishHomingStepper(uint8_t stepperId);
void finishCalibrateStepper(uint8_t stepperId);
void loadStoredStepperValues(); // Function to load stored stepper values from preferences
void initializeSteppers(); // Function to initialize stepper motors
void setupWebSerialCommands(); // Function to setup webSerial message handler
void onLimitSwitchPressed(int buttonId); // Function to handle limit switch press events
void onLimitSwitchReleased(int buttonId); // Function to handle limit switch release events
void saveSteppersAcceleration(uint32_t acceleration); // Function to save and apply stepper acceleration
void saveSteppersSpeed(uint32_t speed); // Function to save and apply stepper speed
void runForward(); // Function to run stepper forward
void stopMotors(); // Function to stop all motors
void moveScreenSafelyFromNormalizedPosition(uint8_t stepperId, float value); // Function to move screen safely from normalized position
void updateDriverEnable(); // Idle auto-disable manager — call every loop(); no-op until DRIVER_ENABLE_PINS is defined

#endif // STEPPER_INTERFACE_H