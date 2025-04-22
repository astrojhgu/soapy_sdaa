#include "sdaa_data.hpp"
using namespace sdaa;

#include <fstream>
void process_ddc(std::vector<std::complex<float>> *p, std::ofstream &ofs)
{
    static int cnt = 0;
    if (cnt % 100 == 0)
    {
        std::cout << cnt << std::endl;
    }
    ofs.write((const char *)p->data(), sizeof(std::complex<float>) * p->size());
    cnt++;
}

void main_thread(SdaaReceiver &processor)
{
    std::ofstream outfile("a.bin");

    while (processor.is_running())
    {
        std::vector<std::complex<float>> *p = nullptr;
        if (processor.pop_ddc(p))
        {
            process_ddc(p, outfile);
            processor.push_free_ddc(p);
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

int main()
{
    try
    {
        SdaaReceiver processor("192.168.10.10", 3000, 8192, 8, fir_coeffs_quarter());
        processor.set_lo_ch(885);

        // std::thread receiver([&processor]
        //                      { processor.receiver_thread(); });

        // std::thread ddc_t([&processor]
        //                   { processor.ddc_thread(); });
        std::thread main_t([&processor]
                           { main_thread(processor); });

        // ddc_t.join();
        // receiver.join();

        processor.start();
        main_t.join();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
