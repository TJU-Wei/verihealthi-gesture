/* SPDX-License-Identifier: MIT */
/*
 * CRC.c
 *
 *  Created on: 2026年6月15日
 *      Author: coolc
 */
#include <stdint.h>
unsigned int crc32_update(unsigned int crc ,const unsigned char *data ,unsigned int len)
{

    for (unsigned int i = 0; i < len; i++)

    {

        crc ^= data[i];

        for (int j = 0; j < 8; j++)

        {

            if (crc & 1)

                crc = (crc >> 1) ^ 0xEDB88320;

            else

                crc >>= 1;

        }

    }

    return crc;
}
