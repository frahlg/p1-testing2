#include <Arduino.h>
#include <HardwareSerial.h>

#define LED           7    // Status LED
#define DTR_IO        6    // Data Terminal Ready
#define RXP1         10    // RX pin for P1 port
#define BAUD_RATE 115200   // Baud rate for P1 port

#define MAX_BUFFER_SIZE 1024
#define FRAME_FLAG 0x7E

// --- DLMS/OBIS Defines (From provided headers) ---
#define DATA_NULL                   0x00
#define DATA_OCTET_STRING           0x09
#define DATA_LONG_UNSIGNED          0x12
#define DATA_LONG_DOUBLE_UNSIGNED   0x06

#define OBIS_CODE_LEN 6 // Standard OBIS code length

#define SCALE_TENTHS                0xFF
#define SCALE_HUNDREDTHS            0xFE
#define SCALE_THOUSANDS             0xFD

// Offsets within the frame (assuming structure from example)
#define DECODER_START_OFFSET 20 

// Indices within the 6-byte OBIS code
#define OBIS_A 0
#define OBIS_B 1
#define OBIS_C 2
#define OBIS_D 3
#define OBIS_E 4
#define OBIS_F 5

// Known OBIS C,D patterns
static const uint8_t OBIS_TIMESTAMP[]          = {0x01, 0x00}; // 0-0:1.0.0*255 (Usually OctetString 0x0C)
static const uint8_t OBIS_ACTIVE_ENERGY_PLUS[] = {0x01, 0x08}; // 1-0:1.8.0*255
static const uint8_t OBIS_ACTIVE_ENERGY_MINUS[]= {0x02, 0x08}; // 1-0:2.8.0*255
static const uint8_t OBIS_REACTIVE_ENERGY_PLUS[] = {0x03, 0x08}; // 1-0:3.8.0*255
static const uint8_t OBIS_REACTIVE_ENERGY_MINUS[]= {0x04, 0x08}; // 1-0:4.8.0*255
static const uint8_t OBIS_ACTIVE_POWER_PLUS[]  = {0x01, 0x07}; // 1-0:1.7.0*255
static const uint8_t OBIS_ACTIVE_POWER_MINUS[] = {0x02, 0x07}; // 1-0:2.7.0*255
static const uint8_t OBIS_REACTIVE_POWER_PLUS[]  = {0x03, 0x07}; // 1-0:3.7.0*255
static const uint8_t OBIS_REACTIVE_POWER_MINUS[] = {0x04, 0x07}; // 1-0:4.7.0*255
static const uint8_t OBIS_VOLTAGE_L1[]         = {0x20, 0x07}; // 1-0:32.7.0*255
static const uint8_t OBIS_VOLTAGE_L2[]         = {0x34, 0x07}; // 1-0:52.7.0*255
static const uint8_t OBIS_VOLTAGE_L3[]         = {0x48, 0x07}; // 1-0:72.7.0*255
static const uint8_t OBIS_CURRENT_L1[]         = {0x1F, 0x07}; // 1-0:31.7.0*255
static const uint8_t OBIS_CURRENT_L2[]         = {0x33, 0x07}; // 1-0:51.7.0*255
static const uint8_t OBIS_CURRENT_L3[]         = {0x47, 0x07}; // 1-0:71.7.0*255
// ---------------------------------------------------

HardwareSerial P1Serial(1);
uint8_t buffer[MAX_BUFFER_SIZE];
int bufferIndex = 0;
bool inFrame = false;

// Function Prototypes
uint16_t swap_uint16(uint16_t val);
uint32_t swap_uint32(uint32_t val);
void decodeDLMSFrame(uint8_t* frame, int length);
const char* getObisDescription(const uint8_t* obisCode);

