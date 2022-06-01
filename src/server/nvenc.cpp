
#include "nvenc.h"
#include "NvEncoder/NvEncoderD3D11.h"
#include "NvEncoder/RGBToNV12ConverterD3D11.h"
#include "streaming/streaming.h"
#include "trace_logging.h"

#include <codecvt>
#include <d3d11.h>

using NvEncPackets = std::vector<std::vector<std::byte>>;

NVEnc::NVEnc() = default;
NVEnc::~NVEnc() = default;

bool NVEnc::init(uint32_t width, uint32_t height, IDevice::VideoFormat videoFormat, Ratio fps)
{
    m_width = width;
    m_height = height;
    m_videoFormat = videoFormat;
    m_fps = fps;

    if (width == 0 || height == 0 || videoFormat == IDevice::VideoFormat::Unknown || fps.numerator == 0)
    {
        error("NVENC", "Video dimensions, fps or format is invalid.");
        return false;
    }

    // Create DXGI factory
    {
        ComPtr<IDXGIFactory> factory;
        if (FAILED(CreateDXGIFactory2(0 /*DXGI_CREATE_FACTORY_DEBUG*/, IID_PPV_ARGS(&factory))))
        {
            error("D3D", "Couldn't create DXGI factory!");
            return false;
        }

        if (FAILED(factory.As(&m_dxgiFactory)))
        {
            error("D3D", "Couldn't get IDXGIFactory7, probably old Windows version.");
            return false;
        }
    }

    // Get the default adapter
    {
        ComPtr<IDXGIAdapter> defaultAdapter;
        if (FAILED(m_dxgiFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&defaultAdapter))))
        {
            error("D3D", "Couldn't get default adapter!");
            return false;
        }

        if (FAILED(defaultAdapter.As(&m_dxgiAdapter)))
        {
            error("D3D", "Couldn't get IDXGIAdapter4, probably old Windows version.");
            return false;
        }

        DXGI_ADAPTER_DESC3 desc;
        m_dxgiAdapter->GetDesc3(&desc);
        std::wstring_convert<std::codecvt<char16_t, char, std::mbstate_t>, char16_t> convert;

        info("D3D", "Using adapter '%s'", convert.to_bytes((char16_t*)desc.Description).c_str());

        if (desc.VendorId != 0x10DE)
        {
            warning("D3D", "Adapter is NOT from nVidia, this will probably cause NVEnc failures.");
        }
    }

    {
        D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1};

        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL actualLevel = D3D_FEATURE_LEVEL_10_0;
        UINT flags = true ? D3D11_CREATE_DEVICE_DEBUG : 0;
        flags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        flags |= D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

        if (FAILED(D3D11CreateDevice(m_dxgiAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, featureLevels, 1, D3D11_SDK_VERSION, &device, &actualLevel, &context)))
        {
            error("D3D", "Couldn't create D3D device");
            return false;
        }

        if (FAILED(device.As(&m_device)))
        {
            error("D3D", "Couldn't get ID3D11Device5, probably old Windows version.");
            return false;
        }

        if (FAILED(context.As(&m_deviceContext)))
        {
            error("D3D", "Couldn't get ID3D11DeviceContext4, probably old Windows version.");
            return false;
        }

        if (actualLevel != D3D_FEATURE_LEVEL_11_1)
        {
            error("D3D", "Didn't get expected feature level for the D3D device.");
            return false;
        }

        // Initialize the upload texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = m_width;
        desc.Height = m_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = videoFormat == IDevice::VideoFormat::NV12 ? DXGI_FORMAT_NV12 : DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_device->CreateTexture2D(&desc, NULL, &m_uploadTexture)))
        {
            error("D3D", "Couldn't create encoder upload texture.");
            return false;
        }
    }

    // Initialize NVEnc itself
    {
        NV_ENC_BUFFER_FORMAT nvencFormat = NV_ENC_BUFFER_FORMAT_NV12;

        m_nvencInstance = std::make_unique<NvEncoderD3D11>(m_device.Get(), m_width, m_height, nvencFormat, 0);

        GUID codec = NV_ENC_CODEC_H264_GUID;
        GUID preset = NV_ENC_PRESET_P3_GUID;

        NV_ENC_INITIALIZE_PARAMS initializeParams = {NV_ENC_INITIALIZE_PARAMS_VER};
        NV_ENC_CONFIG encodeConfig = {NV_ENC_CONFIG_VER};
        initializeParams.encodeConfig = &encodeConfig;
        initializeParams.frameRateNum = m_fps.numerator;
        initializeParams.frameRateDen = m_fps.denominator;

        m_nvencInstance->CreateDefaultEncoderParams(&initializeParams, codec, preset, NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY);
        encodeConfig.profileGUID = NV_ENC_H264_PROFILE_BASELINE_GUID;
        encodeConfig.encodeCodecConfig.h264Config.repeatSPSPPS = 1;
        encodeConfig.encodeCodecConfig.h264Config.useBFramesAsRef = NV_ENC_BFRAME_REF_MODE_DISABLED;
        encodeConfig.gopLength = 0;

        m_nvencInstance->CreateEncoder(&initializeParams);

        if (m_videoFormat != IDevice::VideoFormat::NV12)
        {
            m_rgbToNV12Converter = std::make_unique<RGBToNV12ConverterD3D11>(m_device.Get(), m_deviceContext.Get(), m_width, m_height);
        }

        m_nvencInstance->GetSequenceParams(m_sequenceParameters);
    }

    return true;
}

