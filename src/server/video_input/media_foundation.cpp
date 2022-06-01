
#include "media_foundation.h"
#include "trace_logging.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

namespace
{
enum class DeviceIteration
{
    Continue,
    Stop
};

bool EnumerateDevices(std::function<DeviceIteration(ComPtr<IMFActivate> device)> deviceCallback)
{
    UINT32 count = 0;

    ComPtr<IMFAttributes> pConfig;
    IMFActivate** ppDevices = NULL;

    // Create an attribute store to hold the search criteria.
    HRESULT hr = MFCreateAttributes(&pConfig, 1);

    // Request video capture devices.
    if (SUCCEEDED(hr))
    {
        hr = pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    }

    // Enumerate the devices,
    if (SUCCEEDED(hr))
    {
        hr = MFEnumDeviceSources(pConfig.Get(), &ppDevices, &count);
    }

    if (SUCCEEDED(hr) && count > 0)
    {
        for (DWORD i = 0; i < count; i++)
        {
            deviceCallback(ppDevices[i]);

            ppDevices[i]->Release();
        }

        CoTaskMemFree(ppDevices);

        return true;
    }

    return false;
}

std::string ConvertWCSToMBS(const wchar_t* pstr, long wslen)
{
    int len = ::WideCharToMultiByte(CP_ACP, 0, pstr, wslen, NULL, 0, NULL, NULL);

    std::string dblstr(len, '\0');
    len = ::WideCharToMultiByte(CP_ACP, 0 /* no flags */, pstr, wslen /* not necessary NULL-terminated */, &dblstr[0], len, NULL, NULL /* no default char */);

    return dblstr;
}

std::string GetDeviceName(IMFActivate* device)
{
    WCHAR* szFriendlyName = NULL;

    // Try to get the display name.
    UINT32 cchName;
    if (SUCCEEDED(device->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &szFriendlyName, &cchName)))
    {
        std::string retVal;

        retVal = ConvertWCSToMBS(szFriendlyName, cchName);

        CoTaskMemFree(szFriendlyName);

        return retVal;
    }

    return {};
}
} // namespace

MediaFoundationVideoInput::MediaFoundationVideoInput()
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
}

MediaFoundationVideoInput::~MediaFoundationVideoInput()
{
    MFShutdown();
    CoUninitialize();
}

std::vector<std::string> MediaFoundationVideoInput::enumerateDevices()
{
    std::vector<std::string> retVal;

    ::EnumerateDevices(
        [&retVal](auto device) -> ::DeviceIteration
        {
            retVal.push_back(::GetDeviceName(device.Get()));

            return ::DeviceIteration::Continue;
        });

    return retVal;
}

class Device : public IDevice
{
  public:
    Device(ComPtr<IMFMediaSource> device, const std::string& name) : m_device(device), m_name(name)
    {
    }

    ~Device()
    {
        m_sourceReader = nullptr;

        m_device->Shutdown();
        m_device = nullptr;
    }

    virtual std::string getName() const override
    {
        return m_name;
    }

    bool initSourceReader()
    {
        if (FAILED(MFCreateSourceReaderFromMediaSource(m_device.Get(), nullptr, &m_sourceReader)))
        {
            error("MF", "Failed to create source reader for media source.");
            return false;
        }

        // Build a new media type which uses NV12 as the format (YUV 4:2:0) so that we can natively pass
        // the buffers to nvenc
        ComPtr<IMFMediaType> newMediaType;
        MFCreateMediaType(&newMediaType);

        newMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);

        switch (m_videoFormat)
        {
        case VideoFormat::BGRA:
            newMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
            break;
        case VideoFormat::NV12:
            newMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
            break;
        default:
            error("MF", "Can't set 'Unknown' as the video format.");
            return false;
        }

