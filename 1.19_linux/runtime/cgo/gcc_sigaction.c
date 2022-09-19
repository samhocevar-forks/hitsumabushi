#include <stddef.h>
#include <stdint.h>

#include "libcgo.h"

typedef struct {
} go_sigaction_t;

int32_t
x_cgo_sigaction(intptr_t signum, const go_sigaction_t *goact, go_sigaction_t *oldgoact) {
	return 0;
}
