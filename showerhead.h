
#define BUFFER_FRAMES 32
typedef struct { 
                  int elts;
                  int idx;
                  int idxN1;
                  int idxN2;
                  unsigned long ary[BUFFER_FRAMES];
} CircularBuffer;