        // Try to set the media type
        if (FAILED(m_sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, newMediaType.Get())))
        {
            warning("MF", "Failed to set media type with NV12 as the format. Will need to perform a conversion to NV12 on a per frame basis.");
        }

        // Get the media type after we made the change.
        ComPtr<IMFMediaType> mediaType;
        if (FAILED(m_sourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &mediaType)))
        {
            error("MF", "Failed to get media type.");
            return false;
        }

        {
            UINT64 frameSize = 0;
            mediaType->GetUINT64(MF_MT_FRAME_SIZE, &frameSize);

            m_videoWidth = HI32(frameSize);
            m_videoHeight = LO32(frameSize);
        }

        {
            UINT64 frameRate = 0;
            mediaType->GetUINT64(MF_MT_FRAME_RATE, &frameRate);

            m_fps.numerator = HI32(frameRate);
            m_fps.denominator = LO32(frameRate);
        }

        {
            GUID format;
            mediaType->GetGUID(MF_MT_SUBTYPE, &format);

            if (format == MFVideoFormat_ARGB32)
            {
                m_videoFormat = VideoFormat::BGRA;
            }
            else if (format == MFVideoFormat_NV12)
            {
                m_videoFormat = VideoFormat::NV12;
            }
            else if (format == MFVideoFormat_RGB24)
            {
                m_videoFormat = VideoFormat::RGB24;
            }
            else
            {
                m_videoFormat = VideoFormat::Unknown;
            }
        }

        info("MF", "Video format %d x %d @ %.2f FPS, format: %d", m_videoWidth, m_videoHeight, m_fps.asFloat(), (int)m_videoFormat);

        return true;
    }

    virtual void stream(std::atomic<bool>& run, IDeviceSampleHandler* sampleHandler, IVideoStreamSampleConsumer* sampleConsumer) override
    {
        m_frameId = 0;

        while (run)
        {
            Trace::Capture_WaitForNextSample(m_frameId);

            ComPtr<IMFSample> sample;

            DWORD streamIndex = 0, streamFlags = 0;
            LONGLONG timeStamp = 0;
            if (SUCCEEDED(m_sourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &streamFlags, &timeStamp, &sample)))
            {
                std::chrono::nanoseconds timeStampNs(timeStamp * 100);

                Trace::Capture_SampleReady(m_frameId, timeStampNs);

                DWORD bufferCount = 0, bufferLength = 0;
                if (sample)
                {
                    sample->GetBufferCount(&bufferCount);

                    ComPtr<IMFMediaBuffer> buffer;

                    if (bufferCount == 1)
                    {
                        sample->GetBufferByIndex(0, &buffer);
                    }
                    else if (bufferCount > 1)
                    {
                        sample->ConvertToContiguousBuffer(&buffer);
                    }

                    if (buffer && sampleHandler)
                    {
                        BYTE* data = nullptr;
                        DWORD maxLength = 0, currentLength = 0;
                        if (SUCCEEDED(buffer->Lock(&data, &maxLength, &currentLength)))
                        {
                            sampleHandler->onSample(timeStampNs, data, currentLength, m_frameId, sampleConsumer);

                            buffer->Unlock();
                        }
                    }
                }
            }
            else
            {
                error("MF", "ReadSample() failed.");
                Trace::Capture_SampleFailed(m_frameId);
            }

            m_frameId++;
        }
    }

    virtual void getFrameSize(uint32_t& width, uint32_t& height) const override
    {
        width = m_videoWidth;
        height = m_videoHeight;
    }

    virtual Ratio getFrameRate() const override
    {
        return m_fps;
    }

    virtual VideoFormat getVideoFormat() const override
    {
        return m_videoFormat;
    }

  private:
    ComPtr<IMFMediaSource> m_device;
    ComPtr<IMFSourceReader> m_sourceReader;
    std::string m_name;

    uint32_t m_videoWidth = 0;
    uint32_t m_videoHeight = 0;

    Ratio m_fps;

    uint64_t m_frameId = 0;

    VideoFormat m_videoFormat = VideoFormat::NV12;
};

std::shared_ptr<IDevice> MediaFoundationVideoInput::instantiateDevice(const std::string& name)
{
    info("MF", "Trying to instantiate device '%s'", name.c_str());

    ComPtr<IMFActivate> deviceInstance;

    ::EnumerateDevices(
        [name, &deviceInstance](auto device) -> ::DeviceIteration
        {
            if (::GetDeviceName(device.Get()) == name)
            {
                deviceInstance = device;
                return ::DeviceIteration::Stop;
            }

            return ::DeviceIteration::Continue;
        });

    if (!deviceInstance)
    {
        error("MF", "Didn't find device '%s', can't instantiate it.", name.c_str());
        return nullptr;
    }

    ComPtr<IMFMediaSource> mediaSource;
    if (FAILED(deviceInstance->ActivateObject(IID_PPV_ARGS(&mediaSource))))
    {
        error("MF", "Activating object as a IMFMediaSource failed.");
        return nullptr;
    }

    auto retVal = std::make_shared<Device>(mediaSource, name);

    if (!retVal->initSourceReader())
    {
        return nullptr;
    }

    return retVal;
}