// --- Decoding Logic (Refined with header info, NO DECRYPTION) ---
void decodeDLMSFrame(uint8_t* frame, int length) {
    // --- Variables to store decoded values ---
    float voltageL1 = -1.0, voltageL2 = -1.0, voltageL3 = -1.0;
    float currentL1 = -1.0, currentL2 = -1.0, currentL3 = -1.0;
    float activePowerPlus = -1.0, activePowerMinus = -1.0;
    uint32_t activeEnergyPlus = 0, activeEnergyMinus = 0;
    char timestamp[20] = "";
    bool dataFound = false; // Flag to track if any known OBIS was found
    // ----------------------------------------

    Serial.println("\n--- Debug Decoding Frame ---"); // Indicate debug mode

    int current_position = DECODER_START_OFFSET;

    while (current_position < length - 10) { 
        int loop_start_pos = current_position; // Track position at loop start
        
        if (frame[current_position] == DATA_OCTET_STRING && frame[current_position + 1] == OBIS_CODE_LEN) {
            uint8_t obis_code[OBIS_CODE_LEN];
            memcpy(obis_code, &frame[current_position + 2], OBIS_CODE_LEN);
            const char* description = getObisDescription(obis_code);
            
            Serial.printf("[%04d] Found OBIS: %d-%d:%d.%d.%d*%d (%s)\n", 
                          current_position, 
                          obis_code[OBIS_A], obis_code[OBIS_B], obis_code[OBIS_C],
                          obis_code[OBIS_D], obis_code[OBIS_E], obis_code[OBIS_F],
                          description ? description : "Unknown");

            current_position += 2 + OBIS_CODE_LEN; 

            if (current_position < length) {
                uint8_t data_type = frame[current_position++];
                uint8_t data_length = 0;
                float float_value = 0.0;
                uint32_t uint32_value = 0;
                uint16_t uint16_value = 0;

                bool known_obis = true; // Assume known until proven otherwise

                Serial.printf("  [%04d] Data Type: 0x%02X\n", current_position -1, data_type);

                switch (data_type) {
                    case DATA_LONG_DOUBLE_UNSIGNED: // 0x06
                        data_length = 4;
                        if (current_position + data_length <= length) {
                            memcpy(&uint32_value, &frame[current_position], data_length);
                            uint32_value = swap_uint32(uint32_value);
                            Serial.printf("    Raw Val: %lu\n", uint32_value);
                            // Assign based on OBIS code
                            if (memcmp(&obis_code[OBIS_C], OBIS_ACTIVE_ENERGY_PLUS, 2) == 0) activeEnergyPlus = uint32_value;
                            else if (memcmp(&obis_code[OBIS_C], OBIS_ACTIVE_ENERGY_MINUS, 2) == 0) activeEnergyMinus = uint32_value;
                            // Add other LongDoubleUnsigned OBIS codes here (e.g., Reactive Energy)
                            else known_obis = false;
                        } else { Serial.println("    Error: Not enough data"); data_length = 0; known_obis = false; }
                        break;

                    case DATA_LONG_UNSIGNED: // 0x12
                        data_length = 2;
                        if (current_position + data_length <= length) {
                            memcpy(&uint16_value, &frame[current_position], data_length);
                            uint16_value = swap_uint16(uint16_value);
                            float_value = uint16_value;
                            
                            if (current_position + 3 < length) { 
                                uint8_t scaler = frame[current_position + 3];
                                if (scaler == SCALE_TENTHS) float_value /= 10.0;
                                else if (scaler == SCALE_HUNDREDTHS) float_value /= 100.0;
                                else if (scaler == SCALE_THOUSANDS) float_value /= 1000.0;
                            }
                            Serial.printf("    Raw Val: %u, Scaled Val: %.3f\n", uint16_value, float_value);
                            // Assign based on OBIS code
                            if (memcmp(&obis_code[OBIS_C], OBIS_VOLTAGE_L1, 2) == 0) voltageL1 = float_value;
                            else if (memcmp(&obis_code[OBIS_C], OBIS_VOLTAGE_L2, 2) == 0) voltageL2 = float_value;
                            else if (memcmp(&obis_code[OBIS_C], OBIS_VOLTAGE_L3, 2) == 0) voltageL3 = float_value;
                            else if (memcmp(&obis_code[OBIS_C], OBIS_CURRENT_L1, 2) == 0) currentL1 = float_value;
                            else if (memcmp(&obis_code[OBIS_C], OBIS_CURRENT_L2, 2) == 0) currentL2 = float_value;
                            else if (memcmp(&obis_code[OBIS_C], OBIS_CURRENT_L3, 2) == 0) currentL3 = float_value;
                            else if (memcmp(&obis_code[OBIS_C], OBIS_ACTIVE_POWER_PLUS, 2) == 0) activePowerPlus = float_value;
                            else if (memcmp(&obis_code[OBIS_C], OBIS_ACTIVE_POWER_MINUS, 2) == 0) activePowerMinus = float_value;
                            // Add other LongUnsigned OBIS codes here (e.g., Reactive Power)
                             else known_obis = false;
                        } else { Serial.println("    Error: Not enough data"); data_length = 0; known_obis = false; }
                        break;

                    case DATA_OCTET_STRING: // 0x09
                        if (current_position < length) {
                            data_length = frame[current_position++];
                            Serial.printf("    String Length: %d\n", data_length);
                            if (current_position + data_length <= length) {
                                if (data_length == 12 && obis_code[OBIS_A]==0 && obis_code[OBIS_B]==0 && obis_code[OBIS_C]==1 && obis_code[OBIS_D]==0) {
                                     uint16_t year = swap_uint16(*(uint16_t*)&frame[current_position]);
                                     uint8_t month = frame[current_position+2];
                                     uint8_t day = frame[current_position+3];
                                     uint8_t hour = frame[current_position+5];
                                     uint8_t minute = frame[current_position+6];
                                     uint8_t second = frame[current_position+7];
                                     snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02u %02u:%02u:%02u", year, month, day, hour, minute, second);
                                } else {
                                    // Generic Octet String - do nothing for summary
                                    known_obis = false;
                                }
                                if (!(data_length == 12 && obis_code[OBIS_A]==0 && obis_code[OBIS_B]==0 && obis_code[OBIS_C]==1 && obis_code[OBIS_D]==0)) {
                                     Serial.print("    Raw Val (Hex): ");
                                     for(int k=0; k<data_length && k<8; k++) { // Print max 8 bytes
                                        if(frame[current_position+k]<0x10) Serial.print("0"); 
                                        Serial.print(frame[current_position+k], HEX); Serial.print(" ");
                                     }
                                     if(data_length > 8) Serial.print("...");
                                     Serial.println();
                                }
                            } else { Serial.println("    Error: Not enough data"); data_length = 0; known_obis = false; }
                        } else { Serial.println("    Error: No length byte"); data_length = 0; known_obis = false; }
                        break;

                    case DATA_NULL: // 0x00
                        data_length = 0;
                        // NULL data doesn't contribute to summary
                        known_obis = false;
                        break;
                        
                    default:
                        // Unknown type, skip rest of frame potentially
                        current_position = length; 
                        data_length = 0;
                        known_obis = false;
                        break;
                }
                current_position += data_length;
                if (known_obis) dataFound = true; // Mark that we found at least one piece of data

                Serial.printf("  Processed block, next pos: %d\n", current_position);

                // Skip potential pause/separator bytes after data?
                if(current_position < length - 1 && (frame[current_position] == 0x02 || frame[current_position] == 0x0F)) {
                   Serial.printf("  Skipping separator bytes at %d (0x%02X 0x%02X)\n", current_position, frame[current_position], frame[current_position+1]);
                   current_position += 2; 
                }

            } else {
                Serial.printf("[%04d] Error: No data type found after OBIS code\n", current_position);
                current_position = length; // Stop parsing
            }
        } else {
             // Did not find OBIS structure, advance byte by byte
             // Add a check to prevent infinite loops if stuck
            if (current_position == loop_start_pos) { 
                 current_position++; // Ensure progress if stuck
            }
            // If advancing normally, loop continues from current_position set inside if/else blocks
        }
    }
    
    // --- Print Summary --- 
    if (dataFound) { // Only print if we actually decoded something
        Serial.println("--- Frame Summary ---");
        if (strlen(timestamp) > 0) {
            Serial.printf("Timestamp: %s\n", timestamp);
        }
        if (voltageL1 >= 0 || voltageL2 >= 0 || voltageL3 >= 0) {
            Serial.printf("Voltage (L1/L2/L3): %.1f V / %.1f V / %.1f V\n", voltageL1, voltageL2, voltageL3);
        }
        if (currentL1 >= 0 || currentL2 >= 0 || currentL3 >= 0) {
            Serial.printf("Current (L1/L2/L3): %.2f A / %.2f A / %.2f A\n", currentL1, currentL2, currentL3);
        }
        if (activePowerPlus >= 0 || activePowerMinus >= 0) {
            Serial.printf("Active Power (+/-): %.3f kW / %.3f kW\n", activePowerPlus, activePowerMinus);
        }
         if (activeEnergyPlus > 0 || activeEnergyMinus > 0) {
            // Assuming Wh, convert to kWh for display
            Serial.printf("Active Energy (+/-): %.3f kWh / %.3f kWh\n", activeEnergyPlus / 1000.0, activeEnergyMinus / 1000.0);
        }
        Serial.println("---------------------");
    }
}

