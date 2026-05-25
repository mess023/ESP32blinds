#include "stepper_interface.h"
#include <Preferences.h>
#include <MycilaWebSerial.h>
#include "configLoader.h"
#include <FastAccelStepper.h>
#include <webui.h>

// External references
extern Preferences preferences;
extern WebSerial webSerial;

// ── Driver auto-disable helpers (see DRIVER_ENABLE_PINS in stepper_interface.h)
#ifdef DRIVER_ENABLE_PINS
static unsigned long lastStepperMotionMs = 0;
static bool driversCurrentlyEnabled = true;

static void setDriversEnabled(bool enabled) {
  uint8_t level;
  if (DRIVER_ENABLE_ACTIVE_LOW) {
    level = enabled ? LOW : HIGH;
  } else {
    level = enabled ? HIGH : LOW;
  }
  digitalWrite(enablePinStepper0, level);
  digitalWrite(enablePinStepper1, level);
  driversCurrentlyEnabled = enabled;
}
#endif

// Add definitions for these arrays
bool currentlyHomingStepper[2] = {false, false};
bool sinceStartupHomedStepper[2] = {false, false};

bool currentlyCalibratingStepper[2] = {false, false};
bool calibratedStepper[2] = {false, false};

long stepsTraveledStepper[2];
uint32_t minPositionStepper[2] = {0,0};
long maxPositionStepper[2];



struct stepper_config_s stepper_config;

FastAccelStepper* stepper[2];  // Array of pointers to FastAccelStepper objects
FastAccelStepperEngine engine;

void loadStoredStepperValues() {
  //check if we can load stored stepper values
  
  stepper_config.speed = preferences.getLong("speed", defaultSpeed);
  stepper_config.acceleration = preferences.getLong("acceleration", defaultAcceleration);
  stepper_config.homingSpeed = preferences.getLong("homingSpeed", defaultHomingSpeed);
  stepper_config.calibrationSpeed = preferences.getLong("calibrationSpeed", defaultCalibrationSpeed);
  stepper_config.calibrationAcceleration = preferences.getLong("calibrationAcceleration", defaultCalibrationAcceleration);
  stepper_config.safetyMargin = preferences.getLong("safetyMargin", defaultSafetyMargin);

  long savedStepper0 = preferences.getLong("maxStepper0", 0);
  if (savedStepper0 != 0) {
    maxPositionStepper[0] = savedStepper0;
    calibratedStepper[0] = true;
    webSerial.printf("stepper0 calibration found: %li", maxPositionStepper[0]);
  }
  
  long savedStepper1 = preferences.getLong("maxStepper1", 0);
  if (savedStepper1 != 0) {
    maxPositionStepper[1] = savedStepper1;
    calibratedStepper[1] = true;
    webSerial.printf("stepper1 calibration found: %li", maxPositionStepper[1]);
  }
  
  
  // ##### end load calibration ######
}


void initializeSteppers() {
  // ---------- initialize stepper engine -------------
  engine.init();
  
  //stepper0
  FastAccelStepper *s = engine.stepperConnectToPin(stepPinStepper0, DRIVER_MCPWM_PCNT);
  bool activeLow = !reverseStepper0;
  s->setDirectionPin(dirPinStepper0, activeLow); // Pass pin AND activeLow setting
  s->setSpeedInHz(stepper_config.speed);
  s->setAcceleration(stepper_config.acceleration);
  stepper[0] = s;

  //stepper1
  s = engine.stepperConnectToPin(stepPinStepper1, DRIVER_MCPWM_PCNT);
  activeLow = !reverseStepper1;
  s->setDirectionPin(dirPinStepper1, activeLow); // Pass pin AND activeLow setting
  s->setAcceleration(stepper_config.acceleration);
  stepper[1] = s;

#ifdef DRIVER_ENABLE_PINS
  // Set up the driver ENABLE outputs and start enabled.
  pinMode(enablePinStepper0, OUTPUT);
  pinMode(enablePinStepper1, OUTPUT);
  setDriversEnabled(true);
  lastStepperMotionMs = millis();
#endif
}

// Call every loop(): disables both drivers after DRIVER_IDLE_DISABLE_MS of no
// motion and re-enables them the moment a stepper starts moving again.
// Compiles to a no-op until DRIVER_ENABLE_PINS is defined.
void updateDriverEnable() {
#ifdef DRIVER_ENABLE_PINS
  bool moving = stepper[0]->isRunning() || stepper[1]->isRunning();
  if (moving) {
    lastStepperMotionMs = millis();
    if (!driversCurrentlyEnabled) {
      setDriversEnabled(true);
      webSerial.println(">>> motion detected: stepper drivers re-enabled");
    }
  } else if (driversCurrentlyEnabled &&
             (millis() - lastStepperMotionMs > DRIVER_IDLE_DISABLE_MS)) {
    setDriversEnabled(false);
    webSerial.println(">>> idle > 5 min: stepper drivers disabled to save power");
  }
#endif
}

