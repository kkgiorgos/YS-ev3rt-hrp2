/**
 * \file    ev3cxx_color_sensor.h
 * \brief	EV3RT CPP API for color sensor
 * \author	Jaroslav Páral (jarekparal)
 */

#pragma once

#include <stdio.h>

#include "ev3api.h"
#include "ev3cxx_sensor.h"

namespace ev3cxx {
    
/**
 * \~English
 * \brief    Class ColorSensor. API for working with color sensor.
 */
class ColorSensor : public Sensor
{
public:
	struct ColorRaw_t { int r; int g; int b; };       

    /**
     * \~English
     * \brief       Constructor of class ColorSensor. 
     * \param port  Port index (1-4).      
     */  
    ColorSensor(SensorPort port) : Sensor(port, COLOR_SENSOR) {
        resetCalibration();
    }

    /**
     * \~English
     * \brief           Get reflected light intensity. Red LED on. 
     * \details         It is blocking function => If the sensor is not connected then the program freezes.
     * \param calibrate Get calibrated value. FIRSTLY call calibrateReflection().
     * \param calClamp  Clamp output value to min (0) a max (100).
     * \return          Range: 0 - 100   
     */  
    int reflected(bool_t calibrate = false, bool_t calClamp = false) {
        if(!calibrate) 
            return ev3_color_sensor_get_reflect(m_port);
        else {
            int result = ((ev3_color_sensor_get_reflect(m_port) - m_min) 
                        * (float(100) / (m_max - m_min))); 
            if(calClamp) {
                if(m_min > result)
                    return 0;
                if(m_max < result)
                    return 100;
            }
            return result;
        }
    }

    int reflectedRaw()
    {
        return reflected();//ev3_color_sensor_get_raw_reflect(m_port);
    }

    /**
     * \~English
     * \brief       Get raw reflected light intensity (red, green, blue). Red LED on.
     * \return      Return rgb_raw_t (r, g, b)    
     */  
	rgb_raw_t reflectedRawRgb() {
		rgb_raw_t color;
		ev3_color_sensor_get_rgb_raw(m_port, &color);
		return color;
	}

    /**
     * \~English
     * \brief       Get ambient light intensity. Red LEDs off. 
     * \return      Range: 0 - 100     
     */  
	int ambient() {
        return ev3_color_sensor_get_ambient(m_port);
    }

    /**
     * \~English
     * \brief       Get color. 
     * \return      Return colorid_t (COLOR_NONE, COLOR_BLACK, COLOR_BLUE).     
     */  
	colorid_t color() {
        return ev3_color_sensor_get_color(m_port);
    }

    /**
     * \~English
     * \brief       Set the calibration values (min, max)
     * \details     Call this function in cycle and rotate sensor on line.
     */  
    void calibrateReflection(){
        int val = reflected();
        if(m_max < val)
            m_max = val;
        if(m_min > val)
            m_min = val;
    }

    /**
     * \~English
     * \brief       Reset calibration value 
     * \details     Set max to \a 0 and min to \a 100.    
     */  
    void resetCalibration() {
        m_max = 0;
        m_min = 100;
    }

    /**
     * \~English
     * \brief       Return \a min value of reflected calibration 
     */
    int min() {
        return m_min;
    }

    /**
     * \~English
     * \brief       Return \a max value of reflected calibration 
     */
    int max() {
        return m_max;
    }

private:
    int m_max, m_min;
}; // class ColorSensor

} // namespace ev3cxx
