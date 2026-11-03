#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include "Params.hpp"
#include "interfaces/CommonStructs.hpp"

namespace simple_flight
{

class Mixer
{
public:
    Mixer(const Params* params)
        : params_(params)
    {
    }

    void getMotorOutput(const Axis4r& controls, std::vector<float>& motor_outputs) const
    {
        if (controls.throttle() < params_->motor.min_angling_throttle) {
            motor_outputs.assign(params_->motor.motor_count, controls.throttle());
            return;
        }

        uint16_t motor_count = params_->motor.motor_count;
        const motorMixer_t* mixer_table = nullptr;
        if (motor_count == 2) {
            //coaxial (stacked) layout: motor 0 is the upper rotor turning CCW,
            //motor 1 is the lower rotor turning CW; yaw comes from differential RPM.
            //roll/pitch must come from a separate actuation mechanism on the vehicle
            mixer_table = mixerCoax;
        }
        else if (motor_count == 4) {
            mixer_table = mixerQuadX;
        }
        else {
            throw std::invalid_argument("Mixer: unsupported motor count " + std::to_string(motor_count));
        }

        for (uint16_t motor_index = 0; motor_index < motor_count; ++motor_index) {
            motor_outputs[motor_index] =
                controls.throttle() * mixer_table[motor_index].throttle + controls.pitch() * mixer_table[motor_index].pitch + controls.roll() * mixer_table[motor_index].roll + controls.yaw() * mixer_table[motor_index].yaw;
        }

        float min_motor = *std::min_element(motor_outputs.begin(), motor_outputs.begin() + motor_count);
        if (min_motor < params_->motor.min_motor_output) {
            float undershoot = params_->motor.min_motor_output - min_motor;
            for (uint16_t motor_index = 0; motor_index < motor_count; ++motor_index)
                motor_outputs[motor_index] += undershoot;
        }

        float max_motor = *std::max_element(motor_outputs.begin(), motor_outputs.begin() + motor_count);
        float scale = max_motor / params_->motor.max_motor_output;
        if (scale > params_->motor.max_motor_output) {
            for (uint16_t motor_index = 0; motor_index < motor_count; ++motor_index)
                motor_outputs[motor_index] /= scale;
        }

        for (uint16_t motor_index = 0; motor_index < motor_count; ++motor_index)
            motor_outputs[motor_index] = std::max(params_->motor.min_motor_output,
                                                  std::min(motor_outputs[motor_index], params_->motor.max_motor_output));
    }

private:
    const Params* params_;

    // Custom mixer data per motor
    typedef struct motorMixer_t
    {
        float throttle;
        float roll;
        float pitch;
        float yaw;
    } motorMixer_t;

    //only thing that this matrix does is change the sign
    const motorMixer_t mixerQuadX[4] = {
        //QuadX config
        { 1.0f, -1.0f, 1.0f, 1.0f }, // FRONT_R
        { 1.0f, 1.0f, -1.0f, 1.0f }, // REAR_L
        { 1.0f, 1.0f, 1.0f, -1.0f }, // FRONT_L
        { 1.0f, -1.0f, -1.0f, -1.0f }, // REAR_R
    };

    //coaxial config: both motors carry full throttle, yaw by differential RPM.
    //yaw signs follow the QuadX convention (CCW rotor -> +yaw, CW rotor -> -yaw),
    //matching initializeRotorCoax() where rotor 0 is CCW and rotor 1 is CW
    const motorMixer_t mixerCoax[2] = {
        { 1.0f, 0.0f, 0.0f, 1.0f },  // TOP (CCW)
        { 1.0f, 0.0f, 0.0f, -1.0f }, // BOTTOM (CW)
    };
};

} //namespace