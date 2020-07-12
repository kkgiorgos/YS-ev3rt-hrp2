/**
 * \file    ev3cxx_infrared_sensor.h
 * \brief	EV3RT CPP API for infrared sensor
 * \author	Jaroslav Páral (jarekparal)
 */

#pragma once

#include <stdio.h>

#include "ev3api.h"
#include "ev3cxx_sensor.h"

namespace ev3cxx {
    
/**
 * \~English
 * \brief    Class InfraredSensor. API for working with infrared sensor.
 */
class InfraredSensor : public Sensor
{
public:
    /**
     * \~English
     * \brief       Constructor of class InfraredSensor. 
     * \param port  Port index (1-4).      
     */  
    InfraredSensor(SensorPort port) : Sensor(port, INFRARED_SENSOR) {}

    /**
     * \~English
     * \brief       Get the distance using the infrared sensor. 
	 * \details 	Always returns 0 (error log is outputted) when an invalid sensor number is specified.
     * \return      Distance in percentage (0-100).      
     */  
    int distance() {
        return ev3_infrared_sensor_get_distance(m_port);
    }

    /**
     * \~English
     * \brief      Gets values to seek a remote controller in beacon mode.
	 * \details    When an invalid sensor support number is specified, always return 0 direction and distance of -128 (error log is output).
	 * \return     Struct with heading/distance for all (4) channels.
   */  
    ir_seek_t seek() {
        return ev3_infrared_sensor_seek(m_port);
    }

    /**
     * \~English
	 * \brief      Gets commands from IR remote controllers. 
	 * \details    When an invalid sensor support number is specified, always return 0 pattern (error log is output).
	 * \return     Struct with details of the IR remote buttons pressed.     
     */  
    ir_remote_t remote() {
        return ev3_infrared_sensor_get_remote(m_port);
    }
}; // class InfraredSensor


} // namespace ev3cxx

