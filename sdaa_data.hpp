#ifndef SDAA_DATA_HPP
#define SDAA_DATA_HPP

#pragma once

#include <iostream>
#include <complex>
#include <atomic>
#include <thread>
#include <cstring>
#include <boost/asio.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/pool/object_pool.hpp>
#include "ddc.h"

namespace sdaa
{
    constexpr uint64_t N_PT_PER_FRAME = 4096;

#pragma pack(push, 1)
    struct Payload
    {
        uint32_t head = 0x12345678;
        uint32_t version = 0xa1a1a1a1;
        uint64_t pkt_cnt = 0;
        int64_t base_id = 0;
        uint64_t port_id = 0;
        uint64_t npt_per_frame = N_PT_PER_FRAME;
        uint64_t _reserve = 0;
        int16_t data[N_PT_PER_FRAME] = {};
    };
#pragma pack(pop)

    constexpr size_t QUEUE_CAPACITY = 65536;
    using PayloadQueue = boost::lockfree::spsc_queue<sdaa::Payload *,
                                                     boost::lockfree::capacity<QUEUE_CAPACITY>>;

    using DdcQueue = boost::lockfree::spsc_queue<std::vector<std::complex<float>> *,
                                                 boost::lockfree::capacity<QUEUE_CAPACITY>>;

    class SdaaReceiver
    {
    public:
        SdaaReceiver(const std::string &listen_ip, unsigned short port, size_t ddc_batch_num, size_t ndec, std::vector<float> fir_coeffs);
        ~SdaaReceiver();

        void receiver_thread();
        void ddc_thread();
        bool pop_payload(sdaa::Payload *&p);
        void push_free_payload(sdaa::Payload *p);

        bool pop_ddc(std::vector<std::complex<float>> *&p);
        void push_free_ddc(std::vector<std::complex<float>> *&p);

        bool is_running() const;
        void set_lo_ch(int32_t);
        int32_t get_lo_ch()const;

        void start();
        void stop();

        size_t calc_output_size()const;

    private:
        void init_free_payload_queue();
        void init_free_ddc_queue();

        PayloadQueue payload_queue_;
        PayloadQueue free_payload_queue_;

        DdcQueue ddc_queue_;
        DdcQueue free_ddc_queue_;

        std::atomic<bool> running_{false};
        std::atomic<int32_t> lo_ch_{1024};
        boost::object_pool<sdaa::Payload> payload_pool_;
        boost::object_pool<std::vector<std::complex<float>>> ddc_pool_;
        boost::asio::io_context io_context_;
        boost::asio::ip::udp::socket socket_;
        DDCResources res_;
    };


    std::vector<float> fir_coeffs_quarter();
    std::vector<float> fir_coeffs_half();
}

#endif
