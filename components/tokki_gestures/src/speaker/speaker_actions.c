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

static esp_err_t run_chime(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_CHIME);
}

static esp_err_t run_ping(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_PING);
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

const tokki_action_descriptor_t TOKKI_SPEAKER_CHIME_ACTION = {
    .id = "speaker.chime", .display_name = "Completion chime",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_chime,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_PING_ACTION = {
    .id = "speaker.ping", .display_name = "Short ping",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_ping,
};