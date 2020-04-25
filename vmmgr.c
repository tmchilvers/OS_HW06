/*
*
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
//  STRUCTS
typedef struct tlb {
  int ladd; //  logical address
  int fn; //  frame number
} tlb;

//  ============================================================================
//  GLOBAL VARIABLES
char buffer[SIZE];  // buffer to read each line from the address file
int backingStore; //  Backing Store File int
signed char* backingStoreMap;  //  backing store mmap
FILE* addressFile;  //  Address File pointer
int pageTable[PAGE_TABLE_SIZE]; //  create a pagetable
struct tlb* t;  //  tlb struct

signed char mainMem[MEM_SIZE]; //  create physical memory
int pageFaults = 0; //  keep track of how many page faults there are
int tlbHits = 0;  //  keep track of how many tlb hits there are

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

  //  Allocate memory for tlb (size of tlb entries)
  t = malloc(sizeof(tlb)*TLB_ENTRS);

  //  Set all values of the tlb to -1;
  for(i = 0; i < PAGE_TABLE_SIZE; i++)
  {
    t[i].ladd = -1;
    t[i].fn = -1;
  }

  //  Create a mapping between backstore file data and the virtual memory
  backingStoreMap = mmap(0, MEM_SIZE, PROT_READ, MAP_PRIVATE, backingStore, 0);

  //  --------------------------------------------------------------------------
  int offset; //  offset value from integer value
  int frame;  //  value of page in the page table
  int physicalAdd;  //  physical address
  int logicalAdd; //  logical address
  signed char value;  //  value at main memory
  unsigned char frameTemp = 0; //  iterate through page frames in main mem
  int tlbSize = 0;  //  keep track of tlb size
  int index = 0;  // index for fifo

  //  sort through the address file
  while(fgets(buffer, SIZE, addressFile) != NULL)
  {
    int cont = 0; // 1 if there is a tlb hit
    //  Grab page number and offset value
    logicalAdd = atoi(buffer); // ex: 16916 = 0000 0000 0000 0000 0100 0010 0001 0100
    offset = logicalAdd & 255;    // & 255 =  0000 0000 0000 0000 0000 0000 0001 0100

                                  // ex: 16916 = 0000 0000 0000 0000 0100 0010 0001 0100
    logicalAdd = logicalAdd >> 8;    // >> 8 =   0000 0000 0000 0000 0000 0000 0100 0010
    frame = pageTable[logicalAdd]; //  Grab value from page table at page

    //  --------------------------------------------------------------------------
    //  See if pagenumber is in TLB
    for(i = 0; i < TLB_ENTRS; i++)
    {
      //  Check if TLB is contains the logical address
      if (logicalAdd == t[i].ladd)
      {
        frame = t[i].fn;  //  set frame to tlb frame
        tlbHits++;  //  iterate tlb hits
        cont = 1; //  don't need to update tlb, so pass
        break;
      }
    }

    //  --------------------------------------------------------------------------
    //  If there's a miss in TLB and also Page Table
    if(frame == -1)
    {
      pageFaults++; //  keep track of how many pageFaults there are
      //  copy memory from the backingstore to the main memory
      memcpy(mainMem + (frameTemp * PAGE_SIZE), backingStoreMap + (logicalAdd * PAGE_SIZE), PAGE_SIZE);

      //  set physical address to the frame
      physicalAdd = frameTemp;
      //  set logical address of pagetable to the frame
      pageTable[logicalAdd] = frameTemp;
      //  iterate frame for next miss
      frameTemp++;
    }

    //  If no miss, set physical address to frame grabbed from page table
    else
    {
      physicalAdd = frame;
    }

    //  --------------------------------------------------------------------------
    //  need to update tlb
    if(cont == 0)
    {
      //  if tlb has not reached its full size
      if (tlbSize < TLB_ENTRS)
      {
        //  set values of tlb to current logical address and frame
        t[index].ladd = logicalAdd;
        t[index].fn = frameTemp - 1; //  frameTemp - 1 is the frame number before iterating it
        index++;
        tlbSize++;
      }

      //  when tlb has reached its full size
      else
      {
        //  FIFO, pop the first inputted value and move each value forward
        for(i = 0; i < TLB_ENTRS - 1; i++)
        {
          t[i].ladd = t[i+1].ladd;
          t[i].fn = t[i+1].fn;
        }

        //  Add the most recent logical address and frame to end of the queue
        t[TLB_ENTRS - 1].ladd = logicalAdd;
        t[TLB_ENTRS - 1].fn = frameTemp-1;
      }
    }

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
  printf("Page Faults Rate: %.2f%%\n", pageFaultRate);

  //  Print rate of tlb hits below
  float tlbHitRate = ((float)tlbHits / 1000) * 100;  // 1000 = num of values in address file
  printf("\nTLB Hits: %d\n", tlbHits);
  printf("TLB Hit Rate: %.2f%%\n", tlbHitRate);

  free(t);  //  frameTemp the tlb memory
  munmap(backingStoreMap, MEM_SIZE);  //  unmap memory

  return 0;
}
