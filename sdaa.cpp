#include <SoapySDR/Device.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Formats.h>
#include <yaml-cpp/yaml.h>
#include <yaml-cpp/exceptions.h>
#include <iostream>
#include <cstdint>
#include <memory>
#include <format>
#include <cassert>

#include "sdaa_data.hpp"

using namespace sdaa;

extern "C" size_t find_device(const char *addr, uint32_t *result, size_t max_n, int16_t local_port);
extern "C" bool make_device(const char *addr, int16_t local_port);
extern "C" bool unmake_device(const char *addr, int16_t local_port);
extern "C" bool start_stream(const char *addr, int16_t local_port);

constexpr int16_t local_port = 3002;
using namespace std;
using namespace SoapySDR;

struct SdaaCfg
{
    std::string ctrl_ip;
    uint16_t ctrl_port;
    std::vector<std::tuple<std::string, uint16_t>> payload;
};

namespace YAML
{
    template <>
    struct convert<SdaaCfg>
    {
        static Node encode(const SdaaCfg &rhs)
        {
            Node node;
            node["ctrl_ip"] = rhs.ctrl_ip;
            node["ctrl_port"] = rhs.ctrl_port;
            Node payload_node;
            for (auto &x : rhs.payload)
            {
                Node node1;
                node1["ip"] = std::get<0>(x);
                node1["port"] = std::get<1>(x);
                payload_node.push_back(node1);
            }
            node["payload"] = payload_node;
            return node;
        }

        static bool decode(const Node &node, SdaaCfg &rhs)
        {
            rhs.ctrl_ip = node["ctrl_ip"].as<std::string>();
            rhs.ctrl_port = node["ctrl_port"].as<uint16_t>();
            Node payload_node = node["payload"];
            rhs.payload.clear();
            for (int i = 0; i < payload_node.size(); ++i)
            {
                std::string ip = payload_node[i]["ip"].as<std::string>();
                uint16_t port = payload_node[i]["port"].as<uint16_t>();
                rhs.payload.push_back(std::make_tuple(ip, port));
            }
            return true;
        }
    };
}

/***********************************************************************
 * Device interface
 **********************************************************************/
class SdaaSDR : public SoapySDR::Device
{
private:
    SdaaCfg cfg;
    // int64_t lo_ch;
    uint32_t stream_handler;
    std::unique_ptr<SdaaReceiver> device_handler;
    std::vector<std::complex<float>> *ptr_buffer;
    size_t offset{0};

public:
    // Implement constructor with device specific arguments...
    SdaaSDR() = default;
    SdaaSDR(const SdaaCfg &cfg1)
        : SoapySDR::Device(), cfg(cfg1), stream_handler(0), device_handler(make_unique<SdaaReceiver>(std::get<0>(cfg.payload[0]), std::get<1>(cfg.payload[0]), 8192, 4, fir_coeffs_half())), ptr_buffer(nullptr)
    {
        std::cout << "init" << std::endl;
        assert(make_device(cfg.ctrl_ip.c_str(), 3001));

        offset = device_handler->calc_output_size();
    }

    ~SdaaSDR()
    {
        deactivateStream(
            (Stream *)this,
            0, 0);
        assert(unmake_device(cfg.ctrl_ip.c_str(), 3001));
    }

public:
    std::string getDriverKey(void) const
    {
        return "sdaa";
    }

    std::string getHardwareKey(void) const
    {
        return cfg.ctrl_ip;
    }

    Kwargs getHardwareInfo(void) const
    {
        Kwargs result;
        result["vendor"] = "naoc";
        result["smp rate"] = "480E6 Sps";
        return result;
    }

    void setFrontendMapping(const int direction, const std::string &mapping)
    {
    }

    size_t getNumChannels(const int direction) const
    {
        return direction == SOAPY_SDR_RX ? cfg.payload.size() : 0;
    }

    Kwargs getChannelInfo(const int direction, const size_t channel) const
    {
        Kwargs result;
        result["Num"] = "4";
        result["max input voltage"] = "1 Vpp";

        return result;
    }

    bool getFullDuplex(const int direction, const size_t channel) const
    {
        return false;
    }

    std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const
    {
        std::vector<std::string> result;
        if (direction == SOAPY_SDR_RX && channel < getNumChannels(direction))
        {
            result.push_back(SOAPY_SDR_CF32);
        }
        return result;
    }

