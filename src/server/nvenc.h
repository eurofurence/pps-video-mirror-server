
#pragma once

#include "video_input/device.h"

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct ID3D11Texture2D;
struct IDXGIFactory7;
struct IDXGIAdapter4;

class NvEncoderD3D11;
class RGBToNV12ConverterD3D11;

class IVideoStreamSampleConsumer;

class NVEnc : public IDeviceSampleHandler
{
  public:
    NVEnc();
    ~NVEnc();

    bool init(uint32_t width, uint32_t height, IDevice::VideoFormat videoFormat, Ratio fps);
    void shutdown();

    virtual void onSample(std::chrono::nanoseconds timeStamp, const void* data, uint32_t dataSize, uint64_t frameId, IVideoStreamSampleConsumer* sampleConsumer) override;

  private:
    ComPtr<IDXGIFactory7> m_dxgiFactory;
    ComPtr<IDXGIAdapter4> m_dxgiAdapter;

    ComPtr<ID3D11Device5> m_device;
    ComPtr<ID3D11DeviceContext4> m_deviceContext;
    ComPtr<ID3D11Texture2D> m_uploadTexture;

    std::unique_ptr<NvEncoderD3D11> m_nvencInstance;
    std::unique_ptr<RGBToNV12ConverterD3D11> m_rgbToNV12Converter;

    std::vector<std::byte> m_sequenceParameters;
    std::vector<std::byte> m_firstFrame;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    IDevice::VideoFormat m_videoFormat = IDevice::VideoFormat::Unknown;

    Ratio m_fps;
};
