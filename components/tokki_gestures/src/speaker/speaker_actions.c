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

static esp_err_t run_bubble(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_BUBBLE);
}

static esp_err_t run_whistle(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_WHISTLE);
}

static esp_err_t run_tone_low(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_TONE_LOW);
}

static esp_err_t run_tone_mid(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_TONE_MID);
}

static esp_err_t run_question(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_QUESTION);
}

static esp_err_t run_tone_high(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_TONE_HIGH);
}

static esp_err_t run_sparkle(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_SPARKLE);
}

static esp_err_t run_trill(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_TRILL);
}

static esp_err_t run_tone_rise(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_TONE_RISE);
}

static esp_err_t run_knock(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_KNOCK);
}

static esp_err_t run_sonar(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_SONAR);
}

static esp_err_t run_dog_bark(void)
{
    return tokki_speaker_play(TOKKI_SPEAKER_DOG_BARK);
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

const tokki_action_descriptor_t TOKKI_SPEAKER_BUBBLE_ACTION = {
    .id = "speaker.bubble", .display_name = "Bubble pop (synthesized)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_bubble,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_WHISTLE_ACTION = {
    .id = "speaker.whistle", .display_name = "Rising whistle (synthesized)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_whistle,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_TONE_LOW_ACTION = {
    .id = "speaker.tone_low", .display_name = "Warm tone (440 Hz)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_tone_low,
};

const tokki_action_descriptor_t TOKKI_SPEAKER_TONE_MID_ACTION = {
    .id = "speaker.tone_mid", .display_name = "Clear tone (660 Hz)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_tone_mid,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_QUESTION_ACTION = {
    .id = "speaker.question", .display_name = "Question cue (synthesized)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_question,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_TONE_HIGH_ACTION = {
    .id = "speaker.tone_high", .display_name = "Bright tone (880 Hz)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_tone_high,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_SPARKLE_ACTION = {
    .id = "speaker.sparkle", .display_name = "Sparkle (synthesized)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_sparkle,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_TRILL_ACTION = {
    .id = "speaker.trill", .display_name = "Trill (synthesized)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_trill,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_TONE_RISE_ACTION = {
    .id = "speaker.tone_rise", .display_name = "Rising tones (440/660/880 Hz)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_tone_rise,
};

const tokki_action_descriptor_t TOKKI_SPEAKER_KNOCK_ACTION = {
    .id = "speaker.knock", .display_name = "Knock-knock (synthesized)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_knock,
};
const tokki_action_descriptor_t TOKKI_SPEAKER_SONAR_ACTION = {
    .id = "speaker.sonar", .display_name = "Sonar echo (synthesized)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_sonar,
};

const tokki_action_descriptor_t TOKKI_SPEAKER_DOG_BARK_ACTION = {
    .id = "speaker.dog_bark", .display_name = "Dog bark (recording, twice)",
    .device = TOKKI_DEVICE_SPEAKER, .cancellable = false, .run = run_dog_bark,
};