    std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const
    {
        fullScale = 32767;
        return SOAPY_SDR_S16;
    }

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel) const
    {
        SoapySDR::Range r(30e6, 210e6, 240e6 / 4096);
        return SoapySDR::RangeList{r};
    }

    SoapySDR::RangeList getBandwidthRange(const int direction, const size_t channel) const
    {
        SoapySDR::Range r(120e6, 120e6, 0);
        return SoapySDR::RangeList{r};
    }

    void setBandwidth(const int direction, const size_t channel, const double bw)
    {
    }

    double getBandwidth(const int direction, const size_t channel) const
    {
        return 120e6;
    }

    // Implement all applicable virtual methods from SoapySDR::Device

    void setFrequency(const int direction, const size_t channel, const double frequency, const Kwargs &args = Kwargs())
    {
        std::cout << "=================================" << std::endl;
        std::cout << "new freq:" << frequency << std::endl;
        int idx = frequency / 240e6 * 2048;
        device_handler->set_lo_ch(idx);
        std::cout << "lo ch:" << device_handler->get_lo_ch() << std::endl;
        std::cout << "=================================" << std::endl;
    }

    double getFrequency(const int direction, const size_t channel) const
    {
        return device_handler->get_lo_ch() * 240e6 / 2048;
    }

    SoapySDR::RangeList getSampleRateRange(const int direction, const size_t channel) const
    {
        SoapySDR::Range r(120e6, 120e6, 0);
        return SoapySDR::RangeList{r};
    }

    double getSampleRate(const int direction, const size_t channel) const
    {
        return 120e6;
    }

    SoapySDR::ArgInfoList getStreamArgsInfo(const int direction, const size_t channel)
    {
        if (direction != SOAPY_SDR_RX)
        {
            throw std::runtime_error("sdaa is RX only, use SOAPY_SDR_RX");
        }

        SoapySDR::ArgInfoList stream_args;
        ArgInfo lo_ch_arg;
        lo_ch_arg.key = "lo_ch";
        lo_ch_arg.value = "1024";
        lo_ch_arg.name = "lo channel";
        lo_ch_arg.description = "The channel of lo frequency, lo freq=lo_ch/2048*120 MHz";
        lo_ch_arg.units = "";
        lo_ch_arg.type = ArgInfo::INT;

        stream_args.push_back(lo_ch_arg);

        return stream_args;
    }

    Stream *setupStream(
        const int direction,
        const std::string &format,
        const std::vector<size_t> &channels = std::vector<size_t>(),
        const Kwargs &args = Kwargs())
    {
        std::cout << "==========================" << std::endl;
        for (auto &i : args)
        {
            std::cout << i.first << " : " << i.second << std::endl;
        }
        std::cout << "==========================" << std::endl;
        if (direction == SOAPY_SDR_TX)
        {
            throw std::runtime_error("sdaa is RX only, use SOAPY_SDR_RX");
        }

        if (format != SOAPY_SDR_CF32)
        {
            throw std::runtime_error("sdaa only support CF32");
        }

        auto iter = args.find("lo_ch");
        if (iter != args.end())
        {
            // std::cout << "lo ch not given, use default value 1024" << std::endl;
            device_handler->set_lo_ch(atoi(iter->second.c_str()));

            // throw std::runtime_error("lo_ch not given");
        }

        return (Stream *)this;
    }

    int activateStream(
        Stream *stream,
        const int flags = 0,
        const long long timeNs = 0,
        const size_t numElems = 0)
    {
        device_handler->start();
        assert(start_stream(cfg.ctrl_ip.c_str(), 3001));
        while (!device_handler->pop_ddc(ptr_buffer))
        {
            std::this_thread::yield();
        }
        return 0;
    }

    void closeStream(Stream *stream)
    {
        deactivateStream(stream, 0, 0);
    }

    int deactivateStream(
        Stream *stream,
        const int flags = 0,
        const long long timeNs = 0)
    {
        device_handler->stop();
        device_handler->push_free_ddc(ptr_buffer);
        ptr_buffer = nullptr;
        return 0;
    }

    size_t getStreamMTU(Stream *stream) const
    {
        return device_handler->calc_output_size();
    }

    int readStream(
        Stream *stream,
        void *const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs)
    {
        size_t n_to_copy = numElems;
        while (n_to_copy > 0)
        {
            size_t navail = ptr_buffer->size() - offset;
            size_t n_to_copy1 = std::min(navail, n_to_copy);
            if (n_to_copy1 > 0)
            {
                std::memcpy(buffs[0], (void *)(ptr_buffer->data() + offset), n_to_copy * sizeof(float) * 2);
                n_to_copy -= n_to_copy1;
                offset += n_to_copy1;
            }
            assert(offset <= ptr_buffer->size());
            if (offset == ptr_buffer->size())
            {
                offset = 0;
                device_handler->push_free_ddc(ptr_buffer);

                while (!device_handler->pop_ddc(ptr_buffer))
                {
                    std::this_thread::yield();
                }
            }
        }

        for (int i = 0; i < numElems; ++i)
        {

            // assert(!(std::isnan(x.real())||std::isnan(x.imag())));
            ((std::complex<float> *)buffs[0])[i] /= 50.0;
            auto x = ((std::complex<float> *)buffs[0])[i];
            if (!std::isnormal(x.real()) || !std::isnormal(x.imag()))
            {
                ((std::complex<float> *)buffs[0])[i] = std::complex<float>(1e-5, 1e-5);
            }
        }

        for (int i = 0; i < numElems; ++i)
        {
            auto &x = ((std::complex<float> *)buffs[0])[i];
            if (std::abs(x) > 10e9)
            {
	            //std::cout<<"x"<<std::endl;
                x = 0.0;
            }
        }
        return numElems;
    }

    std::vector<std::string> listAntennas(const int direction, const size_t channel) const
    {
        if (direction == SOAPY_SDR_RX)
        {
            return std::vector<std::string>{"EXT"};
        }
        else
        {
            return std::vector<std::string>();
        }
    }

    void setAntenna(const int direction, const size_t channel, const std::string &name)
    {
    }

    std::string getAntenna(const int direction, const size_t channel) const
    {
        return std::string("EXT");
    }

    bool hasDCOffsetMode(const int direction, const size_t channel) const
    {
        return false;
    }

    std::vector<std::string> listGains(const int direction, const size_t channel) const
    {
        return std::vector<std::string>{"TOTAL"};
    }

    double getGain(const int direction, const size_t channel) const
    {
        return 0.0;
    }

    double getGain(const int direction, const size_t channel, const std::string &name) const
    {
        return 0.0;
    }

    SoapySDR::Range getGainRange(const int direction, const size_t channel) const
    {
        SoapySDR::Range r(0.0, 0.0, 0.0);
        return r;
    }

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const
    {
        SoapySDR::Range r(0.0, 0.0, 0.0);
        return r;
    }
};

