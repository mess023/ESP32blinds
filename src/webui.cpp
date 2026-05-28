#include "blinds_core.h"
#include "stepper_interface.h"
#include "webui.h"
#include <Arduino.h> // For Serial
#include <MycilaWebSerial.h> // For webSerial
#include <Preferences.h>



extern WebSerial webSerial;

extern Preferences preferences;


extern bool calibratedStepper[2];
extern struct stepper_config_s stepper_config;




// Declare functions from main.cpp as extern
extern void runForward(uint8_t stepperId);
extern void stopMotors();
extern void startCalibrationStepper(uint8_t stepperId);

extern void startHomingSteppers(bool force);
extern void saveSteppersSpeed(uint32_t speed);
extern void saveSteppersAcceleration(uint32_t acceleration);
extern void startHomingStepper(uint8_t stepperId, bool force);

uint16_t speedControl, accelControl, safetyMarginControl;
uint16_t calibratedStatusStepper0, calibratedStatusStepper1;
uint16_t homedStatusStepper0, homedStatusStepper1;
uint16_t bottomScreenPositionSlider, topScreenPositionSlider;
uint16_t almStatusLabel;

uint16_t positionStepper0, positionStepper1;

void textCallback(Control *sender, int type);
void generalCallback(Control *sender, int type);

void numberCallback(Control *sender, int type){
    uint32_t value = sender->value.toInt();
    webSerial.printf("new value is  %u \n", value);
}

void updateCalibrationStatusStepper(uint8_t stepperId) {
    if (stepperId == 0) {
        ESPUI.updateLabel(calibratedStatusStepper0, calibratedStepper[0] ? "✅ Calibrated. Max position: " + String(maxPositionStepper[0]) : "❌ Not calibrated yet.");
    } else if (stepperId == 1) {
        ESPUI.updateLabel(calibratedStatusStepper1, calibratedStepper[1] ? "✅ Calibrated. Max position: " + String(maxPositionStepper[1]) : "❌ Not calibrated yet.");
    }
}

void updateHomingStatusStepper(uint8_t stepperId) {
    if (stepperId == 0) {
        ESPUI.updateLabel(homedStatusStepper0, sinceStartupHomedStepper[0] ? "✅ Homed" : "❌ Not homed yet.");
    } else if (stepperId == 1) {
        ESPUI.updateLabel(homedStatusStepper1, sinceStartupHomedStepper[1] ? "✅ Homed" : "❌ Not homed yet.");
    }
}

void updateBottomScreenPositionSlider(uint8_t percentage) {
    ESPUI.updateSlider(bottomScreenPositionSlider, percentage);
}
void updateTopScreenPositionSlider(uint8_t percentage) {
    ESPUI.updateSlider(topScreenPositionSlider, percentage);
}

void updateUICurrentPosition(uint8_t stepperId, long position){
    if (stepperId == 0) {
        ESPUI.updateLabel(positionStepper0, String(position));
    }
    else if (stepperId == 1) {
        ESPUI.updateLabel(positionStepper1, String(position));
    } 
}


void generalCallback(Control *sender, int type) {
	Serial.print("CB: id(");
	Serial.print(sender->id);
	Serial.print(") Type(");
	Serial.print(type);
	Serial.print(") '");
	Serial.print(sender->label);
	Serial.print("' = ");
	Serial.println(sender->value);
}


void speedSaveCallback(Control* sender, int type)
{
    switch (type)
    {
    
    case B_UP:
        // Handle button release
        uint32_t newSpeed = ESPUI.getControl(speedControl)->value.toInt();
        webSerial.printf("Speed value is  %u \n", newSpeed);
        saveSteppersSpeed(newSpeed);
        break;
    }
}



