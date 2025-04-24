#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Types.hpp>
#include <iostream>
#include <fstream>
#include <cassert>
#include <memory>
#include <chrono>
#include <thread>
using namespace std;

using namespace SoapySDR;
int main()
{
    // List all BB60C devices

    Kwargs args;
    args["driver"] = "sdaa";
    args["cfg"] = "cfg.yaml";

    SoapySDR::KwargsList bbDevices = SoapySDR::Device::enumerate(args);

    for (auto &x : bbDevices)
    {
        std::cout << "----" << std::endl;
        for (auto &y : x)
        {
            std::cout << y.first << " : " << y.second << std::endl;
        }
    }

    Kwargs stream_args;
    stream_args["lo_ch"] = "885";
    std::unique_ptr<SoapySDR::Device> sdr = std::unique_ptr<SoapySDR::Device>(SoapySDR::Device::make(bbDevices[0]));
    

    SoapySDR::Stream *rx_stream = sdr->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32, std::vector<size_t>{0}, stream_args);
    sdr->setFrequency(1, 0, 100e6);
    size_t NUM_SAMPLES = 65536*64;
    std::vector<std::complex<float>> buff(NUM_SAMPLES);

    void *buffs[] = {buff.data()};
    int flags;
    long long timeNs;
    assert(sdr->activateStream(rx_stream, 0, 0, 0) == 0);

    ofstream ofs("a.bin");
    for (int i = 0; i < 10; ++i)
    {
        sdr->readStream(
            rx_stream,
            buffs,
            NUM_SAMPLES,
            flags,
            timeNs,
            0);

        ofs.write((const char *)buff.data(), sizeof(float) * 2 * buff.size());
    }
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10000ms);
    sdr->deactivateStream(nullptr, 0,0);
    std::cerr<<"AAA"<<std::endl;
    std::this_thread::sleep_for(10000ms);
}
