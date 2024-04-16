|   Target Device   | ESP32 |
| ----------------- | ----- |

# NMEA Parser
This is a library for parsing NMEA 0183 sentences. The UART peripheral is used to read the GPS module which is transmitting a sequence of sentences containing GPS information. After reading a line the type of message is identified and then it is decoded further. Let's say if the line received is:
$GPGGA,191644.00,4014.86746,N,06955.59231,E,1,08,1.13,332.7,M,-40.6,M,,*71
First the GPGGA string is used to identify the type and then the respective parser function is called. 

## UART Setup
The UART peripheral uses the following configuration 
```C
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
```
The UART port used is UART_NUM_2. The default pins in ESP IDF are wrong. The actual hardware UART GPIO pins are 
|    UART   | GPIO Number |
|    --------    | -- |
|   UART RX Pin  | 16 |
|   UART TX Pin  | 17 |

The picture below shows the ESP32 used for programming and the GPS Module.
![ESP32_GPS](https://github.com/mirhamza708/NMEA_PARSER/assets/55946600/ebb2042f-14f4-43d7-a9e0-7cc98d49fb63)

## How the program works
There is little code to write in the main.c file. we just need to initialize the uart module like this
```C
    #include "uart.h"

    void app_main(void)
    {
        uart_init();
    }
```
In the UART_init() function we first initialize the UART peripheral by using the functions provided with ESP-IDF. ESP-IDF provides a method to generate an even when a certain character has been received. So we use this event to catch the "\n" character as we know that NMEA protocol sentences end with "\n". Whenever the event occurs we read the buffer and store the data in dtmp("$GPGGA,191644.00,4014.86746,N,06955.59231,E,1,08,1.13,332.7,M,-40.6,M,,*71") and pat("\n") we use the dtmp further to decode the required information such as Latitude, Longitude, Time, etc. 
The task uart_event_task constantly checks for data in the UART queue and this queue is updated internally. Whenever data is received in the UART RX Buffer a corresponding event is provided in the event structure. A switch statement is used to check the event type and if the event type is UART_PATTERN_DET then data is read from the UART RX Buffer.

This case is executed in the uart_event_task when a line feed character is received
```C
    //UART_PATTERN_DET
    case UART_PATTERN_DET:
        uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
        int pos = uart_pattern_pop_pos(EX_UART_NUM);
        ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
        if (pos == -1) {
            // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
            // record the position. We should set a larger queue size.
            // As an example, we directly flush the rx buffer here.
            uart_flush_input(EX_UART_NUM);
        } else {
            uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
            uint8_t pat[PATTERN_CHR_NUM + 1];
            memset(pat, 0, sizeof(pat));
            uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "read data: %s", dtmp);
            ESP_LOGI(TAG, "read pat : %s", pat);
        }
        break;
```

## Sentence Parsing
In the case UART_PATTERN_DET a single NMEA sentence is read. And this sentence is passed into our gps_parse() function to extract the relevent information from it. And this data is stored in the structure type gps_t.

## How parsing works
gps_parse function is called inside the uart_event_task. This function first checks that a valid GPS sentence is received (starts with '$' and have length greater than 3), return if sentence invalid. Secondly the CRC is calculated and compared with the one provided in the sentence if it does not match, the function returns and no calculation is done. After both of the checks passes then the sentence is parsed upto the first comma and based on the sentence type the required function is called. for example if the sentence is GPGGA then parse_gga is called and so on for the rest of the sentences.

In the function parse_gga a while loop is executed untill the end of the sentence. When a comma is detected a temporary sub string is made and the contents between commas are copied into it. item_idx counts the position of the sub string and a switch case statement is called based on the this item_idx. for example if the item_idx is 7 and the sentence type is GPRMC then the case 7 is executed inside the function parse_rmc. In this case 7 the speed data is parsed and stored in the global struct gps. All the parsed data is stored in the gps struct.

There are seperate functions for latitude and longitude parsing, checksum calculation, parsing time and date. The parse_time function is defined as static inline because I wanted the scope of this function to be in the same file and the function is small so made inline to avoid function call overhead.

## Debugging
The parsed data can be printed to the serial port by uncommenting one of these lines in the parser.c file
```C
// #define LOG_LOCAL_LEVEL ESP_LOG_ERROR 
// #define LOG_LOCAL_LEVEL ESP_LOG_INFO
```
If none of these lines are uncommented then the log level will be by default set to ESP_LOG_INFO. Currently only two log levels are implemented in the parser.c file
This is an example of the output on the serial port:
```
I (135690) Parser: $GPGGA,080512.00,3414.86611,N,07155.58886,E,1,05,1.36,353.9,M,-40.6,M,,*77

I (135690) Parser: Time:13:5:12.000000
I (135700) Parser: latitude: 34.247768
I (135700) Parser: longitude: 9.593148
I (135700) Parser: fix: 1
I (135710) Parser: Sats: 5
I (135710) Parser: HDOP 1.360000
I (135720) Parser: altitude: 313.299988
```