void accelerationSaveCallback(Control* sender, int type)
{
    switch (type)
    {
    
    case B_UP:
        // Handle button release
        uint32_t newAcceleration = ESPUI.getControl(accelControl)->value.toInt();
        webSerial.printf("Acceleration value is  %u \n", newAcceleration);
        saveSteppersAcceleration(newAcceleration);
        
        break;
    }
}

void forward0Callback(Control* sender, int type) {
    runForward(0);
}

void forward1Callback(Control* sender, int type) {
    runForward(1);
}

void stopCallback(Control* sender, int type) {
    stopMotors();
}

void bottomScreenSliderCallback(Control* sender, int type) {
    // Slider value is 0-100 (%); moveScreenSafelyFromNormalizedPosition wants 0..1.
    // It no-ops until the screen is calibrated and homed.
    moveScreenSafelyFromNormalizedPosition(0, sender->value.toFloat() / 100.0f);
}

void topScreenSliderCallback(Control* sender, int type) {
    moveScreenSafelyFromNormalizedPosition(1, sender->value.toFloat() / 100.0f);
}

void calibrateStepper0Callback(Control* sender, int type) {
    startCalibrationStepper(0);
}

void calibrateStepper1Callback(Control* sender, int type) {
    startCalibrationStepper(1);
}

void homeStepper0Callback(Control* sender, int type) {
    startHomingStepper(0, true);
}

void homeStepper1Callback(Control* sender, int type) {
    startHomingStepper(1, true);
}

void rebootCallback(Control* sender, int type) {
    if (type == B_UP) ESP.restart();
}

void updateAlmStatus(bool fault) {
    ESPUI.updateLabel(almStatusLabel,
        fault ? "⚠ DRIVER FAULT — motors stopped" : "✅ Drivers OK");
    ESPUI.setElementStyle(almStatusLabel,
        fault ? "color: white; background-color: #c0392b; width: 100%;"
              : "color: white; background-color: #27ae60; width: 100%;");
}

void safetyMarginSaveCallback(Control* sender, int type)
{
    switch (type)
    {
    
    case B_UP:
        // Handle button release
        uint32_t newSafetyMargin = ESPUI.getControl(safetyMarginControl)->value.toInt();
        webSerial.printf("Safety Margin value is  now: %u \n", newSafetyMargin);
        stepper_config.safetyMargin = newSafetyMargin;
        preferences.putLong("safetyMargin", newSafetyMargin);
        break;
    }
}


// Add more callback function implementations here as needed


