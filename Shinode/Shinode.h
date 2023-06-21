#include <string>
#include "Arduino.h"

#ifndef Shinode_h
#define Shinode_h

class Shinode {
  public:
    std::string device_id;
    std::string sensors;
    std::string controls;
    std::string polling_interval;
    std::string httpClient;
    std::string token;
    std::string last_poll;

    Shinode(
      std::string device_id, 
      std::string sensors,
      std::string controls,
      std::string httpClient,
      std::string token
    ) {
      // make an authorised request to host server containing:
      // - device id
      // - sensor data config
      // - control action config
      
      // receive system config from server containing:
      // - polling interval
      const char* config = "request(/shinode-register/[this.device_id])";
    }

    // implemented by user, must return array of sensor data objects
    // matching sensors property structure 
    int sense() {}

    // implemented by user, must receive an array of control objects
    // matching controls property structure
    void control (std::string control) {}

    // called in device update loop to sync to host
    void sync () {
      // check if last poll is older than polling period 
      if (true) {
        const int sensor_data = sense();
        const char* controls = "request(/shinode-sync/[this.device_id])";
        control(controls);
      }
    }
};

#endif