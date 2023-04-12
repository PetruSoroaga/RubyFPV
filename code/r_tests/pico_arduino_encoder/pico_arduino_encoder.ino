#include "hardware/spi.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"

#define SPI_CLK  2
#define SPI_CS   5
#define SPI_MOSI 4
#define SPI_MISO 3

#define PIN_ROTARY_SEL 11
#define PIN_ROTARY_CLK 12
#define PIN_ROTARY_DATA 13


uint8_t out_buf[10], in_buf[10];
int countread = 0;

long lastLedTime = 0;
int ledState = 0;

int lastReadClk = 0;
int lastReadData = 0;

int isPositive = 0;
int count = 0;
queue_t queue_to_core2;

void core2_entry() {

   Serial.println("Core 2 started");
    multicore_fifo_push_blocking(23);

   Serial.println("Core 2 waiting");
   // uint32_t g = multicore_fifo_pop_blocking();

   // if (g != 34)
   //     Serial.println("Hmm, that's not right on core 1!\n");
   // else
   //     Serial.println("Its all gone well on core 1!");

    //while (1)
    //    tight_loop_contents();
    while (1)
    {
      //delay(1900);
      //Serial.println("*");
      int c = 0;
      queue_remove_blocking(&queue_to_core2, &c);
      Serial.print(c);
    }
}


// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_ROTARY_CLK, INPUT_PULLUP);
  pinMode(PIN_ROTARY_DATA, INPUT_PULLUP);
  pinMode(PIN_ROTARY_SEL, INPUT_PULLUP);
  
  Serial.begin (57600);

   for( int i=0; i<8; i++ )
   {
      delay(500);
      Serial.println(i);
   }
  lastLedTime = millis();

  lastReadClk = digitalRead(PIN_ROTARY_CLK);
  lastReadData = digitalRead(PIN_ROTARY_DATA);
  
  spi_init(spi_default, 500 * 1000);
    spi_set_slave(spi_default, true);
    gpio_set_function(SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SPI_CLK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO, GPIO_FUNC_SPI);
    gpio_set_function(SPI_CS, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    bi_decl(bi_4pins_with_func(SPI_MOSI, SPI_MISO, SPI_CLK, SPI_CS, GPIO_FUNC_SPI));

  spi_set_slave(spi_default, true);

 queue_init(&queue_to_core2, sizeof(int), 2000);
 
  Serial.println("Starting second core...");
  multicore_launch_core1(core2_entry);
  Serial.println("Wating for second core value...");
  //uint32_t g = multicore_fifo_pop_blocking();
  //Serial.print("Got from core2: ");
  //Serial.println(g);
  //multicore_fifo_push_blocking(34);
  Serial.println("Multicore done.");
}

// the loop function runs over and over again forever
void loop() 
{
  delay(2);
  long timeNow = millis();
  
  int readClk = digitalRead(PIN_ROTARY_CLK);
  int readData = digitalRead(PIN_ROTARY_DATA);
  
  int readClk2 = digitalRead(PIN_ROTARY_CLK);
  int readData2 = digitalRead(PIN_ROTARY_DATA);

  int readClk3 = digitalRead(PIN_ROTARY_CLK);
  int readData3 = digitalRead(PIN_ROTARY_DATA);

  if ( readClk == readClk2 )
  if ( readData == readData2 )
  if ( readClk == readClk3 )
  if ( readData == readData3 )
  if ( readClk != lastReadClk )
  {
    lastReadClk = readClk;
    if ( readClk == 0 )
    {
    if ( readData != readClk )
    {
        if ( ! isPositive )
           Serial.println("");
          Serial.print("+");
       isPositive = 1;
    }
    else
    {
       if ( isPositive )
          Serial.println("");
       Serial.print("-");
       isPositive = 0;
    }
    }
    count++;
    queue_add_blocking(&queue_to_core2, &count);
  }
  
  /*
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
  */
  if ( timeNow > lastLedTime + 200 )
  {
     lastLedTime = timeNow;
     ledState = 1 - ledState;

     if ( ledState )
        digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
     else
        digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  }
}
