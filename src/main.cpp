#include <Arduino.h>
#include <ArtnetETH.h>
#include <ArduinoOSCETH.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <ESPUI.h>
#include <ezButton.h>
#include <FastAccelStepper.h>
#include <MycilaWebSerial.h>
#include <Preferences.h>
#include "configLoader.h"
#include "stepper_interface.h"
#include "blinds_core.h"
#include "webui.h"
#include "ota.h"
#include "webserial.h"

WebSerial webSerial;

// ALM monitoring — wire ALM+ on both drivers to 3.3V, ALM- on both to GPIO 5.
// Normal: transistor off → pin HIGH (pull-up). Fault: transistor on → pin LOW.
#define ALM_PIN 5
static bool almFault = false;

ezButton button1(2); //bottom screen startswitch
ezButton button2(4); //bottom screen endswitch
ezButton button3(17); //top screen startswitch
ezButton button4(12); //top screen endswitch




Preferences preferences;

ArtnetReceiver artnet;
uint16_t universe1 = 1; // 0 - 32767
uint8_t net = 0;        // 0 - 127
uint8_t subnet = 0;     // 0 - 15
uint8_t universe2 = 2;  // 0 - 15

unsigned long lastHeartbeat = 0;


void onOSCReceivedBottomScreenPosition(OscMessage& m) {
    //webSerial.printf("received bottom position: %f", m.arg<float>(0));
    moveScreenSafelyFromNormalizedPosition(0, float(m.arg<float>(0)));
}
void onOSCReceivedTopScreenPosition(OscMessage& m) {
    //webSerial.printf("received top position: %f", m.arg<float>(0));
    moveScreenSafelyFromNormalizedPosition(1, float(m.arg<float>(0)));
}

// ── Calibration triggers (so the desktop controller can calibrate over OSC) ──
void onOSCCalibrateBottom(OscMessage& m) { startCalibrationStepper(0); }
void onOSCCalibrateTop(OscMessage& m)    { startCalibrationStepper(1); }
void onOSCCalibrateBoth(OscMessage& m) {
    startCalibrationStepper(0);
    startCalibrationStepper(1);
}

// ── Status reply: send calibration + live position back to the requester ─────
// Reply args per screen: calibrated(0/1), homed(0/1), maxSteps, currentPosition.
void onOSCStatusRequest(OscMessage& m) {
    OscEther.send(m.remoteIP(), m.remotePort(), "/status",
        (int)calibratedStepper[0], (int)sinceStartupHomedStepper[0],
        (int)maxPositionStepper[0], (int)stepper[0]->getCurrentPosition(),
        (int)calibratedStepper[1], (int)sinceStartupHomedStepper[1],
        (int)maxPositionStepper[1], (int)stepper[1]->getCurrentPosition());
}


void setup() {
  Serial.begin(115200);
  
  if (!loadConfiguration()){
    Serial.println("config loading fucked");
  } else{
    Serial.println("config loaded");
  };
  
  pinMode(ALM_PIN, INPUT_PULLUP);

  button1.setDebounceTime(50); // set debounce time to 50 milliseconds
  button2.setDebounceTime(50); // set debounce time to 50 milliseconds
  button3.setDebounceTime(50); // set debounce time to 50 milliseconds
  button4.setDebounceTime(50); // set debounce time to 50 milliseconds


  

  // Open Preferences with 'blinds' namespace. Each application module, library, etc
  // has to use a namespace name to prevent key name collisions. We will open storage in
  // RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars.
  preferences.begin("blinds", false);
  
  ETH.begin();
  ETH.config(ip, gateway, subnet_mask);
  MDNS.begin(DNSName); 
  
  setupOTA();

  loadStoredStepperValues();
  initializeSteppers();

  setupWebSerialCommands();
 



  setupUI();
  ESPUI.setVerbosity(Verbosity::VerboseJSON);

  // Create a static buffer for the webpage title
  static char titleBuffer[64];
  strcpy(titleBuffer, DNSName);
  strcat(titleBuffer, " Control");
  ESPUI.begin(titleBuffer);
  webSerial.begin(ESPUI.WebServer()); // Initialize WebSerial with ESPUI's server
  

  OscEther.subscribe(7000, "/btm/pos", onOSCReceivedBottomScreenPosition);
  OscEther.subscribe(7000, "/top/pos", onOSCReceivedTopScreenPosition);
  OscEther.subscribe(7000, "/btm/calibrate", onOSCCalibrateBottom);
  OscEther.subscribe(7000, "/top/calibrate", onOSCCalibrateTop);
  OscEther.subscribe(7000, "/calibrate", onOSCCalibrateBoth);
  OscEther.subscribe(7000, "/status", onOSCStatusRequest);

  artnet.begin();
  artnet.subscribeArtDmxUniverse(net, subnet, universe1, onArtnetReceive);

  
  //startHomingSteppers(true);
}