void startCalibrationStepper(uint8_t stepperId) {
  currentlyCalibratingStepper[stepperId] = true;
  calibratedStepper[stepperId] = false;

  stepper[stepperId]->setSpeedInHz(stepper_config.calibrationSpeed);
  stepper[stepperId]->setAcceleration(stepper_config.calibrationAcceleration);
  stepper[stepperId]->runForward();
}


void finishCalibrateStepper(uint8_t stepperId){
  // we are now back at start position.
  // Current position will be the total travel distance (likely negative from moving backward).
  stepsTraveledStepper[stepperId] = stepper[stepperId]->getCurrentPosition();
  stepper[stepperId]->setCurrentPosition(0);
  // Use absolute value for maxPositionStepper to ensure positive travel range
  maxPositionStepper[stepperId] = abs(stepsTraveledStepper[stepperId]);
  webSerial.printf("Calibration: raw position=%li, using absolute value=%li", 
                  stepsTraveledStepper[stepperId], maxPositionStepper[stepperId]);
  currentlyCalibratingStepper[stepperId] = false;
  calibratedStepper[stepperId] = true;
  updateCalibrationStatusStepper(stepperId);

  //also we're homed now
  currentlyHomingStepper[stepperId] = false;
  sinceStartupHomedStepper[stepperId] = true;
  updateHomingStatusStepper(stepperId);
  if (stepperId == 0){
    preferences.putLong("maxStepper0",maxPositionStepper[0]);
    // Force synchronization by ending and reopening preferences
    preferences.end();
    preferences.begin("blinds", false);
    webSerial.printf("saved maxStepper0: %li",maxPositionStepper[0]);
  }
  if (stepperId == 1){
    preferences.putLong("maxStepper1",maxPositionStepper[1]);
    // Force synchronization by ending and reopening preferences
    preferences.end();
    preferences.begin("blinds", false);
    webSerial.printf("saved maxStepper1: %li",maxPositionStepper[1]);
  }

  // we're ready to rumble: set stepper to fullspeed, normal acceleration
  stepper[stepperId]->setSpeedInHz(stepper_config.speed);
  stepper[stepperId]->setAcceleration(stepper_config.acceleration);
}

void startHomingStepper(uint8_t stepperId, bool force) {
 
  currentlyHomingStepper[stepperId] = true;
  stepper[stepperId]->setSpeedInHz(stepper_config.homingSpeed);
  stepper[stepperId]->runBackward();
  webSerial.printf(">>> starting homing for stepper %li", stepperId);

}
  
void finishHomingStepper(uint8_t stepperId){
  stepper[stepperId]->stopMove();
  stepper[stepperId]->setCurrentPosition(0);
  currentlyHomingStepper[stepperId] = false;
  sinceStartupHomedStepper[stepperId] = true;
  webSerial.printf(">>> Homing stepper %li complete!", stepperId);
  stepper[stepperId]->setSpeedInHz(stepper_config.speed);
  updateHomingStatusStepper(stepperId);
}
  


void saveSteppersAcceleration(uint32_t acceleration) {
  stepper_config.acceleration = acceleration;
  preferences.putLong("acceleration", acceleration);
  stepper[0]->setAcceleration(acceleration);
  stepper[1]->setAcceleration(acceleration);
}

void saveSteppersSpeed(uint32_t speed) {
  stepper_config.speed = speed;
  preferences.putLong("speed", speed);
  stepper[0]->setSpeedInHz(speed);
  stepper[1]->setSpeedInHz(speed);
}
void saveSafetyMargin(uint32_t safetyMargin) {
  stepper_config.safetyMargin = safetyMargin;
  preferences.putLong("safetyMargin", safetyMargin);
}


void runForward(uint8_t stepperId) {
  if (stepperId > 1) {
    webSerial.println("Invalid stepper ID");
    return;
  }
  stepper[stepperId]->runForward();
}

void stopMotors() {
  stepper[0]->stopMove();
  stepper[1]->stopMove();
}

