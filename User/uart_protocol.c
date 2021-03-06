/** @file
    @brief Implementation

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
// Copyright 2015-2016 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
*/

/* Internal Includes */
#include "uart_protocol.h"
#include "main.h"
#include "array_init.h"

/* Library/third-party includes */
#include "stm8s.h"

/* Standard includes */
/* - none - */

#ifdef ENABLE_UART

#define ARRAY_ATTRIBUTE NEAR
typedef uint8_t const ARRAY_ATTRIBUTE *ConstCharPtr;

enum
{
  UART_COMMAND_NONE       = 0,
  UART_COMMAND_FLASH      = 'F',
  UART_COMMAND_BLANK      = 'B',
  UART_COMMAND_INTERVAL   = 'I',
  UART_COMMAND_SIMULATION = 'S',
  UART_COMMAND_PATTERN    = 'P',
  UART_COMMAND_ERROR      = 'E',
  UART_COMMAND_HELP       = 'H',
};

enum
{
  UART_CHARACTER_DELIMITER = ':',
  UART_CHARACTER_EOL       = '\r',
  UART_CHARACTER_NEWLINE   = '\n',
  UART_CHARACTER_COMMA     = ',',
};

enum
{
  UART_MODE_READ  = 'R',
  UART_MODE_WRITE = 'W',
};

// FW:10000\n\r
// FR\n\r
// PW:A:00,01,02,03,04

#define UART_MAX_LINE_LENGTH 32
// UART_COMMAND _protocol_data = {0};
ARRAY_ATTRIBUTE uint8_t _protocol_line[UART_MAX_LINE_LENGTH];
uint8_t _protocol_length = 0;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define UART_MAX_WRITE_LENGTH 256
#define UART_MAX_WRITE_LENGTH_MASK (UART_MAX_WRITE_LENGTH - 1)
typedef ARRAY_ATTRIBUTE struct UART_WRITE_BUFFER_
{
  uint8_t buffer[UART_MAX_WRITE_LENGTH];
  uint8_t count;
  uint8_t write;
  uint8_t read;
} UART_WRITE_BUFFER;

UART_WRITE_BUFFER _write_buffer = {0};

