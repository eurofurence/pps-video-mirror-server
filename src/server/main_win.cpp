
#include "cxxopts.hpp"
#include "nvenc.h"
#include "streaming/webrtc.h"
#include "version.h"
#include "video_input/media_foundation.h"

#ifdef _DEBUG
const bool debugBuild = true;
#else
const bool debugBuild = false;
#endif

// Ensure that the nVidia and AMD drivers put us on the high power GPU
extern "C"
{
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x1;
    _declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x1;
}

static std::atomic<bool> run = true;

BOOL WINAPI consoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT)
    {
        info("MAIN", "CTRL+C");
        run = false;

        return TRUE;
    }

    return FALSE;
}

int main(int argc, char** argv)
{
    {
        info("MAIN", "Initializing version " APP_VERSION_NAME);

        // Parse command line options
        cxxopts::Options options(APP_NAME, "PPS Video Mirror Server");
        options.add_options()("d,device", "The video capturer device name", cxxopts::value<std::string>());

        auto result = options.parse(argc, argv);

        std::string deviceName;
        if (result.count("device"))
        {
            deviceName = result["device"].as<std::string>();
            info("MAIN", "User requested device '%s'", deviceName.c_str());
        }

        if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
        {
            error("MAIN", "Couldn't set CTRL handler");
            return -1;
        }

        // Setup media foundation input and enumerate devices
        auto mediaFoundationInput = std::make_unique<MediaFoundationVideoInput>();
        auto inputDevices = mediaFoundationInput->enumerateDevices();

        if (inputDevices.empty())
        {
            error("MAIN", "No input devices!");
            return -1;
        }

        info("MAIN", "Found %d input devices.", inputDevices.size());
        for (const auto& inputDevice : inputDevices)
        {
            info("MAIN", "Input device '%s'.", inputDevice.c_str());
        }

        // Try to instantiate the device the user requested or the first one in the list
        auto inputDevice = mediaFoundationInput->instantiateDevice(deviceName.empty() ? inputDevices[0] : deviceName);

        if (!inputDevice)
        {
            error("MAIN", "Didn't get requested input device. Aborting.");
            return -1;
        }

        if (!inputDevice->prepareStreaming())
        {
            error("MAIN", "Input device failed stream preparation. Aborting.");
            return -1;
        }

        uint32_t inputWidth = 0, inputHeight = 0;
        inputDevice->getFrameSize(inputWidth, inputHeight);

        auto nvenc = std::make_unique<NVEnc>();
        if (!nvenc->init(inputWidth, inputHeight, inputDevice->getVideoFormat(), inputDevice->getFrameRate()))
        {
            error("MAIN", "NVEnc init failed. Aborting.");
            return -1;
        }

        auto webrtcServer = std::make_unique<WebRTCServer>();
        if (!webrtcServer->init(inputDevice->getFrameRate()))
        {
            error("MAIN", "WebRTCServer init failed. Aborting.");
            return -1;
        }

        info("MAIN", "Starting stream.");

        inputDevice->stream(run, nvenc.get(), webrtcServer.get());

        info("MAIN", "Shutting down");

        webrtcServer->shutdown();
        webrtcServer = nullptr;

        nvenc->shutdown();
        nvenc = nullptr;
    }

    _CrtDumpMemoryLeaks();

    return 0;
}
