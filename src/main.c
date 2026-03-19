#include <plibsys.h>
#include <TeleClassic26/version.h>
#include <TeleClassic26/thread_pool.h>

int main(void)
{
    p_libsys_init();

    p_libsys_shutdown();

    return 0;
}