
#pragma once

#include <optional>
#include <string>
#include <vector>


struct BandAction
{
    // mandatory
    std::string id;
    double  freq_from;      // frequency in Hz - defining the band
    double  freq_to;        // frequency in Hz - defining the band

    // all the optional settings
    std::optional<char>     sampling_mode;      // valid: "I", "Q", or "C" (or lowercase)
    std::optional<double>   samplerate;         // A/D sample rate

    std::optional<double>   tuner_bandwidth;
    std::optional<double>   r820t_tuner_band_center;
    std::optional<char>     tuning_sideband;    // valid: "L" or "U" (or lowercase)

    std::optional<bool>     tuner_rf_agc;
    std::optional<double>   tuner_rf_gain_db;

    std::optional<bool>     tuner_if_agc;
    std::optional<double>   tuner_if_gain_db;

    std::optional<bool>     rtl_digital_agc;

    std::optional<bool>     gpio_button0;
    std::optional<bool>     gpio_button1;
    std::optional<bool>     gpio_button2;
    std::optional<bool>     gpio_button3;
    std::optional<bool>     gpio_button4;
};

void init_toml_config();

const BandAction* update_band_action(double new_frequency);

