#include "hardware/spi.h"
#include "pico/binary_info.h"

#define SPI_CLK  2
#define SPI_CS   5
#define SPI_MOSI 4
#define SPI_MISO 3

uint8_t out_buf[10], in_buf[10];
int countread = 0;

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin (57600);

  spi_init(spi_default, 500 * 1000);
    spi_set_slave(spi_default, true);
    gpio_set_function(SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SPI_CLK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO, GPIO_FUNC_SPI);
    gpio_set_function(SPI_CS, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    bi_decl(bi_4pins_with_func(SPI_MOSI, SPI_MISO, SPI_CLK, SPI_CS, GPIO_FUNC_SPI));

  spi_set_slave(spi_default, true);
}

// the loop function runs over and over again forever
void loop() {
  out_buf[0] = countread%256;
  out_buf[1] = countread/256;
  if ( spi_is_readable(spi_default) )
  {
     countread++;
     spi_write_read_blocking(spi_default, out_buf, in_buf, 2);
     int val = (int)in_buf[0] + 256*(int)in_buf[1];
     Serial.print(val);
     Serial.print(" ");
     Serial.print(in_buf[0]);
     Serial.print(",");
     Serial.println(in_buf[1]);
  }
  else
  {
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(100);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(100);
  }
}
