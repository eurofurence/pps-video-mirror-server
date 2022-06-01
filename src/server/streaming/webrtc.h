
#pragma once

#include "streaming.h"

class SignalingWebServer;
class WebRTCConnection;

class WebRTCServer : public IVideoStreamSampleConsumer
{
  public:
    WebRTCServer();
    ~WebRTCServer();

    bool init(Ratio frameRate);
    void shutdown();

    virtual void onEncodedSampleAvailable(std::chrono::nanoseconds originalTimeStamp, const std::vector<std::byte>& sample, uint64_t frameId,
                                          const std::vector<std::byte>& sequenceParameters) override;

  private:
    friend SignalingWebServer;

    WebRTCConnection* createConnectionInstance();
    WebRTCConnection* getConnectionByIndex(uint64_t index) const;

    void broadCastJSON(const std::string& json);
    void broadCastVideoSample(std::chrono::nanoseconds originalTimeStamp, const std::vector<std::byte>& sample, const std::vector<std::byte>& sequenceParameters);

    void tick();

    std::unique_ptr<SignalingWebServer> m_signalingWebServer;

    mutable std::mutex m_connectionMutex;
    std::unordered_map<uint64_t, std::unique_ptr<WebRTCConnection>> m_connections;
    std::uint64_t m_nextConnectionIndex = 0;

    std::chrono::nanoseconds m_lastSentSampleTimeStamp;

    Ratio m_frameRate;
};
