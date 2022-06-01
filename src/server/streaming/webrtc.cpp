

#include "webrtc.h"
#include "nlohmann/json.hpp"
#include "trace_logging.h"

#include <CivetServer.h>

#include <rtc/rtc.hpp>

#include <ctime>

namespace
{
// taken from https://stackoverflow.com/questions/10905892/equivalent-of-gettimeday-for-windows

struct timezone
{
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval* tv, struct timezone* tz)
{
    if (tv)
    {
        FILETIME filetime; /* 64-bit value representing the number of 100-nanosecond intervals since
                              January 1, 1601 00:00 UTC */
        ULARGE_INTEGER x;
        ULONGLONG usec;
        static const ULONGLONG epoch_offset_us = 11644473600000000ULL; /* microseconds betweeen Jan 1,1601 and Jan 1,1970 */

        GetSystemTimePreciseAsFileTime(&filetime);
        x.LowPart = filetime.dwLowDateTime;
        x.HighPart = filetime.dwHighDateTime;
        usec = x.QuadPart / 10 - epoch_offset_us;
        tv->tv_sec = time_t(usec / 1000000ULL);
        tv->tv_usec = long(usec % 1000000ULL);
    }
    if (tz)
    {
        TIME_ZONE_INFORMATION timezone;
        GetTimeZoneInformation(&timezone);
        tz->tz_minuteswest = timezone.Bias;
        tz->tz_dsttime = 0;
    }
    return 0;
}

std::chrono::microseconds currentTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return std::chrono::microseconds(uint64_t(time.tv_sec) * 1000 * 1000 + time.tv_usec);
}
} // namespace

using nlohmann::json;

// The connection class encapsulates the state for one streaming end point connected
// via the WebRTC protocol (so one client)
class WebRTCConnection
{
  public:
    enum class State : uint8_t
    {
        WaitingForConnection = 0,
        Connected,
        Disconnected
    };

    WebRTCConnection(uint64_t index, std::chrono::nanoseconds startTimeStamp, double frameTime) : m_index(index), m_startTimeStamp(startTimeStamp), m_frameTime(frameTime)
    {
        rtc::Configuration config = {};
        config.portRangeBegin = 40000;
        config.portRangeEnd = 50000;

        m_peerConnection = std::make_shared<rtc::PeerConnection>(config);
        m_peerConnection->onStateChange(
            [this](rtc::PeerConnection::State newState)
            {
                info("WebRTC", "Connection (%d) new state: %d", m_index, (int)newState);

                switch (newState)
                {
                case rtc::PeerConnection::State::Connected:
                    setState(State::Connected);
                    break;

                case rtc::PeerConnection::State::Closed:
                case rtc::PeerConnection::State::Disconnected:
                case rtc::PeerConnection::State::Failed:
                    setState(State::Disconnected);
                    break;
                }
            });

        m_peerConnection->onGatheringStateChange(
            [this](rtc::PeerConnection::GatheringState newState)
            {
                info("WebRTC", "Connection (%d) new gathering state: %d", m_index, (int)newState);

                if (newState == rtc::PeerConnection::GatheringState::Complete)
                {
                    auto localDesc = m_peerConnection->localDescription();

                    json offer = {{"type", localDesc->typeString()}, {"sdp", std::string(localDesc.value())}, {"index", m_index}};

                    m_offer = offer.dump();
                    m_hasOfferAvailable.store(true);
                }
            });

        // Set up the video track, this is basically the code from the streaming sample of libdatachannel
        // in a very condensed and simplified form ;)
        {
            const uint8_t payloadType = 102;
            const uint32_t ssrc = 1;
            const std::string cname = "video-stream";
            const std::string msid = "stream1";

            auto video = rtc::Description::Video(cname, rtc::Description::Direction::SendOnly);
            video.addH264Codec(payloadType);
            video.addSSRC(1, cname, msid);

            m_videoTrack = m_peerConnection->addTrack(video);

            m_videoTrack->onOpen(
                [this]()
                {
                    info("WebRTC", "Connection (%d) video channel opened.", m_index);

                    m_videoSrReporter->rtpConfig->setStartTime(static_cast<double>(currentTime().count()) / (1000 * 1000), rtc::RtpPacketizationConfig::EpochStart::T1970);
                    m_videoSrReporter->startRecording();

                    m_videoTrackAvailable = true;
                });

            auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, cname, payloadType, rtc::H264RtpPacketizer::defaultClockRate);
            auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(rtc::H264RtpPacketizer::Separator::LongStartSequence, rtpConfig);
            auto h264handler = std::make_shared<rtc::H264PacketizationHandler>(packetizer);

            m_videoSrReporter = std::make_shared<rtc::RtcpSrReporter>(rtpConfig);
            h264handler->addToChain(m_videoSrReporter);

            auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
            h264handler->addToChain(nackResponder);

            m_videoTrack->setMediaHandler(h264handler);
        }