void NVEnc::shutdown()
{
    if (m_nvencInstance)
    {
        NvEncPackets packets;

        try
        {
            m_nvencInstance->EndEncode(packets);
            m_nvencInstance->DestroyEncoder();
        }
        catch (...)
        {
        }

        m_nvencInstance = nullptr;
    }

    m_uploadTexture = nullptr;

    m_rgbToNV12Converter = nullptr;

    if (m_deviceContext)
    {
        m_deviceContext->Flush();
        m_deviceContext->ClearState();
        m_deviceContext = nullptr;
    }

    m_device = nullptr;
}

void NVEnc::onSample(std::chrono::nanoseconds timeStamp, const void* data, uint32_t dataSize, uint64_t frameId, IVideoStreamSampleConsumer* sampleConsumer)
{
    // Get the next input frame,
    // map the D3D texture which is the input and upload the data we got passed in
    const NvEncInputFrame* encoderInputFrame = m_nvencInstance->GetNextInputFrame();

    Trace::Encode_WaitForNextInputFrame(frameId);

    ID3D11Texture2D* encoderInputTexture = static_cast<ID3D11Texture2D*>(encoderInputFrame->inputPtr);

    Trace::Encode_NextInputFrameAvailable(frameId);

    D3D11_MAPPED_SUBRESOURCE map;
    if (FAILED(m_deviceContext->Map(m_uploadTexture.Get(), D3D11CalcSubresource(0, 0, 1), D3D11_MAP_WRITE, 0, &map)))
    {
        error("D3D", "Failed to map the upload texture.");
        return;
    }

    Trace::Encode_UploadTextureMapped(frameId);

    if (m_videoFormat == IDevice::VideoFormat::NV12)
    {
        std::memcpy(map.pData, data, dataSize);
    }
    else if (m_videoFormat == IDevice::VideoFormat::BGRA)
    {
        if (map.RowPitch == 4 * m_width)
        {
            std::memcpy(map.pData, data, dataSize);
        }
        else
        {
            for (uint32_t y = 0; y < m_height; ++y)
            {
                std::memcpy(static_cast<std::byte*>(map.pData) + y * map.RowPitch, static_cast<const std::byte*>(data) + y * m_width * 4, m_width * 4);
            }
        }
    }
    else if (m_videoFormat == IDevice::VideoFormat::RGB24)
    {
        for (uint32_t y = 0; y < m_height; ++y)
        {
            for (uint32_t x = 0; x < m_width; ++x)
            {
                (static_cast<std::byte*>(map.pData) + y * map.RowPitch)[x * 4 + 0] = (static_cast<const std::byte*>(data) + y * m_width * 3)[x * 3 + 0];
                (static_cast<std::byte*>(map.pData) + y * map.RowPitch)[x * 4 + 1] = (static_cast<const std::byte*>(data) + y * m_width * 3)[x * 3 + 1];
                (static_cast<std::byte*>(map.pData) + y * map.RowPitch)[x * 4 + 2] = (static_cast<const std::byte*>(data) + y * m_width * 3)[x * 3 + 2];
            }
        }
    }
    else
    {
        error("D3D", "Unhandled video format!");
        return;
    }

    m_deviceContext->Unmap(m_uploadTexture.Get(), D3D11CalcSubresource(0, 0, 1));

    Trace::Encode_UploadTextureUnmapped(frameId);

    // Trigger a conversion to NV12 if we have a BGRA texture so that the encoder
    // can use the NV12 data
    if (m_videoFormat == IDevice::VideoFormat::NV12)
    {
        m_deviceContext->CopyResource(encoderInputTexture, m_uploadTexture.Get());
    }
    else
    {
        m_rgbToNV12Converter->ConvertRGBToNV12(m_uploadTexture.Get(), encoderInputTexture);
    }

    Trace::Encode_InputFrameTextureUpdated(frameId);

    NvEncPackets packets;

    NV_ENC_PIC_PARAMS picParams = {NV_ENC_PIC_PARAMS_VER};
    // picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEINTRA;

    m_nvencInstance->EncodeFrame(packets, &picParams /* TODO: Force I-Frame only when a new connection came in etc. */);

    Trace::Encode_EncodeFrameFinished(frameId, packets.size() > 0 ? packets[0].size() : 0);

    // info("NVENC", "Encoded frame: %d packets, packet 0 size: %d", packets.size(), packets.size() > 0 ? packets[0].size() : 0);

    // TODO: Handle multi-packet frames (if they ever crop up)
    if (packets.size() == 1)
    {
        if (m_firstFrame.empty())
        {
            m_firstFrame.resize(packets[0].size());
            std::memcpy(m_firstFrame.data(), packets[0].data(), packets[0].size());
        }

        sampleConsumer->onEncodedSampleAvailable(timeStamp, packets[0], frameId, m_firstFrame);
    }

    /*
    static FILE* fp = fopen("E:\\raw.h264", "wb");

    if (packets.size() > 0)
    {
        fwrite(packets[0].data(), sizeof(std::byte), packets[0].size(), fp);
        fflush(fp);
    }*/
}
