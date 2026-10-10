/* Links against opentrack-usbmux and checks that the library initialised.
 *
 * Does not need a device or a running usbmuxd: with no daemon,
 * usbmuxd_get_device_list() fails with a negative errno, which is fine. */

#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <usbmuxd.h>

int main(void)
{
    printf("libusbmuxd %s\n", libusbmuxd_version());

#ifdef _WIN32
    /* This program never calls WSAStartup(). The socket only opens if
     * libimobiledevice-glue's static initialiser ran and called it, which
     * fails if the linker dropped glue.c. */
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed with %d: Winsock was not initialised\n", WSAGetLastError());
        return 1;
    }
    closesocket(s);
#endif

    usbmuxd_device_info_t* devices = NULL;
    int n = usbmuxd_get_device_list(&devices);
    if (n < 0) {
        printf("usbmuxd_get_device_list: %d (no usbmuxd running?)\n", n);
    } else {
        printf("usbmuxd_get_device_list: %d device(s)\n", n);
        for (int i = 0; i < n; i++)
            printf("  %s\n", devices[i].udid);
        usbmuxd_device_list_free(&devices);
    }
    return 0;
}
