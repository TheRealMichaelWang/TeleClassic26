#include <stdio.h>
#include <stdlib.h>
#include <TeleClassic26/version.h>

int main(void)
{
    printf("Hello, TeleClassic26! v%d.%d.%d\n",
           TELECLASSIC26_VERSION_MAJOR,
           TELECLASSIC26_VERSION_MINOR,
           TELECLASSIC26_VERSION_PATCH);
    return EXIT_SUCCESS;
}
