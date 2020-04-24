/*
* 
*
* - TLB:
*   - https://www.geeksforgeeks.org/translation-lookaside-buffer-tlb-in-paging/
*
* - FILE:
*   - https://www.tutorialspoint.com/c_standard_library/c_function_fgets.htm
*   - https://www.geeksforgeeks.org/input-output-system-calls-c-create-open-close-read-write/
*
* - MMAP:
*   - https://www.poftut.com/mmap-tutorial-with-examples-in-c-and-cpp-programming-languages/
*   - http://man7.org/linux/man-pages/man2/mmap.2.html
*   - https://www.tutorialspoint.com/unix_system_calls/mmap.htm
*   - https://stackoverflow.com/questions/26582920/mmap-memcpy-to-copy-file-from-a-to-b
*/

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

//  ============================================================================
//  CONSTANTS
#define PAGE_TABLE_SIZE 256 //  size of page table
#define PAGE_SIZE 256 //  size of each page
#define TLB_ENTRS 16  //  number of TLB entries
#define FRAME_SIZE 256  //  size of each frame
#define FRAMES 256  //  number of frames
#define MEM_SIZE 65536  //  size of physical memory
#define SIZE 10  //  size for string buffer

//  ============================================================================
//  GLOBAL VARIABLES
char buffer[SIZE];  // buffer to read each line from the address file
int backingStore; //  Backing Store File int
signed char* backingStoreMap;  //  backing store mmap
FILE* addressFile;  //  Address File pointer
int pageTable[PAGE_TABLE_SIZE]; //  create a pagetable
signed char mainMem[MEM_SIZE]; //  create physical memory
int pageFaults = 0; //  keep track of how many page faults there are

//  ============================================================================
//  MAIN PROGRAM
int main(int argc, char const *argv[])
{
  //  ==========================================================================
  //  ERROR CHECK

  //  Not enough arguments
  if(argc != 3)
  {
    printf("Please include the backing store and address file respectfully as an argument.\n");
    exit(EXIT_FAILURE);
  }

  //  Check if backing store file can be opened
  if((backingStore = open(argv[1], O_RDONLY)) < 0)
  {
    printf("Unable to open backing store file.\n");
    exit(EXIT_FAILURE);
  }

  //  Check if address file can be opened
  if((addressFile = fopen(argv[2], "r")) == NULL)
  {
    printf("Unable to open address file.\n");
    exit(EXIT_FAILURE);
  }

  //  ==========================================================================
  //  VIRTUAL ADDRESS TRANSLATOR

  //  --------------------------------------------------------------------------
  //  INSTANTIATION
  //  Set all values of the page table to -1
  int i;
  for(i = 0; i < PAGE_TABLE_SIZE; i++)
    pageTable[i] = -1;

  //  Create a mapping between backstore file data and the virtual memory
  backingStoreMap = mmap(0, MEM_SIZE, PROT_READ, MAP_PRIVATE, backingStore, 0);

  //  --------------------------------------------------------------------------
  int offset; //  offset value from integer value
  int frame;  //  value of page in the page table
  int physicalAdd;  //  physical address
  int logicalAdd; //  logical address
  signed char value;  //  value at main memory
  unsigned char free = 0; //  iterate through page frames in main mem

  //  sort through the address file
  while(fgets(buffer, SIZE, addressFile) != NULL)
  {
    //  Grab page number and offset value
    logicalAdd = atoi(buffer); // ex: 16916 = 0000 0000 0000 0000 0100 0010 0001 0100
    offset = logicalAdd & 255;    // & 255 =  0000 0000 0000 0000 0000 0000 0001 0100

                                  // ex: 16916 = 0000 0000 0000 0000 0100 0010 0001 0100
    logicalAdd = logicalAdd >> 8;    // >> 8 =   0000 0000 0000 0000 0000 0000 0100 0010
    frame = pageTable[logicalAdd]; //  Grab value from page table at page

    //  --------------------------------------------------------------------------
    //  If there's a miss
    if(frame == -1)
    {
      pageFaults++; //  keep track of how many pageFaults there are
      //  copy memory from the backingstore to the main memory
      memcpy(mainMem + (free * PAGE_SIZE), backingStoreMap + (logicalAdd * PAGE_SIZE), PAGE_SIZE);

      //  set physical address to the frame
      physicalAdd = free;
      //  set logical address of pagetable to the frame
      pageTable[logicalAdd] = free;
      //  iterate frame for next miss
      free++;
    }

    //  If no miss, set physical address to frame grabbed from page table
    else
      physicalAdd = frame;

    //  --------------------------------------------------------------------------
    //  grab the value just entered into the main memory
    value = mainMem[(physicalAdd * PAGE_SIZE) + offset];

    printf("Logical Address: %d, Physical Address: %d, Value: %d\n", logicalAdd, physicalAdd, value);
  }

  //  ==========================================================================
  //  STATISTICS

  //  Print rate of page faults below
  float pageFaultRate = ((float)pageFaults / 1000) * 100;  // 1000 = num of values in address file
  printf("\nPage Faults: %d\n", pageFaults);
  printf("Page-fault Rate: %.2f%%\n", pageFaultRate);

  return 0;
}