// This is the main function which builds our GUI
void setupUI() {

    String clearLabelStyle = "background-color: unset; width: 100%;";
    String switcherLabelStyle = "width: 60px; margin-left: .3rem; margin-right: .3rem; background-color: unset;";

    // --------------------- Control tab ---------------------
    auto controlstab = ESPUI.addControl(Tab, "", "Control");
        almStatusLabel = ESPUI.addControl(Label, "Driver Status", "✅ Drivers OK", ControlColor::Emerald, controlstab);
        ESPUI.setElementStyle(almStatusLabel, "color: white; background-color: #27ae60; width: 100%;");

        ESPUI.addControl(Label, "⚠ Calibration required",
            "The position sliders only move a screen once it has been calibrated "
            "AND homed (use the buttons below). Until then they do nothing.",
            ControlColor::Alizarin, controlstab);
        bottomScreenPositionSlider = ESPUI.addControl(Slider, "Positions", "0", Dark, controlstab, bottomScreenSliderCallback);
        ESPUI.setVertical(bottomScreenPositionSlider);
        topScreenPositionSlider = ESPUI.addControl(Slider, "", "100", None, bottomScreenPositionSlider, topScreenSliderCallback);
        ESPUI.setVertical(topScreenPositionSlider);

        ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, bottomScreenPositionSlider), clearLabelStyle);
        ESPUI.setElementStyle(ESPUI.addControl(Label, "", "B", None, bottomScreenPositionSlider), switcherLabelStyle);
        ESPUI.setElementStyle(ESPUI.addControl(Label, "", "T", None, bottomScreenPositionSlider), switcherLabelStyle);

        //stepper0
        calibratedStatusStepper0 = ESPUI.addControl(Label, "Bottom Screen", "Not calibrated yet.", Dark, controlstab);
        updateCalibrationStatusStepper(0);
        ESPUI.addControl(Button, "Calibrate stepper0", "Calibrate stepper0", ControlColor::Dark, calibratedStatusStepper0, calibrateStepper0Callback);

        homedStatusStepper0 = ESPUI.addControl(Label, "Homed Stepper 0", "Not homed yet.", Dark, calibratedStatusStepper0);
        updateHomingStatusStepper(0);
        ESPUI.addControl(Button, "Home stepper0", "Home stepper0", ControlColor::Dark, calibratedStatusStepper0, homeStepper0Callback);
        positionStepper0 = ESPUI.addControl(Label, "currentPosition", "", ControlColor::Dark, calibratedStatusStepper0);
        ESPUI.addControl(Button, "forward", "forward", ControlColor::Dark, calibratedStatusStepper0, forward0Callback);
        ESPUI.addControl(Button, "stop", "stop", ControlColor::Alizarin, calibratedStatusStepper0, stopCallback);

        //stepper1
        calibratedStatusStepper1 = ESPUI.addControl(Label, "Top Screen", "Not calibrated yet.", Dark, controlstab);
        updateCalibrationStatusStepper(1);
        ESPUI.addControl(Button, "Calibrate stepper1", "Calibrate stepper1", ControlColor::Dark, calibratedStatusStepper1, calibrateStepper1Callback);

        homedStatusStepper1 = ESPUI.addControl(Label, "Homed Stepper 1", "Not homed yet.", Dark, calibratedStatusStepper1);
        updateHomingStatusStepper(1);
        ESPUI.addControl(Button, "Home stepper1", "Home stepper1", ControlColor::Dark, calibratedStatusStepper1, homeStepper1Callback);
        positionStepper1 = ESPUI.addControl(Label, "currentPosition", "", ControlColor::Dark, calibratedStatusStepper1);
        ESPUI.addControl(Button, "forward", "forward", ControlColor::Dark, calibratedStatusStepper1, forward1Callback);
        ESPUI.addControl(Button, "stop", "stop", ControlColor::Alizarin, calibratedStatusStepper1, stopCallback);

    
    
    // --------------------- Settings tab ---------------------
    auto settingstab = ESPUI.addControl(Tab, "", "Settings");
        speedControl = ESPUI.addControl(Number, "Speed", String(stepper_config.speed), ControlColor::Dark, settingstab , numberCallback);
        ESPUI.addControl(Min, "", "0", None, speedControl);
        ESPUI.addControl(Max, "", "30000", None, speedControl);
        ESPUI.addControl(Button, "Save", "Save", ControlColor::Dark, speedControl, speedSaveCallback);

        accelControl = ESPUI.addControl(Number, "Acceleration", String(stepper_config.acceleration), ControlColor::Dark, settingstab , numberCallback);
        ESPUI.addControl(Min, "", "0", None, accelControl);
        ESPUI.addControl(Max, "", "800000", None, accelControl);
        ESPUI.addControl(Button, "Save", "Save", ControlColor::Dark, accelControl, accelerationSaveCallback);

        safetyMarginControl = ESPUI.addControl(Number, "Safety Margin", String(stepper_config.safetyMargin), ControlColor::Dark, settingstab , numberCallback);
        ESPUI.addControl(Min, "", "0", None, safetyMarginControl);
        ESPUI.addControl(Max, "", "10000", None, safetyMarginControl);
        ESPUI.addControl(Button, "Save", "Save", ControlColor::Dark, safetyMarginControl, safetyMarginSaveCallback);

        ESPUI.addControl(Button, "Reboot", "Reboot device", ControlColor::Alizarin, settingstab, rebootCallback);

}