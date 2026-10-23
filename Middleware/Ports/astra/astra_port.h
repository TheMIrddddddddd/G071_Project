#pragma once
#include <cstdint>
/* The Astra HAL only draws into a CPU-owned 1024-byte staging frame. It never
 * sends I2C, delays, publishes a frame or scans physical keys. */
bool AstraPort_Init(uint8_t *frame);
void AstraPort_Bind(uint8_t *frame);
