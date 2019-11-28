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
const size_t num_bars = 60;

constexpr ssize_t matrix_to_bar[CMMKProM::key_map_rows][CMMKProM::key_map_cols] = {
    //                                    V
    {59, 48, 47, 36, 35, 24, 23, 12, 11,  0, 11, 12, 23, 24, 35, 36, 47, 48, 59},
    {58, 49, 46, 37, 34, 25, 22, 13, 10,  1, 10, 13, 22, 25, 34, 37, 46, 49, 58},
    {57, 50, 45, 38, 33, 26, 21, 14,  9,  2,  9, 14, 21, 26, 33, 38, 45, 50, 57},
    {56, 51, 44, 39, 32, 27, 20, 15,  8,  3,  8, 15, 20, 27, 32, 39, 44, 51, 56},
    {55, 52, 43, 40, 31, 28, 19, 16,  7,  4,  7, 16, 19, 28, 31, 40, 43, 52, 55},
    {54, 53, 42, 41, 30, 29, 18, 17,  6,  5,  6, 17, 18, 29, 30, 41, 42, 53, 54}
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
