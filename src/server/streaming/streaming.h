
#pragma once

class IVideoStreamSampleConsumer
{
  public:
    virtual ~IVideoStreamSampleConsumer() = default;

    virtual void onEncodedSampleAvailable(std::chrono::nanoseconds originalTimeStamp, const std::vector<std::byte>& sample, uint64_t frameId, const std::vector<std::byte>& sequenceParameters) = 0;
};
