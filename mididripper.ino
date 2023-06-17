#include <MIDI.h>
#include <Adafruit_NeoPixel.h>
#include <vector>

#define NUM_LEDS 16
#define LED_PIN 5

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define MAX_GROUPS 3 // Number of LED groups you want to control simultaneously

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

#define PIEZO_THRESH 60

#define ARP_DIVIDER 2
#define DRUM_DIVIDER 32
#define KICK_NOTE 36
#define SNARE_NOTE 57

const std::vector<uint8_t> arpNotes = {75, 79, 83, 78, 75, 79, 84, 78}; // Example notes: C4, E4, G4, C5
const int arpChannel = 3; // Channel 3 for arpeggio notes
int currentArpNoteIndex = -1; // Initialize to -1 so that the first note is at index 0

uint16_t drip = 0;
uint16_t velocity_div = 0;

unsigned long kickStartTime = 0;
bool kickNoteOn = false;
unsigned long snareStartTime = 0;
bool snareNoteOn = false;

const int FLICKER_DECAY_TIME_L = 400; // 3 seconds for example
const auto FLICKER_COLOR_A_L = strip.Color(0, 255, 0);   // green
const auto FLICKER_COLOR_B_L = strip.Color(0, 255, 255); // turq
const int FLICKER_START_INDEX_L = 2;
const int FLICKER_END_INDEX_L = 5;

const int FLICKER_DECAY_TIME_R = 700; // 3 seconds for example
const auto FLICKER_COLOR_A_R = strip.Color(0, 0, 255);   // Blue
const auto FLICKER_COLOR_B_R = strip.Color(255, 0, 255); // Purple
const int FLICKER_START_INDEX_R = 10;
const int FLICKER_END_INDEX_R = 13;

const int FLICKER_DECAY_TIME_C = 3000; // 3 seconds for example
const auto FLICKER_COLOR_A_C = strip.Color(255, 0, 0);   // Red
const auto FLICKER_COLOR_B_C = strip.Color(255, 128, 0); // Orange
const int FLICKER_START_INDEX_C = 0;
const int FLICKER_END_INDEX_C = 15;

const int MAX_INITIAL_BRIGHTNESS = 256;
const int MIN_INITIAL_BRIGHTNESS = 500;

struct FlickerGroup {
  uint32_t colorA;
  uint32_t colorB;
  unsigned long decayTime;
  uint8_t startIdx;
  uint8_t endIdx;
  unsigned long startTime;
  bool active;
};

FlickerGroup flickerGroups[MAX_GROUPS];

void updateFlickerGroups() {
  uint8_t r[NUM_LEDS] = {0};
  uint8_t g[NUM_LEDS] = {0};
  uint8_t b[NUM_LEDS] = {0};

  for (int i = 0; i < MAX_GROUPS; i++) {
    FlickerGroup& group = flickerGroups[i];
    if (!group.active) continue;

    unsigned long elapsedTime = millis() - group.startTime;

    if (elapsedTime >= group.decayTime) {
      group.active = false;
      continue;
    }

    int maxBrightness = (256 * (group.decayTime - elapsedTime)) / group.decayTime;
    int minBrightness = (600 * (group.decayTime - elapsedTime)) / group.decayTime;

    for (int j = group.startIdx; j <= group.endIdx; j++) {
      uint32_t color = (j % 2 == 0) ? group.colorA : group.colorB;
      int scale = random(minBrightness, 1001);

      r[j] = min(255, r[j] + (((color >> 16) & 0xFF) * scale * maxBrightness) / (1000 * 255));
      g[j] = min(255, g[j] + (((color >> 8) & 0xFF) * scale * maxBrightness) / (1000 * 255));
      b[j] = min(255, b[j] + ((color & 0xFF) * scale * maxBrightness) / (1000 * 255));
    }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, r[i], g[i], b[i]);
  }

  strip.show();
}

void startFlicker(uint32_t colorA, uint32_t colorB, unsigned long decayTime, uint8_t startIdx, uint8_t endIdx) {
  for (int i = 0; i < MAX_GROUPS; i++) {
    if (!flickerGroups[i].active) {
      flickerGroups[i].colorA = colorA;
      flickerGroups[i].colorB = colorB;
      flickerGroups[i].decayTime = decayTime;
      flickerGroups[i].startIdx = startIdx;
      flickerGroups[i].endIdx = endIdx;
      flickerGroups[i].startTime = millis();
      flickerGroups[i].active = true;
      return;
    }
  }
}

