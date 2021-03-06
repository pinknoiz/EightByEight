/*
 * DMA Matrix Driver
 * 
 * Copyright (c) 2014, 2016 Blinkinlabs, LLC
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>
#include "pins_arduino.h"
#include "eightbyeight.h"

//Display Geometry
#define PWM_BITS 12 // Number of physical steps in the inner PWM cycle
#define PAGED_BITS 2 // Number of simulated (dithered) steps in the outer PWM cycle

#define PAGES 4 // Number of pages that the paged bits are expanded into

#define BIT_DEPTH (PWM_BITS + PAGED_BITS)

// RGB pixel type
class Pixel {
public:
    Pixel() {
        R = 0;
        G = 0;
        B = 0;
    }

    Pixel(uint8_t r, uint8_t g, uint8_t b) {
        R = r;
        G = g;
        B = b;
    }

    uint8_t R;
    uint8_t G;
    uint8_t B;
};

// Big 'ol waveform that is bitbanged onto the GPIO bus by the DMA engine
// There are LED_ROWS separate loops, where the LED matrix address lines
// to be set before they are activated.
// For each of these rows, there are then PWM_BITS separate inner loops
// And each inner loop has LED_COLS * 2 bytes states (the data is LED_COLS long, plus the clock signal is baked in)

#define BITS_PER_COLUMN 24      // Number of bits that need to be written out on each PWM cycle
#define BITS_PER_WRITE  12      // Number of bits written out on each SPI transfer

// Number of SPI transactions per PWM cycle
#define WRITES_PER_COLUMN (BITS_PER_COLUMN/BITS_PER_WRITE)

// Number of bytes required to store a single row of full-color data output, in SPI mode
#define ROW_DEPTH_SIZE (WRITES_PER_COLUMN*PWM_BITS)

// Number of bytes required to store an entire panel's worth of data output, in SPI mode
#define PANEL_DEPTH_SIZE (ROW_DEPTH_SIZE*LED_ROWS)

class Matrix {
public:
    Matrix();

    // Enable the matrix
    void begin();

    // Change the system brightness
    // @param brightness Display brightness scale, from 0 (off) to 1 (fully on)
    void setBrightness(float brightness);

    // Get the brightness of the matrix
    float getBrightness() const;

    // Update a single pixel in the array
    // @param column int pixel column (0 to led_cols - 1)
    // @param row int pixel row (0 to led_rows - 1)
    // @param r uint8_t new red value for the pixel (0 - 255)
    // @param g uint8_t new green value for the pixel (0 - 255)
    // @param b uint8_t new blue value for the pixel (0 - 255)
    void setPixelColor(uint8_t column, uint8_t row, uint8_t r, uint8_t g, uint8_t b);

    // Update a single pixel in the array
    // @param column int pixel column (0 to led_cols - 1)
    // @param row int pixel row (0 to led_rows - 1)
    // @param pixel Pixel New pixel color
    void setPixelColor(uint8_t column, uint8_t row, const Pixel& pixel);

    // Get the display pixel buffer
    // @return Pointer to the pixel display buffer, a uint8_t array of size
    // LED_ROWS*LED_COLS
    Pixel* getPixels();

    // Update the matrix using the data in the Pixels[] array
    void show();

    // ISR loop. TODO: make this friends only?
    void refresh();

private:
    float brightness;

    typedef uint16_t dmaBuffer_t;
    dmaBuffer_t* frontBuffer;
    dmaBuffer_t* backBuffer;

    // Display memory
    // These are stored as RGB triplets, and are what the user writes into
    Pixel pixels[LED_ROWS * LED_COLS];

    // Data output bitstream
    // This is the bitstream that the DMA engine writes to the GPIO port
    // connected to the current-controlled shift registers, in order to create
    // the PWM waveforms to actually drive the display output.
    // TODO: Trigger interrupt from last address and skip the extra data transfer?
    dmaBuffer_t dmaBuffer[2][PAGES*PANEL_DEPTH_SIZE];

    // Address output bitstream
    // This is the bitstream that the DMA engine writes out to the GPIO port
    // connected to the address select mux, in order to enable the output rows
    // sequentially.
    uint8_t Addresses[PWM_BITS*LED_ROWS+1];

    // Timer output buffers (these are DMAd to the FTM0_MOD and FTM0_C1V registers)
    uint16_t FTM0_MODStates[PWM_BITS*LED_ROWS + 1];
    uint16_t FTM0_C1VStates[PWM_BITS*LED_ROWS + 1];

    volatile bool swapBuffers;
    uint8_t currentPage;

    // The display is double-buffered internally. This function returns
    // true if there is already an update waiting.
    bool bufferWaiting() const;

    void setupTCD0();
    void setupTCD1();
    void setupTCD2();
    void setupTCD3();

    void armTCD0(void* source, int majorLoops);
    void armTCD1(void* source, int majorLoops);
    void armTCD2(void* source, int majorLoops);
    void armTCD3(void* source, int majorLoops);

    void setupSPI0();
    void setupFTM0();

    void buildAddressTable();
    void buildTimerTables();

    void pixelsToDmaBuffer(Pixel* pixelInput, dmaBuffer_t buffer[]);
};

extern Matrix matrix;

#endif