        // Set up the data channel
        m_dataChannel = m_peerConnection->createDataChannel("broadcast");
        m_dataChannel->onOpen(
            [this]()
            {
                info("WebRTC", "Connection (%d) data channel opened.", m_index);

                m_dataChannelAvailable = true;
            });

        m_peerConnection->setLocalDescription();
    }

    ~WebRTCConnection()
    {
        m_videoTrackAvailable = false;
        m_dataChannelAvailable = false;

        if (m_videoTrack)
        {
            m_videoTrack->close();
            m_videoTrack = nullptr;
        }

        m_videoSrReporter = nullptr;

        if (m_dataChannel)
        {
            m_dataChannel->close();
            m_dataChannel = nullptr;
        }

        if (m_peerConnection)
        {
            m_peerConnection->close();
            m_peerConnection = nullptr;
        }
    }

    uint64_t getIndex() const
    {
        return m_index;
    }

    State getState() const
    {
        return m_state;
    }

    void setState(State newState)
    {
        m_state = newState;
    }

    bool hasOfferAvailable() const
    {
        return m_hasOfferAvailable;
    }

    const std::string& getOffer() const
    {
        return m_offer;
    }

    void setAnswer(const std::string& answer)
    {
        auto json = json::parse(answer);
        rtc::Description answerDesc(json["sdp"].get<std::string>(), json["type"].get<std::string>());
        m_peerConnection->setRemoteDescription(answerDesc);
    }

    void sendStringOnDataChannel(const std::string& str)
    {
        if (!m_dataChannelAvailable)
            return;

        m_dataChannel->send(str);
    }

    void sendVideoSample(std::chrono::nanoseconds timeStamp, const std::vector<std::byte>& sample, const std::vector<std::byte>& sequenceParameters)
    {
        if (!m_videoTrackAvailable)
            return;

        // First update the time stamps
        auto elapsedSeconds = m_frameCount * m_frameTime;
        auto rtpConfig = m_videoSrReporter->rtpConfig;
        uint32_t elapsedTimeStamp = rtpConfig->secondsToTimestamp(elapsedSeconds);

        rtpConfig->timestamp = rtpConfig->startTimestamp + elapsedTimeStamp;

        auto reportedElapsedTimeStamp = rtpConfig->timestamp - m_videoSrReporter->previousReportedTimestamp;

        if (rtpConfig->timestampToSeconds(reportedElapsedTimeStamp) > 1)
        {
            m_videoSrReporter->setNeedsToReport();
        }

        // If this is the first frame send the sequence parameters first
        if (m_frameCount == 0)
        {
            m_videoTrack->send(sequenceParameters.data(), sequenceParameters.size());
        }

        // Send the actual sample
        m_videoTrack->send(sample.data(), sample.size());

        m_frameCount++;
    }

  private:
    uint64_t m_index = 0;
    std::chrono::nanoseconds m_startTimeStamp;
    std::atomic<State> m_state = State::WaitingForConnection;
    std::atomic<bool> m_hasOfferAvailable = false;
    std::atomic<bool> m_dataChannelAvailable = false;
    std::atomic<bool> m_videoTrackAvailable = false;
    std::string m_offer = "{}";

    std::shared_ptr<rtc::PeerConnection> m_peerConnection;
    std::shared_ptr<rtc::DataChannel> m_dataChannel;
    std::shared_ptr<rtc::Track> m_videoTrack;
    std::shared_ptr<rtc::RtcpSrReporter> m_videoSrReporter;

    uint64_t m_frameCount = 0;
    double m_frameTime = 0;
};

