#include <cerrno>
#include <cstdlib>
#include <malloc.h>

extern "C" int posix_memalign(void ** memptr, size_t alignment, size_t size) {
    if (memptr == nullptr || alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0) {
        return EINVAL;
    }

    void * ptr = memalign(alignment, size);
    if (ptr == nullptr) {
        return ENOMEM;
    }

    *memptr = ptr;
    return 0;
}