// Helper to get description based on OBIS C,D bytes
const char* getObisDescription(const uint8_t* obisCode) {
    if (memcmp(&obisCode[OBIS_C], OBIS_TIMESTAMP, 2) == 0) return "Timestamp";
    if (memcmp(&obisCode[OBIS_C], OBIS_ACTIVE_ENERGY_PLUS, 2) == 0) return "Active Energy (+)";
    if (memcmp(&obisCode[OBIS_C], OBIS_ACTIVE_ENERGY_MINUS, 2) == 0) return "Active Energy (-)";
    if (memcmp(&obisCode[OBIS_C], OBIS_REACTIVE_ENERGY_PLUS, 2) == 0) return "Reactive Energy (+)";
    if (memcmp(&obisCode[OBIS_C], OBIS_REACTIVE_ENERGY_MINUS, 2) == 0) return "Reactive Energy (-)";
    if (memcmp(&obisCode[OBIS_C], OBIS_ACTIVE_POWER_PLUS, 2) == 0) return "Active Power (+)";
    if (memcmp(&obisCode[OBIS_C], OBIS_ACTIVE_POWER_MINUS, 2) == 0) return "Active Power (-)";
    if (memcmp(&obisCode[OBIS_C], OBIS_REACTIVE_POWER_PLUS, 2) == 0) return "Reactive Power (+)";
    if (memcmp(&obisCode[OBIS_C], OBIS_REACTIVE_POWER_MINUS, 2) == 0) return "Reactive Power (-)";
    if (memcmp(&obisCode[OBIS_C], OBIS_VOLTAGE_L1, 2) == 0) return "Voltage L1";
    if (memcmp(&obisCode[OBIS_C], OBIS_VOLTAGE_L2, 2) == 0) return "Voltage L2";
    if (memcmp(&obisCode[OBIS_C], OBIS_VOLTAGE_L3, 2) == 0) return "Voltage L3";
    if (memcmp(&obisCode[OBIS_C], OBIS_CURRENT_L1, 2) == 0) return "Current L1";
    if (memcmp(&obisCode[OBIS_C], OBIS_CURRENT_L2, 2) == 0) return "Current L2";
    if (memcmp(&obisCode[OBIS_C], OBIS_CURRENT_L3, 2) == 0) return "Current L3";
    return nullptr; // Unknown
}
// --------------------------------------------------------------------

