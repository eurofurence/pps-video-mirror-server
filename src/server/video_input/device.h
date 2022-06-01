
#pragma once

class IVideoStreamSampleConsumer;

class IDeviceSampleHandler
{
  public:
    virtual ~IDeviceSampleHandler() = default;

    virtual void onSample(std::chrono::nanoseconds timeStamp, const void* data, uint32_t dataSize, uint64_t frameId, IVideoStreamSampleConsumer* sampleConsumer) = 0;
};

class IDevice
{
  public:
    enum class VideoFormat
    {
        Unknown,
        BGRA,
        NV12,
        RGB24 // Used by some Web cams. Not recommended as uploading it is more involved than other formats.
    };

    virtual ~IDevice() = default;

    virtual std::string getName() const = 0;

    virtual bool prepareStreaming()
    {
        return true;
    }

    virtual void stream(std::atomic<bool>& run, IDeviceSampleHandler* sampleHandler, IVideoStreamSampleConsumer* sampleConsumer) = 0;

    virtual void getFrameSize(uint32_t& width, uint32_t& height) const = 0;
    virtual Ratio getFrameRate() const = 0;

    virtual VideoFormat getVideoFormat() const = 0;
};
