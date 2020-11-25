#include "src/PID_AutoTune_v0/PID_AutoTune_v0.h"
PID_ATune tuner(&pidInput, &pidOutput);

#define STABLE_TEMP_WAIT 3*60000
#define STABLE_TEMP_DIFF 0.2
#define TUNE_TEMP_OFFSET 5
#define WAIT_RESULT_COUNT 10 

void tune_pid_loop() {
  float diff;
  
  static unsigned long time, startTime, lastOutputChange;
  static float lastTemperature;
  static float output;
  static float saveKp = 0;
  static float saveKi;
  static float saveKd;
  
  if(pidTunerState == 0) return;

  time = millis();
  
  switch (pidTunerState) {
  case 1:  //Start tune process
    if (isStandby) return;
    pidTunerState = 3;
    saveKp = regulator.GetKp();
    saveKi = regulator.GetKi();
    saveKd = regulator.GetKd();
    regulator.SetTunings(saveKp/2, 0, 0);
    startTime = time;
    output = 0;
    lastOutputChange = time;
    lastTemperature = global_current_temp_f;
    pidTunerStateStr  = "starting";
    break;

  case 2:  //Stop tune process
    pidTunerState = 0;
    if(saveKp != 0) {
      regulator.SetTunings(saveKp, saveKi, saveKd);
      regulator.SetMode(AUTOMATIC);
      saveKp = 0;
    }
    pidTunerStateStr  = "";
    break;

  case 3:   // waiting for stable temperature
    if (fabs(global_current_temp_f - lastTemperature) > STABLE_TEMP_DIFF) {
      startTime = time;
      lastTemperature = global_current_temp_f;
      break;
    }
    if (time - startTime >= STABLE_TEMP_WAIT) {  
      pidTunerState = 4;  
      output = heaterPowerLog.getMean() * 100.0;
    }
    break;
 
  case 4:   // start tuner
    pidTunerState = 5;
    pidTunerStateStr  = "in process";
    
    regulator.SetMode(DIRECT); //set PID to Manual mode
    tuner.SetControlType(1); //0=PI, 1=PID    
    tuner.SetNoiseBand(0.05);
    tuner.SetOutputStep(output);
    pidOutput = output;

  case 5:   // tuner loop
    pidInput = global_current_temp_f;
    byte resultReady = (tuner.Runtime());
    heater.setPWM(pidOutput);

    if (resultReady) {
      saveKp = 0;
      pidTunerState = TUNE_STOP; //Stop pid tuner
      
      global_pid_kp = tuner.GetKp();
      global_pid_ki = tuner.GetKi();
      global_pid_kd = tuner.GetKd();
      regulator.SetTunings(global_pid_kp, global_pid_ki, global_pid_kd);
      regulator.SetMode(AUTOMATIC);
      
      writeConfig();
    }
    break;
  }

}
