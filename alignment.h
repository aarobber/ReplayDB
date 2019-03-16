#ifndef ALIGNMENT_H
#define ALIGNMENT_H

#define ALIGN_SIZE 8
#define ROUND_TO_ALIGN(sz) ((sz) + ((ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1))
#define IS_ALIGNED(addr) (((addr) & (ALIGN_SIZE-1)) == 0)

#endif
