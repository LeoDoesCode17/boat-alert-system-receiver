#include "sensor_manager.h"
#include "sensors/rotary_encoder.h"
#include "config/pin.h"

namespace sensor_manager
{
    void initialize()
    {
        rotary_encoder::initialize(pin::ROTARY_ENCODER_DT, pin::ROTARY_ENCODER_CLK, pin::ROTARY_ENCODER_SW);
    }
    long getEncoderPosition()
    {
        return rotary_encoder::getPosition();
    }

    void setEncoderPosition(long position)
    {
        rotary_encoder::setPosition(position);
    }

    bool wasEncoderButtonPressed()
    {
        return rotary_encoder::wasButtonPressed();
    }
} // namespace sensor_manager
