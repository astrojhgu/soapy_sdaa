#include "sdaa_data.hpp"
#include "ddc.h"
namespace sdaa
{
    SdaaReceiver::SdaaReceiver(const std::string &listen_ip, unsigned short port, size_t ddc_batch_num, size_t ndec, std::vector<float> fir_coeffs)
        : socket_(io_context_,
                  boost::asio::ip::udp::endpoint(
                      boost::asio::ip::address::from_string(listen_ip), port))
    {
        int K = fir_coeffs.size() / ndec;
        std::cout << "K=" << K << std::endl;
        init_ddc_resources(&res_, N_PT_PER_FRAME, ddc_batch_num, ndec, K, fir_coeffs.data());

        init_free_payload_queue();
        init_free_ddc_queue();
        try
        {
            socket_.set_option(
                boost::asio::socket_base::receive_buffer_size(1024 * 1024 * 100));
        }
        catch (const std::exception &e)
        {
            std::cerr << "Socket option error: " << e.what() << std::endl;
        }
    }

    SdaaReceiver::~SdaaReceiver()
    {
        free_ddc_resources(&res_);
        running_ = false;
    }

    void SdaaReceiver::set_lo_ch(int32_t lo_ch)
    {
        lo_ch_ = lo_ch;
    }

    void SdaaReceiver::init_free_payload_queue()
    {
        for (size_t i = 0; i < QUEUE_CAPACITY; ++i)
        {
            free_payload_queue_.push(payload_pool_.construct());
        }
    }

    void SdaaReceiver::init_free_ddc_queue()
    {
        size_t n_out = calc_output_size();
        for (size_t i = 0; i < 8; ++i)
        {
            free_ddc_queue_.push(ddc_pool_.construct(n_out));
        }
    }

    void SdaaReceiver::receiver_thread()
    {
        constexpr uint64_t MAX_GAP = 1000000;
        uint64_t expected_pkt_cnt = 0;
        int64_t current_base_id = 0;
        uint64_t current_port_id = 0;
        bool initial = true;

        boost::asio::ip::udp::endpoint sender_endpoint;

        while (running_)
        {
            sdaa::Payload *p = nullptr;
            if (!free_payload_queue_.pop(p))
            {
                // std::cout<<"full.."<<std::endl;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                continue;
            }

            boost::system::error_code ec;
            size_t length = socket_.receive_from(
                boost::asio::buffer(p, sizeof(sdaa::Payload)),
                sender_endpoint, 0, ec);

            if (ec || length != sizeof(sdaa::Payload))
            {
                free_payload_queue_.push(p);
                continue;
            }

            if (p->head != 0x12345678 || p->version != 0xa1a1a1a1)
            {
                free_payload_queue_.push(p);
                continue;
            }

            if (p->pkt_cnt == 0)
            {
                std::cout << "initialized" << std::endl;
                initial = true;
            }

            if (initial)
            {
                expected_pkt_cnt = p->pkt_cnt + 1;
                current_base_id = p->base_id;
                current_port_id = p->port_id;
                initial = false;
                payload_queue_.push(p);
            }
            else
            {
                if (p->pkt_cnt == expected_pkt_cnt)
                {
                    current_base_id = p->base_id;
                    current_port_id = p->port_id;
                    payload_queue_.push(p);
                    expected_pkt_cnt++;
                }
                else if (p->pkt_cnt > expected_pkt_cnt)
                {
                    std::cout << "u";
                    uint64_t missing = p->pkt_cnt - expected_pkt_cnt;
                    for (uint64_t i = 0; i < missing; ++i)
                    {
                        sdaa::Payload *fake = nullptr;
                        if (!free_payload_queue_.pop(fake))
                            break;

                        fake->pkt_cnt = expected_pkt_cnt + i;
                        fake->base_id = current_base_id;
                        fake->port_id = current_port_id;
                        memset(fake->data, 0, sizeof(fake->data));

                        payload_queue_.push(fake);
                    }
                    current_base_id = p->base_id;
                    current_port_id = p->port_id;
                    payload_queue_.push(p);
                    expected_pkt_cnt = p->pkt_cnt + 1;
                }
                else
                {
                    if ((expected_pkt_cnt - p->pkt_cnt) > MAX_GAP)
                    {
                        expected_pkt_cnt = p->pkt_cnt + 1;
                        current_base_id = p->base_id;
                        current_port_id = p->port_id;
                        payload_queue_.push(p);
                    }
                    else
                    {
                        free_payload_queue_.push(p);
                    }
                }
            }
        }
    }

