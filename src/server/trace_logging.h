
#pragma once

namespace Trace
{
// Trace events for the capture portion of the frame (i.e. using media foundation to grab frames)
void Capture_WaitForNextSample(uint64_t frameId);
void Capture_SampleReady(uint64_t frameId, std::chrono::nanoseconds timeStamp);
void Capture_SampleFailed(uint64_t frameId);

// Trace events for the video encode portion of the frame (i.e. using nvEnc)
void Encode_WaitForNextInputFrame(uint64_t frameId);
void Encode_NextInputFrameAvailable(uint64_t frameId);
void Encode_UploadTextureMapped(uint64_t frameId);
void Encode_UploadTextureUnmapped(uint64_t frameId);
void Encode_InputFrameTextureUpdated(uint64_t frameId);
void Encode_EncodeFrameFinished(uint64_t frameId, uint64_t packetSize);

// Trace events for the WebRTC portion
void WebRTC_ConnectionOffer();
void WebRTC_ConnectionAnswer();
void WebRTC_VideoSampleEnqueue(uint64_t frameId);
void WebRTC_VideoSamplePickup(uint64_t frameId);

} // namespace Trace
