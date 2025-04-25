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
#define USE_CUDA

#include "sdaa_data.h"
#include "sdaa_ctrl.h"

using namespace sdaa;

// constexpr int16_t local_port = 3002;
using namespace std;
using namespace SoapySDR;

struct SdaaCfg
{
    std::vector<uint32_t> ctrl_ip;
    uint16_t ctrl_port;
    uint16_t local_port;
    std::vector<std::tuple<std::vector<uint32_t>, uint16_t>> payload;
};

uint32_t ip2int(const std::vector<uint32_t> &ip)
{
    assert(ip.size() == 4);
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i)
    {
        result <<= 8;
        result += ip[i];
    }
    return result;
}

std::vector<uint32_t> int2ip(uint32_t x)
{
    std::vector<uint32_t> result;
    for (int i = 0; i < 4; ++i)
    {
        result.push_back((x & 0xff000000) >> 24);
        x <<= 8;
    }
    return result;
}

std::vector<uint32_t> parse_ip_to_vector(const std::string &ip_str)
{
    std::vector<uint32_t> result;
    std::istringstream ss(ip_str);
    std::string token;

    while (std::getline(ss, token, '.'))
    {
        int num = std::stoi(token);
        if (num < 0 || num > 255)
        {
            throw std::out_of_range("IP segment out of range: " + token);
        }
        result.push_back(static_cast<uint32_t>(num));
    }

    if (result.size() != 4)
    {
        throw std::invalid_argument("Invalid IP address format: " + ip_str);
    }

    return result;
}