void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH); // Quick blink on startup
  delay(500);
  digitalWrite(LED, LOW);
  
  Serial.begin(BAUD_RATE);
  delay(1000);
  Serial.println("\nAidon P1 Reader Started");
  
  pinMode(DTR_IO, OUTPUT);
  digitalWrite(DTR_IO, HIGH);
  
  P1Serial.begin(BAUD_RATE, SERIAL_8N1, RXP1, -1, true); // Inversion ON
  Serial.println("Listening for P1 data...");
}

void loop() {
  while (P1Serial.available()) {
    uint8_t inByte = P1Serial.read();
    
    if (inByte == FRAME_FLAG) {
      if (inFrame && bufferIndex > 0) {
        decodeDLMSFrame(buffer, bufferIndex);
      }
      bufferIndex = 0;
      inFrame = true;
    } else if (inFrame) {
      if (bufferIndex < MAX_BUFFER_SIZE) {
        buffer[bufferIndex++] = inByte;
      } else {
        Serial.println("Error: Frame buffer overflow!");
        inFrame = false;
        bufferIndex = 0;
      }
    }
  }
  
  delay(10);
}

// --- Swap functions (from dlms_decoder.cpp) ---
uint16_t swap_uint16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

uint32_t swap_uint32(uint32_t val) {
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
}
// ------------------------------------------------- 