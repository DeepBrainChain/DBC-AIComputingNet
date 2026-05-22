/* sysctl_compat.c
 *
 * glibc 2.32+ removed the obsolete sysctl(2) wrapper. The vendored
 * libevent.a (built against an older glibc) still references it from
 * arc4_seed_sysctl_linux(), which prevents the dbc binary from linking
 * on Ubuntu 22.04 / glibc 2.35.
 *
 * We provide a minimal compatibility stub here. libevent only calls
 * sysctl as part of its RNG seed mixing — when sysctl is unavailable it
 * silently falls back to /dev/urandom (which is what we want anyway).
 * Returning -1 with errno=ENOSYS makes libevent take that fallback path.
 */

#include <errno.h>
#include <sys/types.h>

#ifdef __linux__

/* Match the legacy <linux/sysctl.h> signature without including the
 * removed header. */
int sysctl(int *name, int nlen,
           void *oldval, size_t *oldlenp,
           void *newval, size_t newlen) {
    (void)name; (void)nlen; (void)oldval; (void)oldlenp; (void)newval; (void)newlen;
    errno = ENOSYS;
    return -1;
}

#endif
