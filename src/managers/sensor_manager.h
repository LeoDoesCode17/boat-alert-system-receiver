#pragma once

namespace sensor_manager
{
    void initialize();
    long getEncoderPosition();
    void setEncoderPosition(long position);
    bool wasEncoderButtonPressed();
} // namespace sensor_manager
