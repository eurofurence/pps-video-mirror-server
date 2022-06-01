
#pragma once

#include "device.h"

class MediaFoundationVideoInput
{
public:

  MediaFoundationVideoInput();
  ~MediaFoundationVideoInput();

  std::vector<std::string> enumerateDevices();

  std::shared_ptr<IDevice> instantiateDevice(const std::string& name);

private:

};