    void SdaaReceiver::ddc_thread()
    {
        while (is_running())
        {
            sdaa::Payload *p = nullptr;
            if (pop_payload(p))
            {
                if (ddc(p->data, lo_ch_.load(), &res_))
                {
                    std::vector<std::complex<float>> *p1 = nullptr;
                    while (is_running() && !free_ddc_queue_.pop(p1))
                    {
                        std::this_thread::yield();
                    }
                    fetch_output(p1->data(), &res_);
                    ddc_queue_.push(p1);
                }
                push_free_payload(p);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    bool SdaaReceiver::pop_payload(sdaa::Payload *&p)
    {
        return payload_queue_.pop(p);
    }

    void SdaaReceiver::push_free_payload(sdaa::Payload *p)
    {
        free_payload_queue_.push(p);
    }

    bool SdaaReceiver::pop_ddc(std::vector<std::complex<float>> *&p)
    {
        return ddc_queue_.pop(p);
    }

    void SdaaReceiver::push_free_ddc(std::vector<std::complex<float>> *&p)
    {
        free_ddc_queue_.push(p);
    }

    bool SdaaReceiver::is_running() const
    {
        return running_.load();
    }

    void SdaaReceiver::start()
    {
        running_ = true;
        std::thread receiver([this]
                             { this->receiver_thread(); });

        std::thread ddc_t([this]
                          { this->ddc_thread(); });
        receiver.detach();
        ddc_t.detach();
    }

    void SdaaReceiver::stop()
    {
        running_ = false;
    }

    size_t SdaaReceiver::calc_output_size()const{
        return ::calc_output_size(&res_);
    }

    std::vector<float> fir_coeffs()
    {
        return std::vector<float>{
            1.24919278e-04,
            1.87340062e-04,
            2.54336466e-04,
            3.19419542e-04,
            3.74655087e-04,
            4.11165271e-04,
            4.19813565e-04,
            3.92038171e-04,
            3.20780621e-04,
            2.01440077e-04,
            3.27717133e-05,
            -1.82358899e-04,
            -4.36453212e-04,
            -7.17168929e-04,
            -1.00748549e-03,
            -1.28628744e-03,
            -1.52937838e-03,
            -1.71090455e-03,
            -1.80513157e-03,
            -1.78848233e-03,
            -1.64171198e-03,
            -1.35207001e-03,
            -9.15281892e-04,
            -3.37176770e-04,
            3.65206597e-04,
            1.16318370e-03,
            2.01677387e-03,
            2.87590733e-03,
            3.68250762e-03,
            4.37340718e-03,
            4.88399089e-03,
            5.15240109e-03,
            5.12408244e-03,
            4.75640038e-03,
            4.02303662e-03,
            2.91785302e-03,
            1.45792206e-03,
            -3.14549107e-04,
            -2.33162605e-03,
            -4.50054372e-03,
            -6.70616835e-03,
            -8.81500806e-03,
            -1.06806578e-02,
            -1.21504677e-02,
            -1.30731356e-02,
            -1.33068463e-02,
            -1.27275284e-02,
            -1.12367619e-02,
            -8.76886842e-03,
            -5.29673644e-03,
            -8.35986530e-04,
            4.55284086e-03,
            1.07642930e-02,
            1.76503082e-02,
            2.50247886e-02,
            3.26702858e-02,
            4.03465187e-02,
            4.78003304e-02,
            5.47766012e-02,
            6.10295684e-02,
            6.63339694e-02,
            7.04954233e-02,
            7.33595010e-02,
            7.48189943e-02,
            7.48189943e-02,
            7.33595010e-02,
            7.04954233e-02,
            6.63339694e-02,
            6.10295684e-02,
            5.47766012e-02,
            4.78003304e-02,
            4.03465187e-02,
            3.26702858e-02,
            2.50247886e-02,
            1.76503082e-02,
            1.07642930e-02,
            4.55284086e-03,
            -8.35986530e-04,
            -5.29673644e-03,
            -8.76886842e-03,
            -1.12367619e-02,
            -1.27275284e-02,
            -1.33068463e-02,
            -1.30731356e-02,
            -1.21504677e-02,
            -1.06806578e-02,
            -8.81500806e-03,
            -6.70616835e-03,
            -4.50054372e-03,
            -2.33162605e-03,
            -3.14549107e-04,
            1.45792206e-03,
            2.91785302e-03,
            4.02303662e-03,
            4.75640038e-03,
            5.12408244e-03,
            5.15240109e-03,
            4.88399089e-03,
            4.37340718e-03,
            3.68250762e-03,
            2.87590733e-03,
            2.01677387e-03,
            1.16318370e-03,
            3.65206597e-04,
            -3.37176770e-04,
            -9.15281892e-04,
            -1.35207001e-03,
            -1.64171198e-03,
            -1.78848233e-03,
            -1.80513157e-03,
            -1.71090455e-03,
            -1.52937838e-03,
            -1.28628744e-03,
            -1.00748549e-03,
            -7.17168929e-04,
            -4.36453212e-04,
            -1.82358899e-04,
            3.27717133e-05,
            2.01440077e-04,
            3.20780621e-04,
            3.92038171e-04,
            4.19813565e-04,
            4.11165271e-04,
            3.74655087e-04,
            3.19419542e-04,
            2.54336466e-04,
            1.87340062e-04,
            1.24919278e-04,
        };
    }
}
