#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

#include "mk_pro_m.h"
#include "ModularSpec/OpenALDataFetcher.h"
#include "ModularSpec/Spectrum.h"
#include "ModularSpec/util.h"

const size_t fft_size = 8192;
const size_t sample_rate = 44100;
const size_t num_bars = 30;

constexpr ssize_t matrix_to_bar[CMMKProM::key_map_rows][CMMKProM::key_map_cols] = {
    //                                    V
    {27, 26, 21, 20, 15, 14,  9,  8,  3,  2,  3,  8,  9, 14, 15, 20, 21, 26, 27},
    {28, 25, 22, 19, 16, 13, 10,  7,  4,  1,  4,  7, 10, 13, 16, 19, 22, 25, 28},
    {29, 24, 23, 18, 17, 12, 11,  6,  5,  0,  5,  6, 11, 12, 17, 18, 23, 24, 29},
    {29, 24, 23, 18, 17, 12, 11,  6,  5,  0,  5,  6, 11, 12, 17, 18, 23, 24, 29},
    {28, 25, 22, 19, 16, 13, 10,  7,  4,  1,  4,  7, 10, 13, 16, 19, 22, 25, 28},
    {27, 26, 21, 20, 15, 14,  9,  8,  3,  2,  3,  8,  9, 14, 15, 20, 21, 26, 27},
};

void run(CMMKProM &kb) {
    float audio_data[fft_size];
    OpenALDataFetcher audio_fetcher(
        sample_rate,
        fft_size,
        [](const std::vector<std::string> &list) {
            // try to find "Monitor of Built-in Audio Analog Stereo"
            for (size_t i = 0; i < list.size(); ++i) {
                if (list[i].find("Monitor") != std::string::npos &&
                    list[i].find("Analog") != std::string::npos) {
                    return i;
                }
            }

            std::cerr << "failed to find audio device" << std::endl;
            return (size_t)0;
        }
    );

    Spectrum spec(fft_size);
    spec.UseLinearNormalisation(1, num_bars * 2);
    spec.average_weight = 0.7;
    spec.scale = 1;

    const float avg_max_weight = 0.8f;
    float avg_max = 0;
    float bar_data[num_bars];
    uint8_t out_bar_data[num_bars];
    CMMKProM::led_matrix matrix{};
    while (1) {
        audio_fetcher.UpdateData();
        audio_fetcher.GetData(audio_data);
        spec.Update(audio_data);
        spec.GetData(50, 2500, sample_rate, bar_data, num_bars);

        const float scale = std::clamp(1 / std::max(avg_max, 0.1f), 1.f, 10.f);
        float max = 0;
        for (size_t i = 0; i < num_bars; i++) {
            float val = std::pow(bar_data[i], 1.5f);
            max = std::max(max, val);
            out_bar_data[i] = (uint8_t)(std::clamp(val * scale * 255.0f, 1.0f, 255.0f));
        }
        avg_max = avg_max_weight * avg_max + (1.f - avg_max_weight) * max;
        // std::cout << "max = " << max << "; avg = " << avg_max << "; scale = " << scale << std::endl;

        for (size_t x = 0; x < CMMKProM::key_map_cols; x++) {
            for (size_t y = 0; y < CMMKProM::key_map_rows; y++) {
                matrix[y][x].r = out_bar_data[matrix_to_bar[y][x]];
            }
        }

        kb.set_leds_smooth(matrix);

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

}

int main() {
    try {
        CMMKProM kb;
        run(kb);
        // kb.do_thing();
    } catch (std::runtime_error &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
