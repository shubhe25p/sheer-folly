#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


#define BUF_SIZE (4096)  // size of circular buffer (one page)


int main() {
   // Create a temporary file to back our shared memory
   int fd = memfd_create("circular_buffer", 0);
   if (fd == -1) {
   	perror("memfd_create");
   	exit(1);
   }
 
   // Set the file size to our buffer size
   if (ftruncate(fd, BUF_SIZE) == -1) {
   	perror("ftruncate");
   	close(fd);
   	exit(1);
   }
 
   // Allocate space for two contiguous pages
   void *addr = mmap(NULL, BUF_SIZE * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   if (addr == MAP_FAILED) {
   	perror("mmap reservation");
   	close(fd);
   	exit(1);
   }
 
   // Map the file twice in the reserved space
   void *buf1 = mmap(addr, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
   if (buf1 == MAP_FAILED) {
   	perror("mmap buf1");
   	munmap(addr, BUF_SIZE * 2);
   	close(fd);
   	exit(1);
   }
 
   void *buf2 = mmap((char*)addr + BUF_SIZE, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
   if (buf2 == MAP_FAILED) {
   	perror("mmap buf2");
   	munmap(addr, BUF_SIZE * 2);
   	close(fd);
   	exit(1);
   }
 
   // Fill the buffer with data
   for (int i = 0; i < BUF_SIZE; i++) {
   	((char*)buf1)[i] = 'A' + (i % 26);
   }
 
   printf("First buffer contents (first 52 chars): ");
   for (int i = 0; i < 52; i++) {
   	putchar(((char*)buf1)[i]);
   }
   printf("\n\n");
 
   printf("Second buffer contents (first 52 chars): ");
   for (int i = 0; i < 52; i++) {
   	putchar(((char*)buf2)[i]);
   }
   printf("\n\n");
 
   // Demonstrate circular reading - show page boundary clearly
   char *read_ptr = (char*)buf1;
   printf("Demonstrating circular buffer - reading across page boundary:\n");
   printf("Reading from position 4090 to 4110 (crosses page boundary at 4096):\n\n");
 
   for (int i = 4090; i < 4111; i++) {
   	// Show exactly where the page boundary is
   	if (i == 4096) {
       	printf("\n");
       	printf("--- PAGE BOUNDARY (4096) ---\n");
       	printf("End of page 1 ↑ | Start of page 2 ↓\n");
       	printf("(Same memory!)   | (Same memory!)\n");
       	printf("--- PAGE BOUNDARY (4096) ---\n");
   	}
 	 
   	printf("pos[%4d]: '%c'", i, read_ptr[i]);
 	 
   	// Show which virtual page we're in
   	if (i < 4096) {
       	printf(" (page 1)\n");
   	} else {
       	printf(" (page 2 - mirrors page 1)\n");
   	}
   }
 
   printf("\nNotice how the characters repeat after position 4095!\n");
   printf("This proves both pages map to the same physical memory.\n");
 
   // Clean up
   close(fd);  // Close the file descriptor
   munmap(addr, BUF_SIZE * 2);  // Unmap the entire region
 
   return 0;
}



