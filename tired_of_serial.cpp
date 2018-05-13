#include <Arduino.h>
#include <avr/pgmspace.h>
#include "tired_of_serial.h"

void print_pgm_string(const char **pgm_str_table, byte index) {
  const char *pgm_str = (const char*) pgm_read_word((pgm_str_table + index));
  for(; pgm_read_byte(pgm_str) !=0; pgm_str++) { Serial.print((char) pgm_read_byte( pgm_str )); };
  }