// The signaling web server is used to handle the offer and response
// portion of the WebRTC protocol to avoid the need of a full signaling server setup
class SignalingWebServer
{
  public:
    class OfferHandler : public CivetHandler
    {
      public:
        OfferHandler(SignalingWebServer& server) : m_server(server)
        {
        }

        virtual bool handleGet([[maybe_unused]] CivetServer* server, struct mg_connection* connection, int* status_code) override
        {
            char authToken[128] = {0};
            if (mg_get_request_info(connection)->query_string)
            {
                mg_get_var2(mg_get_request_info(connection)->query_string, std::strlen(mg_get_request_info(connection)->query_string), "authToken", authToken, sizeof(authToken), 0);
            }

            // We make a simple auth against a hard coded token, just to avoid spawning connections which shouldn't be there
            if (std::strcmp(authToken, "PPSVideoMirror"))
            {
                *status_code = 403;
                mg_printf(connection, "HTTP/1.1 403 OK\r\nContent-Type: text/json\r\nConnection: close\r\n\r\n");
                mg_printf(connection, "{}");

                return true;
            }

            // Create a new WebRTC streaming connection
            auto streamingConnection = m_server.m_webRtcServer.createConnectionInstance();

            // Wait until the WebRTC stack has set up the offer that the browser needs
            // on the remote side.
            while (!streamingConnection->hasOfferAvailable())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            *status_code = 200;
            mg_printf(connection, "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nConnection: close\r\n\r\n");
            mg_write(connection, streamingConnection->getOffer().c_str(), streamingConnection->getOffer().size());

            return true;
        }

      private:
        SignalingWebServer& m_server;
    };

    class AnswerHandler : public CivetHandler
    {
      public:
        AnswerHandler(SignalingWebServer& server) : m_server(server)
        {
        }

        virtual bool handlePost([[maybe_unused]] CivetServer* server, struct mg_connection* connection, int* status_code) override
        {
            char connectionIndex[128] = {0};
            if (mg_get_request_info(connection)->query_string)
            {
                mg_get_var2(mg_get_request_info(connection)->query_string, std::strlen(mg_get_request_info(connection)->query_string), "connectionIndex", connectionIndex, sizeof(connectionIndex), 0);
            }

            // We make a simple auth against a hard coded token, just to avoid spawning connections which shouldn't be there
            if (std::strlen(connectionIndex) == 0)
            {
                *status_code = 403;
                mg_printf(connection, "HTTP/1.1 403 OK\r\nContent-Type: text/json\r\nConnection: close\r\n\r\n");
                mg_printf(connection, "{}");

                return true;
            }

            // Create a new WebRTC streaming connection
            auto streamingConnection = m_server.m_webRtcServer.getConnectionByIndex(std::atoi(connectionIndex));

            if (!streamingConnection)
            {
                *status_code = 404;
                mg_printf(connection, "HTTP/1.1 404 OK\r\nContent-Type: text/json\r\nConnection: close\r\n\r\n");
                mg_printf(connection, "{}");

                return true;
            }

            std::vector<char> content;
            content.resize(mg_get_request_info(connection)->content_length + 1);
            content[mg_get_request_info(connection)->content_length] = '\0';

            mg_read(connection, content.data(), mg_get_request_info(connection)->content_length);

            streamingConnection->setAnswer(content.data());

            *status_code = 200;
            mg_printf(connection, "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nConnection: close\r\n\r\n");
            mg_printf(connection, "{}");

            return true;
        }

      private:
        SignalingWebServer& m_server;
    };