void protocol_init()
{
  _write_buffer.count = 0;
  _write_buffer.write = 0;
  _write_buffer.read  = 0;

  _protocol_length = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t protocol_is_output_ready() { return _write_buffer.count > 0; }

uint8_t protocol_get_output_byte()
{
  uint8_t ch = _write_buffer.buffer[_write_buffer.read++];
  _write_buffer.read &= UART_MAX_WRITE_LENGTH_MASK;
  _write_buffer.count--;
  return ch;
}

void protocol_put_output_byte(uint8_t ch)
{
  _write_buffer.buffer[_write_buffer.write++] = ch;
  _write_buffer.write &= UART_MAX_WRITE_LENGTH_MASK;
  _write_buffer.count++;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void protocol_parse_flash_read();
void protocol_parse_flash_write();

void protocol_parse_blank_read();
void protocol_parse_blank_write();

void protocol_parse_interval_read();
void protocol_parse_interval_write();

void protocol_parse_sim_read();
void protocol_parse_sim_write();

void protocol_parse_pattern_read();
void protocol_parse_pattern_write();

void protocol_output_error(uint8_t *info, uint8_t info_length);

void protocol_help();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void protocol_put_input_byte(uint8_t ch)
{
  protocol_put_output_byte(ch);

  if (_protocol_length < UART_MAX_LINE_LENGTH)
    _protocol_line[_protocol_length++] = ch;

  if (ch != UART_CHARACTER_EOL)
    return;

  if (_protocol_length < 2)
  {
    _protocol_length = 0;
    return;
  }

  uint8_t read;
  switch (_protocol_line[1])
  {
  case UART_MODE_READ:
    read = TRUE;
    break;

  case UART_MODE_WRITE:
    read = FALSE;
    break;

  default:
    protocol_output_error("mode", 4);
    _protocol_length = 0;
    return;
  }

  switch (_protocol_line[0])
  {
  case UART_COMMAND_HELP:
    protocol_help();
    break;
  case UART_COMMAND_FLASH:
    if (read)
      protocol_parse_flash_read();
    else
      protocol_parse_flash_write();
    break;
  case UART_COMMAND_BLANK:
    if (read)
      protocol_parse_blank_read();
    else
      protocol_parse_blank_write();
    break;
  case UART_COMMAND_INTERVAL:
    if (read)
      protocol_parse_interval_read();
    else
      protocol_parse_interval_write();
    break;
  case UART_COMMAND_SIMULATION:
    if (read)
      protocol_parse_sim_read();
    else
      protocol_parse_sim_write();
    break;
  case UART_COMMAND_PATTERN:
    if (read)
      protocol_parse_pattern_read();
    else
      protocol_parse_pattern_write();
    break;
  default:
    protocol_output_error("command", 7);
    _protocol_length = 0;
    return;
  }

  _protocol_length = 0;
}

static void protocol_put_hex_nibble(uint8_t nibble)
{
  nibble &= 0x0F;
  protocol_put_output_byte(nibble < 0x0A ? ('0' + nibble) : ('A' - 0x0A + nibble));
}

static void protocol_put_hex_uint8(uint8_t val)
{
  protocol_put_hex_nibble(val >> 4);
  protocol_put_hex_nibble(val);
}

static void protocol_put_hex_uint16(uint16_t val)
{
  protocol_put_hex_nibble((uint8_t)(val >> 12));
  protocol_put_hex_nibble((uint8_t)(val >> 8));
  protocol_put_hex_nibble((uint8_t)(val >> 4));
  protocol_put_hex_nibble((uint8_t)(val));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void protocol_output_error(uint8_t *info, uint8_t info_length)
{
  uint8_t space_available = UART_MAX_WRITE_LENGTH - _write_buffer.count;

  // if overflow
  if (space_available < info_length + 3) // "E:xxxxx\n"
    return;

  protocol_put_output_byte(UART_COMMAND_ERROR);
  protocol_put_output_byte(UART_CHARACTER_DELIMITER);

  while (info_length--)
    protocol_put_output_byte(*info++);

  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);
}

void protocol_output_string(uint8_t *info, uint8_t info_length)
{
  uint8_t space_available = UART_MAX_WRITE_LENGTH - _write_buffer.count;

  // if overflow
  if (space_available < info_length) // "E:xxxxx\n"
    return;

  while (info_length--)
    protocol_put_output_byte(*info++);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t hex_to_int(uint8_t ch)
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 0x0a;
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 0x0a;
  return U8_MAX;
}

#define PROTO_MAX_HEX_DIGIT_VAL 0x0f

/// Parses two hex digits from a string
static bool parseHexUint8(ConstCharPtr str, uint8_t *out)
{
  if (!out || !str)
  {
    return FALSE;
  }
  uint8_t value_hi = hex_to_int(str[0]);
  uint8_t value_lo = hex_to_int(str[1]);
  if (value_lo > PROTO_MAX_HEX_DIGIT_VAL || value_hi > PROTO_MAX_HEX_DIGIT_VAL)
  {
    protocol_output_error("value", 5);
    return FALSE;
  }
  *out = value_lo | (value_hi << 4);
  return TRUE;
}

/// Parses four hex digits from a string
static bool parseHexUint16(ConstCharPtr str, uint16_t *out)
{
  if (!out || !str)
  {
    return FALSE;
  }
  uint8_t value_12 = hex_to_int(str[0]);
  uint8_t value_08 = hex_to_int(str[1]);
  uint8_t value_04 = hex_to_int(str[2]);
  uint8_t value_00 = hex_to_int(str[3]);
  if (value_12 > PROTO_MAX_HEX_DIGIT_VAL || value_08 > PROTO_MAX_HEX_DIGIT_VAL || value_04 > PROTO_MAX_HEX_DIGIT_VAL ||
      value_00 > PROTO_MAX_HEX_DIGIT_VAL)
  {
    protocol_output_error("value", 5);
    return FALSE;
  }

  *out = (((uint16_t)value_12) << 12) | (((uint16_t)value_08) << 8) | (((uint16_t)value_04) << 4) |
         (((uint16_t)value_00) << 0);
  return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void protocol_parse_flash_write()
{
  if (_protocol_line[2] != UART_CHARACTER_DELIMITER)
  {
    protocol_output_error("delimiter", 9);
    return;
  }

  uint16_t value;
  if (!parseHexUint16(&(_protocol_line[3]), &value))
  {
    return;
  }

  if (value < 10 || value > 10000)
  {
    protocol_output_error("limit", 5);
    return;
  }

  set_flash_period(value);
  protocol_parse_flash_read();
}

void protocol_parse_flash_read()
{
  uint8_t space_available = UART_MAX_WRITE_LENGTH - _write_buffer.count;

  // if overflow
  if (space_available < 8) // "FR:0010\n"
    return;

  protocol_put_output_byte(UART_COMMAND_FLASH);
  protocol_put_output_byte(UART_MODE_READ);
  protocol_put_output_byte(UART_CHARACTER_DELIMITER);
  uint16_t period = get_flash_period();
  protocol_put_hex_uint16(period);
  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void protocol_parse_blank_write()
{
  if (_protocol_line[2] != UART_CHARACTER_DELIMITER)
  {
    protocol_output_error("delimiter", 9);
    return;
  }
  uint16_t value;
  if (!parseHexUint16(&(_protocol_line[3]), &value))
  {
    return;
  }

  if (value < 10 || value > 10000)
  {
    protocol_output_error("limit", 5);
    return;
  }

  set_blank_period(value);

  protocol_parse_blank_read();
}

void protocol_parse_blank_read()
{
  uint8_t space_available = UART_MAX_WRITE_LENGTH - _write_buffer.count;

  // if overflow
  if (space_available < 8) // "FR:0010\n"
    return;

  protocol_put_output_byte(UART_COMMAND_BLANK);
  protocol_put_output_byte(UART_MODE_READ);
  protocol_put_output_byte(UART_CHARACTER_DELIMITER);
  uint16_t period = get_blank_period();
  protocol_put_hex_uint16(period);
  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void protocol_parse_interval_write()
{
  if (_protocol_line[2] != UART_CHARACTER_DELIMITER)
  {
    protocol_output_error("delimiter", 9);
    return;
  }

  uint16_t value;
  if (!parseHexUint16(&(_protocol_line[3]), &value))
  {
    return;
  }

  if (value < 10 || value > 10000)
  {
    protocol_output_error("limit", 5);
    return;
  }

  set_interval_period(value);

  protocol_parse_interval_read();
}

void protocol_parse_interval_read()
{
  uint8_t space_available = UART_MAX_WRITE_LENGTH - _write_buffer.count;

  // if overflow
  if (space_available < 8) // "FR:0010\n"
    return;

  protocol_put_output_byte(UART_COMMAND_INTERVAL);
  protocol_put_output_byte(UART_MODE_READ);
  protocol_put_output_byte(UART_CHARACTER_DELIMITER);
  uint16_t period = get_interval_period();
  protocol_put_hex_uint16(period);
  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void protocol_parse_sim_write()
{
  if (_protocol_line[2] != UART_CHARACTER_DELIMITER)
  {
    protocol_output_error("delimiter", 9);
    return;
  }

  uint8_t value;
  if (!parseHexUint8(&(_protocol_line[3]), &value))
  {
    return;
  }

  if (value < 50)
  {
    protocol_output_error("limit", 5);
    return;
  }

  set_interval_simulator(value);

  protocol_parse_sim_read();
}

void protocol_parse_sim_read()
{
  uint8_t space_available = UART_MAX_WRITE_LENGTH - _write_buffer.count;

  // if overflow
  if (space_available < 6) // "SR:10\n"
    return;

  protocol_put_output_byte(UART_COMMAND_SIMULATION);
  protocol_put_output_byte(UART_MODE_READ);
  protocol_put_output_byte(UART_CHARACTER_DELIMITER);
  uint8_t period = get_simulation_period();
  protocol_put_hex_uint8(period);
  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void protocol_parse_pattern_write()
{
  if (_protocol_line[2] != UART_CHARACTER_DELIMITER || _protocol_line[4] != UART_CHARACTER_DELIMITER)
  {
    protocol_output_error("delimiter", 9);
    return;
  }

  uint8_t index = hex_to_int(_protocol_line[3]);
  if (index > PROTO_MAX_HEX_DIGIT_VAL)
  {
    protocol_output_error("index", 9);
    return;
  }
  uint8_t i, j;
  for (i = 0, j = 0; i < LED_LINE_LENGTH; i++)
  {

    uint8_t value;
    bool parseSuccess = parseHexUint8(&(_protocol_line[5 + j]), &value);
    j += 2;
    if (_protocol_line[5 + j++] != UART_CHARACTER_COMMA)
    {
      protocol_output_error("comma", 5);
      return;
    }
    if (!parseSuccess)
    {
      protocol_output_error("value", 5);
      return;
    }

    pattern_array[index][i] = value;
  }

  line_array_init(index, pattern_array[index]);

  protocol_parse_pattern_read();
}

void protocol_parse_pattern_read()
{
  uint8_t space_available = UART_MAX_WRITE_LENGTH - _write_buffer.count;

  if (_protocol_line[2] != UART_CHARACTER_DELIMITER)
  {
    protocol_output_error("delimiter", 9);
    return;
  }

  uint8_t index = hex_to_int(_protocol_line[3]);
  if (index > PROTO_MAX_HEX_DIGIT_VAL)
  {
    protocol_output_error("index", 5);
    return;
  }

  // if overflow
  if (space_available < 20) // "PR:1:00,01,02,03,04\n"
    return;

  protocol_put_output_byte(UART_COMMAND_PATTERN);
  protocol_put_output_byte(UART_MODE_READ);
  protocol_put_output_byte(UART_CHARACTER_DELIMITER);
  protocol_put_hex_nibble(index);
  protocol_put_output_byte(UART_CHARACTER_DELIMITER);
  uint8_t i;
  for (i = 0; i < LED_LINE_LENGTH; i++)
  {
    protocol_put_hex_uint8(pattern_array[index][i]);
    protocol_put_output_byte(UART_CHARACTER_COMMA);
  }
  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void protocol_help()
{
  protocol_output_string("HW: FR/FW-flash period", 22);
  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);

  protocol_output_string("HW: BR/BW-blank period", 22);
  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);

  protocol_output_string("HW: IR/IW-interval period", 25);
  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);

  protocol_output_string("HW: SR/SW-simulation period", 27);
  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);

  protocol_output_string("HW: PR/PW-pattern", 17);
  protocol_put_output_byte(UART_CHARACTER_EOL);
  protocol_put_output_byte(UART_CHARACTER_NEWLINE);
}
#endif