void playNextArpNote() {
  // If a note was previously played, send a Note Off message for that note
  if (currentArpNoteIndex != -1) {
    MIDI.sendNoteOff(arpNotes[currentArpNoteIndex], 0, arpChannel);
  }
  
  // Increment the index and loop back to the beginning if at the end of the vector
  currentArpNoteIndex = (currentArpNoteIndex + 1) % arpNotes.size();
  
  // Send a Note On message for the next note
  MIDI.sendNoteOn(arpNotes[currentArpNoteIndex], 100, arpChannel); // 100 is an example velocity
}

void startKick() {
  Serial.println("Kick");
  bool vel = !(velocity_div % 6);
  MIDI.sendNoteOn(KICK_NOTE, 100 + (20 * static_cast<int>(vel)), 10);
  kickStartTime = millis();
  kickNoteOn = true;
  startFlicker(FLICKER_COLOR_A_C, FLICKER_COLOR_B_C, FLICKER_DECAY_TIME_C, FLICKER_START_INDEX_C, FLICKER_END_INDEX_C);
}

void startSnare() {
  Serial.println("Snare");
  bool vel = !(velocity_div % 6);
  MIDI.sendNoteOn(SNARE_NOTE, 100 + (20 * static_cast<int>(vel)), 10);
  snareStartTime = millis();
  snareNoteOn = true;
  startFlicker(FLICKER_COLOR_A_R, FLICKER_COLOR_B_R, FLICKER_DECAY_TIME_R, FLICKER_START_INDEX_R, FLICKER_END_INDEX_R);
}

void endKick() {
  if (kickNoteOn && millis() - kickStartTime >= 300) {
    MIDI.sendNoteOff(KICK_NOTE, 0, 10);
    kickNoteOn = false;
    velocity_div += 1;
  }
}

void endSnare() {
  if (snareNoteOn && millis() - snareStartTime >= 300) {
    MIDI.sendNoteOff(SNARE_NOTE, 0, 10);
    snareNoteOn = false;
    velocity_div += 1;
  }
}

void setup() {
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  MIDI.begin(); // Launch MIDI

  strip.begin();
  strip.show();
  strip.setBrightness(255);

  // Initialize the flicker groups
  for (int i = 0; i < MAX_GROUPS; i++) {
    flickerGroups[i].active = false;
  }
}

unsigned long lastTriggerTimeA0 = 0;
unsigned long lastTriggerTimeA1 = 0;
bool cooldownA0 = false;
bool cooldownA1 = false;
bool firstTriggerA0 = false;
bool firstTriggerA1 = false;

unsigned long lastFlickerUpdateTime = 0;
unsigned long flickerRate = 30; // Flicker update rate in milliseconds

void loop() {
  int a0Val = analogRead(A0);
  int a1Val = analogRead(A1);

  // Handling A0 trigger
  if (!cooldownA0 && a0Val > PIEZO_THRESH) {
    if (!firstTriggerA0) {
      firstTriggerA0 = true;
    } else {
      Serial.printf("Trigger A0: %d\n", a0Val);
      MIDI.sendClock();
      startFlicker(FLICKER_COLOR_A_L, FLICKER_COLOR_B_L, FLICKER_DECAY_TIME_L, FLICKER_START_INDEX_L, FLICKER_END_INDEX_L);
      drip += 1;
      if (!(drip % DRUM_DIVIDER)) {
        startKick();
      }
      if (!(drip % ARP_DIVIDER)) {
        playNextArpNote();
      }
      cooldownA0 = true;
      lastTriggerTimeA0 = millis();
    }
  }

  // Handling A1 trigger
  if (!cooldownA1 && a1Val > PIEZO_THRESH) {
    if (!firstTriggerA1) {
      firstTriggerA1 = true;
    } else {
      Serial.printf("Trigger A1: %d\n", a1Val);
      startSnare();
      cooldownA1 = true;
      lastTriggerTimeA1 = millis();
    }
  }

  // Handling cooldowns
  unsigned long currentTime = millis();
  if (cooldownA0 && currentTime - lastTriggerTimeA0 >= 200) {
    cooldownA0 = false;
    firstTriggerA0 = false;
  }
  if (cooldownA1 && currentTime - lastTriggerTimeA1 >= 200) {
    cooldownA1 = false;
    firstTriggerA1 = false;
  }

  // Check to turn off kick and snare
  endKick();
  endSnare();

  // Update flicker groups at the specified flicker rate
  if (currentTime - lastFlickerUpdateTime >= flickerRate) {
    updateFlickerGroups();
    lastFlickerUpdateTime = currentTime;
  }
}
