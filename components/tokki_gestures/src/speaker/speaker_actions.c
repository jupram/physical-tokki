#include "speaker/speaker_actions.h"

#include "tokki_speaker.h"

static esp_err_t run_drink_water(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_DRINK_WATER);
}

static esp_err_t run_chirp(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_CHIRP);
}

static esp_err_t run_alert(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_ALERT);
}

const tokki_action_descriptor_t TOKKI_SPEAKER_DRINK_WATER_ACTION = {
    .id = "speaker.drink_water", .display_name = "Drink water phrase",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_drink_water,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_CHIRP_ACTION = {
    .id = "speaker.chirp", .display_name = "Bird-like chirp (synthesized)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_chirp,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_ALERT_ACTION = {
    .id = "speaker.alert", .display_name = "Short alert tone",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_alert,
};