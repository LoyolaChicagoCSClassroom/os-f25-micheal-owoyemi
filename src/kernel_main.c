#include <stdint.h>

#define MULTIBOOT2_HEADER_MAGIC         0xe85250d6

// Multiboot header
const unsigned int multiboot_header[]  __attribute__((section(".multiboot"))) = {MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16+MULTIBOOT2_HEADER_MAGIC), 0, 12};

// --- Terminal Driver Constants & Globals ---
volatile uint16_t *vga_buffer = (uint16_t*)0xb8000; // Base address of video mem 
const int VGA_COLS = 80; // [cite: 13]
const int VGA_ROWS = 25;
int term_col = 0;
int term_row = 0;
const uint8_t term_color = 0x07; // Gray text on black background 

// --- External Function Prototypes ---
// Defining the function pointer type used by rprintf [cite: 34]
typedef void (*func_ptr)(int);
// Declaration for the provided esp_printf function
void esp_printf(const func_ptr f_ptr, char *ctrl, ...);


// --- Helper Functions ---

uint8_t inb (uint16_t _port) {
    uint8_t rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
    return rv;
}

// Function to scroll the terminal up by one line [cite: 54]
void terminal_scroll() {
    // Overwrite the first line with the second, second with third, etc. [cite: 55]
    for (int y = 0; y < VGA_ROWS - 1; y++) {
        for (int x = 0; x < VGA_COLS; x++) {
            const int current_index = y * VGA_COLS + x;
            const int next_line_index = (y + 1) * VGA_COLS + x;
            vga_buffer[current_index] = vga_buffer[next_line_index];
        }
    }

    // Clear the last line
    for (int x = 0; x < VGA_COLS; x++) {
        const int index = (VGA_ROWS - 1) * VGA_COLS + x;
        vga_buffer[index] = ((uint16_t)term_color << 8) | ' ';
    }
}

// The required putc function 
void putc(int data) {
    // Handle control characters
    // esp_printf handles \n by sending \r (0x0D) then \n (0x0A)
    
    if (data == '\n') {
        term_row++;
    } 
    else if (data == '\r') {
        term_col = 0;
    } 
    else {
        // Calculate offset in video memory 
        const int index = term_row * VGA_COLS + term_col;
        
        // Write the character and color to video memory 
        // Upper 8 bits: Color, Lower 8 bits: ASCII character
        vga_buffer[index] = ((uint16_t)term_color << 8) | (unsigned char)data;
        
        term_col++;

        // Handle line wrapping if we hit the edge of the screen
        if (term_col >= VGA_COLS) {
            term_col = 0;
            term_row++;
        }
    }

    // Handle scrolling if we have exceeded the screen height [cite: 54]
    if (term_row >= VGA_ROWS) {
        terminal_scroll();
        term_row = VGA_ROWS - 1; // Reset row to the last line
    }
}

void main() {
    // 1. Test the driver by printing the execution level 
    // We assume execution level 0 (Kernel/Ring 0) for this context.
    esp_printf(putc, "Kernel Booted successfully.\nCurrent Execution Level: %d\n", 0);

    // Demonstration of scrolling (optional, but good for testing)
    // esp_printf(putc, "Printing many lines to test scrolling...\n");
    // for(int i = 0; i < 30; i++) {
    //     esp_printf(putc, "Line number: %d\n", i);
    // }

    while(1) {
        uint8_t status = inb(0x64);

        if(status & 1) {
            uint8_t scancode = inb(0x60);
            // Optional: You could print the scancode to test keyboard input
            // esp_printf(putc, "Keyboard scancode: %x\n", scancode);
        }
    }
}