void onLimitSwitchPressed(int buttonId){
  webSerial.printf("Button %d pressed", buttonId);
  Serial.println(buttonId);
  switch (buttonId) {
    case 1:
    {
      //limitStartStepper0
      stepper[0]->stopMove();
      webSerial.println("limitStartStepper0 pressed");
      webSerial.printf("rawposition: %li", stepper[0]->getCurrentPosition());
      //currentlyHomingStepper[0] = true;
      stepper[0]->setSpeedInHz(stepper_config.homingSpeed/10);
      stepper[0]->runForward();
      break;
    }
    case 2:
    {
      //limitEndStepper0
      stepper[0]->stopMove();
      webSerial.println("limitEndStepper0 pressed");
      stepper[0]->setSpeedInHz(stepper_config.homingSpeed/10);
      stepper[0]->runBackward();
      break;
    }
    case 3:
    {
      //limitStartStepper1
      stepper[1]->stopMove();
      //currentlyHomingStepper[1] = true;
      stepper[1]->setSpeedInHz(stepper_config.homingSpeed/10);
      stepper[1]->runForward();
      break;
    }
    case 4:
    {
      //limitEndStepper1
      stepper[1]->stopMove();
      stepper[1]->setSpeedInHz(stepper_config.homingSpeed/10);
      stepper[1]->runBackward();
      break;
    }
  }
}

void onLimitSwitchReleased(int buttonId){
  switch (buttonId) {
    case 1:
    {
      //limitStartStepper0
      stepper[0]->stopMove();
      if (currentlyHomingStepper[0]){
        finishHomingStepper(0);
      }

      if (currentlyCalibratingStepper[0]){
        webSerial.printf("final End position reached during calibration %li", stepper[0]->getCurrentPosition());
        finishCalibrateStepper(0);
      }
      break;
    }    case 2:
    {
      //limitEndStepper0
      stepper[0]->stopMove();
      if (currentlyCalibratingStepper[0]){
        // When we reach the end position during calibration, set the position to 0
        // so that we can measure the total travel distance when moving back
        webSerial.printf("Start position reached during calibration %li", stepper[0]->getCurrentPosition());
        stepper[0]->setCurrentPosition(0);
        stepper[0]->setSpeedInHz(stepper_config.calibrationSpeed);
        stepper[0]->runBackward();
        webSerial.println("End position reached during calibration, position reset to 0, moving backward");
      }
      break;
    }
    case 3:
    {
      //limitStartStepper1
      stepper[1]->stopMove();
      
      if (currentlyHomingStepper[1]){
        finishHomingStepper(1);
      }

      if (currentlyCalibratingStepper[1]){
        finishCalibrateStepper(1);
      }
      break;
    }    case 4:
    {
      //limitEndStepper1
      stepper[1]->stopMove();
      if (currentlyCalibratingStepper[1]){
        // When we reach the end position during calibration, set the position to 0
        // so that we can measure the total travel distance when moving back
        stepper[1]->setCurrentPosition(0);
        stepper[1]->setSpeedInHz(stepper_config.calibrationSpeed);
        stepper[1]->runBackward();
        webSerial.println("End position reached during calibration, position reset to 0, moving backward");
      }
      break;
    }
  }
}

void moveScreenSafelyFromNormalizedPosition(uint8_t stepperId, float normalizedPosition) {
  if (calibratedStepper[stepperId] == false){
    webSerial.println(">>> stepper not yet calibrated, OSC positions ignored");
    return;
  }
  if (sinceStartupHomedStepper[stepperId] == false){
    webSerial.println(">>> stepper not yet homed, OSC positions ignored");
    return;
  } 
  if (normalizedPosition < 0.0 || normalizedPosition > 1.0){
    webSerial.println(">>> invalid OSC position, must be between 0 and 1");
    return;
  }
  //webSerial.printf(">>> stepper %i OSC input value: %f", stepperId, normalizedPosition);
  // Safety margin
  long safetyMargin = stepper_config.safetyMargin;
  //webSerial.printf(">>> safety margin: %li", safetyMargin);
  long safetyMinimum = 0 + safetyMargin;
  long safetyMaximum = maxPositionStepper[stepperId] - safetyMargin;
  long remappedValue = (long)(normalizedPosition * (float)(safetyMaximum - safetyMinimum) + safetyMinimum);
  //webSerial.printf("- safetyMaximum: %li", safetyMaximum);
  //webSerial.printf("- remapped value: %li", remappedValue);
  long currentPosition = stepper[stepperId]->getCurrentPosition();
  webSerial.printf("stepper: %i osc input: %f safetyMin: %li safetyMax: %li remappedto: %li currentposition: %li" , stepperId, normalizedPosition, safetyMinimum, safetyMaximum, remappedValue, currentPosition);
  
  stepper[stepperId]->moveTo(remappedValue);
  if (stepperId == 0) {
    updateBottomScreenPositionSlider(normalizedPosition * 100);
  } else {
    updateTopScreenPositionSlider(normalizedPosition * 100);
  }
}