void loop() {
  button1.loop();
  button2.loop();
  button3.loop();
  button4.loop();
  if(button1.isPressed()){
    onLimitSwitchPressed(1);
  }
  if(button1.isReleased()){
    onLimitSwitchReleased(1);
  }
  if(button2.isPressed()){
    onLimitSwitchPressed(2);
  }
  if(button2.isReleased()){
    onLimitSwitchReleased(2);
  }
  if(button3.isPressed()){
    onLimitSwitchPressed(3);
  }
  if(button3.isReleased()){
    onLimitSwitchReleased(3);
  }
  if(button4.isPressed()){
    onLimitSwitchPressed(4);
  }
  if(button4.isReleased()){
    onLimitSwitchReleased(4);
  }

  handleOTA();
  OscEther.update();
  artnet.parse();  // check if artnet packet has come and execute callback function

  updateDriverEnable();  // idle auto-disable

  bool almNow = (digitalRead(ALM_PIN) == LOW);
  if (almNow != almFault) {
    almFault = almNow;
    if (almFault) stopMotors();
    updateAlmStatus(almFault);
  }

  long currentPositionStepper0 = stepper[0]->getCurrentPosition();
  long currentPositionStepper1 = stepper[1]->getCurrentPosition();

  updateUICurrentPosition(0, currentPositionStepper0);
  updateUICurrentPosition(1, currentPositionStepper1);
  //webSerial.printf("current raw position stepper0: %li", currentPositionStepper0);
}


void onArtnetReceive(const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
    // will be called on incoming artnet data

    // This window reads its own 4-channel block within the universe, starting at
    // dmxAddress (1-based). Window1=1, Window2=5, Window3=9, Window4=13.
    uint16_t off = dmxAddress - 1;            // byte index of first channel
    if (off + 3 >= size) {
        return;                               // packet too short for our block
    }

    //combine two bytes into a 16 bit value
    uint16_t m1 = (data[off + 0] * 256) + data[off + 1];
    //webSerial.print("M1: %hi", m1);

    uint16_t m2 = (data[off + 2] * 256) + data[off + 3];
    //webSerial.print("M2: %hi", m2);

    if (calibratedStepper[0]){
      long remappedPosStepper0 = (long)(((long long)m1 * maxPositionStepper[0]) / 65535LL);

      webSerial.printf(">>> moving stepper0 to: %li",remappedPosStepper0 );

      stepper[0]->moveTo(remappedPosStepper0);
    } else{
      //webSerial.println("> incoming artnet but stepper0 not calibrated");
    }
    if (calibratedStepper[1]){
      long remappedPosStepper1 = (long)(((long long)m2 * maxPositionStepper[1]) / 65535LL);
      //webSerial.print(">>> moving stepper1 to: %li",remappedPosStepper1 );
      stepper[1]->moveTo(remappedPosStepper1);
    } else{
      //webSerial.println("> incoming artnet but stepper1 not calibrated");
    }
    
}
