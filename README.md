PPS Video Mirror Server Project
=======

[![License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

About
-------

This project aims to deliver low latency video streaming from capture cards (i.e. capturing video from a connected video camera) to multiple endpoints.
We use this for the Paw Pet Show at Eurofurence to allow our actors to see live how the audience would see their puppets and to correct focus of the puppets and more.

In a nutshell we use Windows Media Foundation to get video frames from a capture card, encode them with nvEnc and then use WebRTC (via libdatachannel) to stream the results to the endpoints.

Building
--------

Building works via CMake, there are two dependencies which need to be installed via vcpkg: libdatachannel[srtp]:x64-windows and civetweb:x64-windows.
Also necessary is the NVidia video encoder SDK which can be downloaded from NVidia directly (needs a developer account with them). Set the environment variable NV_VIDEO_CODEC_SDK_DIR to the folder of the extracted SDK and CMake will find the SDK automatically.
