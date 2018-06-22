#include "stdafx.h"
#include "windows.h"

#include "ow/ow.h"
#include "ow/devices/ow_device_ds18x20.h"

ow_t ow;

/**
 * \brief           Application entry point
 */
int
main(void) {
    uint8_t ri[8], rom_addresses[20][8];
    size_t count, i;

    ow_init(&ow, NULL);                         /* Initialize 1-Wire module */

    /*
     * Scan for 1-wire devices
     */
    ow_protect(&ow);                            /* Protect from multiple-thread */
    if (ow_search_reset(&ow) == owOK) {         /* Start search for devices */
        printf("Search for 1-Wire device started!\r\n");
        count = 0;
        while (ow_search(&ow, ri) == owOK) {    /* Scan device by device */
            memcpy(rom_addresses[count], ri, sizeof(ri));
            printf("Device found: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                ri[0], ri[1], ri[2], ri[3], ri[4], ri[5], ri[6], ri[7]
            );
            if (ow_ds18x20_is_b(&ow, rom_addresses[count])) {
                ow_ds18x20_set_resolution(&ow, rom_addresses[count], 9 + count % 4);
            }
            count++;
        }
        printf("Search finished with %u devices on 1-Wire bus\r\n", (unsigned)count);
    } else {
        printf("Search reset was unsuccessful!\r\n");
    }
    ow_unprotect(&ow);                          /* We can now unprotect again */

    /* Check if we have any device on 1-Wire */
    /* Manually start DS18x20 temperature conversion on all devices */
    if (count > 0) {
        ow_ds18x20_start(&ow, NULL);            /* Wait for some time */
        Sleep(1000);
        for (i = 0; i < count; i++) {
            float temp;
            if (ow_ds18x20_is_b(&ow, rom_addresses[i])) {
                uint8_t resolution = ow_ds18x20_get_resolution(&ow, rom_addresses[i]);
                if (ow_ds18x20_read(&ow, rom_addresses[i], &temp)) {
                    printf("Sensor %u returned temperature %f degrees (%u bits)\r\n", (unsigned)i, temp, (unsigned)resolution);
                }
            }
        }
    }

    return 0;
}