/***********************************************************************
 * Find available devices
 **********************************************************************/
SoapySDR::KwargsList findSdaaSDR(const SoapySDR::Kwargs &args)
{
    (void)args;
    for (auto &x : args)
    {
        std::cout << x.first << " : " << x.second << std::endl;
    }
    // locate the device on the system...
    // return a list of 0, 1, or more argument maps that each identify a device
    SoapySDR::Kwargs args1;
    SoapySDR::KwargsList kwl;

    // kwl.push_back(args1);

    uint32_t result[255];
    auto iter = args.find("cfg");
    const char *cfg_file = nullptr;

    if (iter != args.end())
    {
        cfg_file = iter->second.c_str();
    }
    else
    {
        cfg_file = "./cfg.yaml";
    }

    SdaaCfg cfg;
    try
    {
        cfg = YAML::LoadFile(cfg_file).as<SdaaCfg>();
    }
    catch (const YAML::BadFile &e)
    {
        return kwl;
    }

    std::string addr = cfg.ctrl_ip;

    auto n = find_device(addr.c_str(), result, 255, 3002);
    std::cout << n << " devices found" << std::endl;
    std::cout << "they are:" << std::endl;
    for (ulong i = 0; i < n; ++i)
    {
        auto ip = result[i];
        auto ip1 = (ip & 0xff000000) >> 24;
        auto ip2 = (ip & 0xff0000) >> 16;
        auto ip3 = (ip & 0xff00) >> 8;
        auto ip4 = (ip & 0xff);
        auto ip_str = std::format("{}.{}.{}.{}", ip1, ip2, ip3, ip4);
        std::cout << ip_str << std::endl;
        SoapySDR::Kwargs args1;
        args1["addr"] = ip_str;
        args1["cfg"] = cfg_file;
        kwl.push_back(args1);
    }
    std::cout << "kwl size=" << kwl.size() << std::endl;
    std::cout << "-----==================------" << std::endl;

    return kwl;
}

/***********************************************************************
 * Make device instance
 **********************************************************************/
SoapySDR::Device *makeSdaaSDR(const SoapySDR::Kwargs &args)
{
    (void)args;
    std::cout << "size=" << args.size() << std::endl;
    for (auto &x : args)
    {
        std::cout << x.first << " : " << x.second << std::endl;
    }
    // create an instance of the device object given the args
    // here we will translate args into something used in the constructor

    auto iter = args.find("cfg");
    const char *cfg_file = nullptr;
    if (iter != args.end())
    {
        cfg_file = iter->second.c_str();
    }
    else
    {
        cfg_file = "./cfg.yaml";
    }

    SdaaCfg cfg = YAML::LoadFile(cfg_file).as<SdaaCfg>();

    return new SdaaSDR(cfg);
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerSdaaSDR("sdaa", &findSdaaSDR, &makeSdaaSDR, SOAPY_SDR_ABI_VERSION);