std::string vector_to_ip_string(const std::vector<uint32_t> &parts)
{
    if (parts.size() != 4)
    {
        throw std::invalid_argument("IP vector must contain exactly 4 elements");
    }

    for (auto part : parts)
    {
        if (part > 255)
        {
            throw std::out_of_range("IP segment out of range: " + std::to_string(part));
        }
    }

    std::ostringstream ss;
    ss << parts[0] << '.' << parts[1] << '.' << parts[2] << '.' << parts[3];
    return ss.str();
}

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
            node["local_port"] = rhs.local_port;
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
            rhs.ctrl_ip = node["ctrl_ip"].as<std::vector<uint32_t>>();
            rhs.ctrl_port = node["ctrl_port"].as<uint16_t>();
            rhs.local_port = node["local_port"].as<uint16_t>();
            Node payload_node = node["payload"];
            rhs.payload.clear();
            for (int i = 0; i < payload_node.size(); ++i)
            {
                std::vector<uint32_t> ip = payload_node[i]["ip"].as<std::vector<uint32_t>>();
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
    int64_t lo_ch;
    float voltage_gain;

    std::unique_ptr<CSdr, decltype(free_sdr_device) &> device_handler;

public:
    // Implement constructor with device specific arguments...
    SdaaSDR() = default;
    SdaaSDR(const SdaaCfg &cfg1)
        : SoapySDR::Device(), cfg(cfg1), device_handler(new_sdr_device(ip2int(cfg.ctrl_ip), 3001, ip2int(std::get<0>(cfg.payload[0])), std::get<1>(cfg.payload[0])), free_sdr_device), lo_ch(1024), voltage_gain(1.0)
    {
    }

    ~SdaaSDR()
    {
        stop_data_stream(device_handler.get());
    }

public:
    std::string getDriverKey(void) const
    {
        return "sdaa";
    }

    std::string getHardwareKey(void) const
    {
        // return parse_ip_to_vector(cfg.ctrl_ip);
        return vector_to_ip_string(cfg.ctrl_ip);
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
        lo_ch = frequency / 240e6 * 2048;
        set_lo_ch(device_handler.get(), lo_ch);
        std::cout << "lo ch:" << lo_ch << std::endl;
        std::cout << "=================================" << std::endl;
    }

    double getFrequency(const int direction, const size_t channel) const
    {
        return lo_ch * 240e6 / 2048;
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
            // device_handler->set_lo_ch(atoi(iter->second.c_str()));
            set_lo_ch(device_handler.get(), atoi(iter->second.c_str()));

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
        // device_handler->start();
        start_data_stream(device_handler.get());
        std::cout << "ctrl addr=" << vector_to_ip_string(cfg.ctrl_ip) << std::endl;
        std::cout << "payload addr=" << vector_to_ip_string(std::get<0>(cfg.payload[0])) << std::endl;

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
        std::cerr << "BBB" << std::endl;
        stop_data_stream(device_handler.get());
        return 0;
    }

    size_t getStreamMTU(Stream *stream) const
    {
        return get_mtu();
    }

    int readStream(
        Stream *stream,
        void *const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs)
    {
        fetch_data(device_handler.get(), (CComplex *)buffs[0], numElems);
        auto buff = (CComplex *)buffs[0];
        for (int i = 0; i < numElems; ++i)
        {
            buff[i].re *= voltage_gain;
            buff[i].im *= voltage_gain;
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
        return std::log10(voltage_gain) * 20;
    }

    double getGain(const int direction, const size_t channel, const std::string &name) const
    {
        return std::log10(voltage_gain) * 20;
    }

    SoapySDR::Range getGainRange(const int direction, const size_t channel) const
    {
        SoapySDR::Range r(-30.0, 0.0, 0.0);
        return r;
    }

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const
    {
        SoapySDR::Range r(-100.0, 0.0, 0.0);
        return r;
    }

    void setGain(const int direction, const size_t channel, const std::string &name, const double value)
    {
        voltage_gain = std::pow(10.0, value / 20);
        std::cerr << "gain=" << voltage_gain << std::endl;
    }
};

/***********************************************************************
 * Find available devices
 **********************************************************************/
SoapySDR::KwargsList findSdaaSDR(const SoapySDR::Kwargs &args)
{
    std::cout << "================" << std::endl;
    std::cout << "finding sdaa sdr" << std::endl;
    std::cout << "================" << std::endl;
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
        cfg_file = "./cfg0.yaml";
    }

    std::vector<SdaaCfg> cfg;
    try
    {
        cfg = YAML::LoadFile(cfg_file).as<std::vector<SdaaCfg>>();
    }
    catch (const YAML::BadFile &e)
    {
        return kwl;
    }

    for (int dev_idx = 0; dev_idx < cfg.size(); ++dev_idx)
    {
        const auto &c = cfg[dev_idx];
        auto iter = args.find("ip");
        if (iter != args.end())
        {
            auto ip_vec = parse_ip_to_vector(iter->second);
            if (c.ctrl_ip != ip_vec)
            {
                continue;
            }
        }

        iter = args.find("dev_idx");
        if (iter != args.end())
        {
            auto idx = atoi(iter->second.c_str());
            std::cout<<"idx="<<idx<<std::endl;
            if (idx != dev_idx)
            {
                continue;
            }
        }

        auto n = find_device(ip2int(c.ctrl_ip), result, 255, 3002);
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
            std::cout << "dev idx= " << dev_idx << std::endl;
            SoapySDR::Kwargs args1;
            args1["cfg"] = cfg_file;
            args1["dev_idx"] = std::format("{}", dev_idx);

            kwl.push_back(args1);
        }
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
    std::cout << "===============" << std::endl;
    std::cout << "making sdaa sdr" << std::endl;
    std::cout << "===============" << std::endl;

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
        cfg_file = "./cfg0.yaml";
    }

    iter = args.find("dev_idx");
    assert(iter != args.end());
    auto dev_idx = atoi(iter->second.c_str());
    std::cout << "a" << std::endl;
    auto cfg = YAML::LoadFile(cfg_file).as<std::vector<SdaaCfg>>();

    std::cout << "b" << std::endl;
    return new SdaaSDR(cfg[dev_idx]);
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerSdaaSDR("sdaa", &findSdaaSDR, &makeSdaaSDR, SOAPY_SDR_ABI_VERSION);
