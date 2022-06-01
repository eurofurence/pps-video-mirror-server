
#include "trace_logging.h"

#include <TraceLoggingProvider.h>
#include <winmeta.h>

// {57C88543-4F97-48BB-9206-3FE1A3BE2220}
TRACELOGGING_DEFINE_PROVIDER(g_hTLProvider, "Eurofurence.PPS.VideoMirrorServer", (0x57c88543, 0x4f97, 0x48bb, 0x92, 0x6, 0x3f, 0xe1, 0xa3, 0xbe, 0x22, 0x20));

class ProviderRegistration
{
  public:
    ProviderRegistration()
    {
        TraceLoggingRegister(g_hTLProvider);
    }

    ~ProviderRegistration()
    {
        TraceLoggingUnregister(g_hTLProvider);
    }
};

static ProviderRegistration s_TLProviderRegistration;

namespace Trace
{

void Capture_WaitForNextSample(uint64_t frameId)
{
    TraceLoggingWrite(g_hTLProvider, "Capture_WaitForNextSample", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingUInt64(frameId, "FrameId"));
}

void Capture_SampleReady(uint64_t frameId, std::chrono::nanoseconds timeStamp)
{
    TraceLoggingWrite(g_hTLProvider, "Capture_SampleReady", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingUInt64(frameId, "FrameId"), TraceLoggingUInt64(timeStamp.count(), "TimeStampNS"));
}

void Capture_SampleFailed(uint64_t frameId)
{
    TraceLoggingWrite(g_hTLProvider, "Capture_SampleFailed", TraceLoggingLevel(WINEVENT_LEVEL_ERROR), TraceLoggingUInt64(frameId, "FrameId"));
}

void Encode_WaitForNextInputFrame(uint64_t frameId)
{
    TraceLoggingWrite(g_hTLProvider, "Encode_WaitForNextInputFrame", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingUInt64(frameId, "FrameId"));
}

void Encode_NextInputFrameAvailable(uint64_t frameId)
{
    TraceLoggingWrite(g_hTLProvider, "Encode_NextInputFrameAvailable", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingUInt64(frameId, "FrameId"));
}

void Encode_UploadTextureMapped(uint64_t frameId)
{
    TraceLoggingWrite(g_hTLProvider, "Encode_UploadTextureMapped", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingUInt64(frameId, "FrameId"));
}

void Encode_UploadTextureUnmapped(uint64_t frameId)
{
    TraceLoggingWrite(g_hTLProvider, "Encode_UploadTextureUnmapped", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingUInt64(frameId, "FrameId"));
}

void Encode_InputFrameTextureUpdated(uint64_t frameId)
{
    TraceLoggingWrite(g_hTLProvider, "Encode_InputFrameTextureUpdated", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingUInt64(frameId, "FrameId"));
}

void Encode_EncodeFrameFinished(uint64_t frameId, uint64_t packetSize)
{
    TraceLoggingWrite(g_hTLProvider, "Encode_EncodeFrameFinished", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingUInt64(frameId, "FrameId"),
                      TraceLoggingUInt32(static_cast<uint32_t>(packetSize), "PacketSize"));
}

void WebRTC_ConnectionOffer()
{
    TraceLoggingWrite(g_hTLProvider, "WebRTC_ConnectionOffer", TraceLoggingLevel(WINEVENT_LEVEL_INFO));
}

void WebRTC_ConnectionAnswer()
{
    TraceLoggingWrite(g_hTLProvider, "WebRTC_ConnectionAnswer", TraceLoggingLevel(WINEVENT_LEVEL_INFO));
}

void WebRTC_VideoSampleEnqueue(uint64_t frameId)
{
    TraceLoggingWrite(g_hTLProvider, "WebRTC_VideoSampleEnqueue", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingUInt64(frameId, "FrameId"));
}

void WebRTC_VideoSamplePickup(uint64_t frameId)
{
    TraceLoggingWrite(g_hTLProvider, "WebRTC_VideoSamplePickup", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingUInt64(frameId, "FrameId"));
}

} // namespace Trace