  public:
    SignalingWebServer(WebRTCServer& webRtcServer) : m_webRtcServer(webRtcServer), m_offerHandler(*this), m_answerHandler(*this)
    {
        const char* serverOptions[] = {"document_root", ".\\src\\server\\www\\", "listening_ports", "8081", nullptr};

        m_webServer = std::make_unique<CivetServer>(serverOptions);

        m_webServer->addHandler("/offer", m_offerHandler);
        m_webServer->addHandler("/answer", m_answerHandler);
    }

    ~SignalingWebServer()
    {
        m_webServer = nullptr;

        mg_exit_library();
    }

  private:
    friend OfferHandler;

    WebRTCServer& m_webRtcServer;

    OfferHandler m_offerHandler;
    AnswerHandler m_answerHandler;

    std::unique_ptr<CivetServer> m_webServer;
};

WebRTCServer::WebRTCServer() = default;

WebRTCServer::~WebRTCServer() = default;

bool WebRTCServer::init(Ratio frameRate)
{
    m_frameRate = frameRate;

    m_signalingWebServer = std::make_unique<SignalingWebServer>(*this);

    rtc::InitLogger(rtc::LogLevel::Info,
                    [](rtc::LogLevel level, std::string message) -> void
                    {
                        switch (level)
                        {
                        case rtc::LogLevel::Fatal:
                        case rtc::LogLevel::Error:
                            error("WebRTC", message.c_str());
                            break;
                        case rtc::LogLevel::Warning:
                            warning("WebRTC", message.c_str());
                            break;
                        case rtc::LogLevel::Info:
                        case rtc::LogLevel::Verbose:
                        case rtc::LogLevel::Debug:
                            info("WebRTC", message.c_str());
                            break;
                        }
                    });

    return true;
}

void WebRTCServer::shutdown()
{
    m_signalingWebServer = nullptr;

    {
        std::lock_guard _(m_connectionMutex);
        m_connections.clear();
    }
}

void WebRTCServer::onEncodedSampleAvailable(std::chrono::nanoseconds originalTimeStamp, const std::vector<std::byte>& sample, uint64_t frameId, const std::vector<std::byte>& sequenceParameters)
{
    broadCastVideoSample(originalTimeStamp, sample, sequenceParameters);

    tick();
}

WebRTCConnection* WebRTCServer::createConnectionInstance()
{
    double frameTime = 1.0f / m_frameRate.asFloat();

    auto connection = std::make_unique<WebRTCConnection>(m_nextConnectionIndex++, m_lastSentSampleTimeStamp, frameTime);

    auto retVal = connection.get();

    {
        std::lock_guard _(m_connectionMutex);
        m_connections[connection->getIndex()] = std::move(connection);
    }

    return retVal;
}

WebRTCConnection* WebRTCServer::getConnectionByIndex(uint64_t index) const
{
    std::lock_guard _(m_connectionMutex);

    auto it = m_connections.find(index);

    if (it == m_connections.cend())
    {
        return nullptr;
    }

    return (*it).second.get();
}

void WebRTCServer::broadCastJSON(const std::string& json)
{
    // Loop through all active connections and send them the JSON data
    // this is used to broadcast POI positions for example.
    {
        std::lock_guard _(m_connectionMutex);

        for (auto& it : m_connections)
        {
            it.second->sendStringOnDataChannel(json);
        }
    }
}

void WebRTCServer::broadCastVideoSample(std::chrono::nanoseconds originalTimeStamp, const std::vector<std::byte>& sample, const std::vector<std::byte>& sequenceParameters)
{
    m_lastSentSampleTimeStamp = originalTimeStamp;

    // Loop through all active connections and send them the video sample.
    {
        std::lock_guard _(m_connectionMutex);

        for (auto& it : m_connections)
        {
            it.second->sendVideoSample(originalTimeStamp, sample, sequenceParameters);
        }
    }
}

void WebRTCServer::tick()
{
    {
        std::lock_guard _(m_connectionMutex);

        auto it = m_connections.begin();
        while (it != m_connections.end())
        {
            if (it->second->getState() == WebRTCConnection::State::Disconnected)
            {
                info("WebRTC", "Cleaning up connection %d since it's disconnected.", it->first);
                it = m_connections.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}
