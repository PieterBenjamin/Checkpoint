#ifndef _CP_H_
#define _CP_H_

#include <stdint.h>  // for uint32_t, etc.

// We abstract the idea of some quantities into user defined types
//  so  that we could, in theory, easily change between uint32_t and
// uint64_t realizations, say.

typedef uint32_t CPSize_t;   // for sizes of containers and indices into containers

#endif  // _CP_H_
