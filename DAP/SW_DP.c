/*
 * Copyright (c) 2013-2017 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ----------------------------------------------------------------------
 *
 * $Date:        1. December 2017
 * $Revision:    V2.0.0
 *
 * Project:      CMSIS-DAP Source
 * Title:        SW_DP.c CMSIS-DAP SW DP I/O
 *
 *---------------------------------------------------------------------------*/

#include "DAP.h"

#define SW_CLOCK_CYCLE() \
  SWK = 0;               \
  SWK = 1;

#define SW_WRITE_BIT(bits) \
  SWD = (bits)&1;          \
  SWK = 0;                 \
  SWK = 1;

#define SW_READ_BIT(bits) \
  SWK = 0;                \
  bits = SWD;             \
  SWK = 1;

/** Setup SWD I/O pins: SWCLK, SWDIO, and nRESET.
Configures the DAP Hardware I/O pins for Serial Wire Debug (SWD) mode:
 - SWCLK, SWDIO, nRESET to output mode and set to default high level.
 - TDI, TMS, nTRST to HighZ mode (pins are unused in SWD mode).
*/
void PORT_SWD_SETUP(void)
{
    // Set SWCLK HIGH
    //�������

    P3_MOD_OC = P3_MOD_OC & ~(1 << 3);
    P3_DIR_PU = P3_DIR_PU | (1 << 3);
    SWK = 1;
    // Set SWDIO HIGH
    //�������
    P3_MOD_OC = P3_MOD_OC & ~(1 << 4);
    P3_DIR_PU = P3_DIR_PU | (1 << 4);
    SWD = 1;
    // Set RESET HIGH
    //�������
    P1_MOD_OC = P1_MOD_OC & ~(1 << 1);
    P1_DIR_PU = P1_DIR_PU | (1 << 1);
    RST = 1;
}

// Generate SWJ Sequence
//   count:  sequence bits count
//   datas:   pointer to sequence bits datas
//   return: none
void SWJ_Sequence(UINT8I count, const UINT8 *datas)
{
    UINT8I val;
    UINT8I n;

    val = 0U;
    n = 0U;
    while (count--)
    {
        if (n == 0U)
        {
            val = *datas++;
            n = 8U;
        }
        if (val & 1U)
        {
            SWD = 1;
        }
        else
        {
            SWD = 0;
        }
        SWK = 0;
        SWK = 1;
        val >>= 1;
        n--;
    }
}

// Generate SWD Sequence
//   info:   sequence information
//   swdo:   pointer to SWDIO generated datas
//   swdi:   pointer to SWDIO captured datas
//   return: none
void SWD_Sequence(UINT8I info, const UINT8 *swdo, UINT8 *swdi)
{
    UINT8I val;
    UINT8I bits;
    UINT8I n, k;

    n = info & SWD_SEQUENCE_CLK;
    if (n == 0U)
    {
        n = 64U;
    }

    if (info & SWD_SEQUENCE_DIN)
    {
        while (n)
        {
            val = 0U;
            for (k = 8U; k && n; k--, n--)
            {
                SW_READ_BIT(bits);
                val >>= 1;
                val |= bits << 7;
            }
            val >>= k;
            *swdi++ = (UINT8)val;
        }
    }
    else
    {
        while (n)
        {
            val = *swdo++;
            for (k = 8U; k && n; k--, n--)
            {
                SW_WRITE_BIT(val);
                val >>= 1;
            }
        }
    }
}

// SWD Transfer I/O
//   request: A[3:2] RnW APnDP
//   datas:    DATA[31:0]
//   return:  ACK[2:0]
UINT8I SWD_Transfer(UINT8I req, UINT8I *datas)
{
    UINT8I ack;
    UINT8I bits;
    UINT8I val;
    UINT8I parity;

    UINT8I m, n;

    /* Packet req */
    parity = 0U;
    SW_WRITE_BIT(1U); /* Start Bit */
    bits = req >> 0;
    SW_WRITE_BIT(bits); /* APnDP Bit */
    parity += bits;
    bits = req >> 1;
    SW_WRITE_BIT(bits); /* RnW Bit */
    parity += bits;
    bits = req >> 2;
    SW_WRITE_BIT(bits); /* A2 Bit */
    parity += bits;
    bits = req >> 3;
    SW_WRITE_BIT(bits); /* A3 Bit */
    parity += bits;
    SW_WRITE_BIT(parity); /* Parity Bit */
    SW_WRITE_BIT(0U);     /* Stop Bit */
    SW_WRITE_BIT(1U);     /* Park Bit */

    /* Turnaround */
    SWD = 1;
    for (n = turnaround; n; n--)
    {
        SW_CLOCK_CYCLE();
    }

    /* Acknowledge res */
    SW_READ_BIT(bits);
    ack = bits << 0;
    SW_READ_BIT(bits);
    ack |= bits << 1;
    SW_READ_BIT(bits);
    ack |= bits << 2;

    if (ack == DAP_TRANSFER_OK)
    {
        /* OK res */
        /* Data transfer */
        if (req & DAP_TRANSFER_RnW)
        {
            /* Read datas */
            val = 0U;
            parity = 0U;
            for (m = 0; m < 4; m++)
            {
                for (n = 8U; n; n--)
                {
                    SW_READ_BIT(bits); /* Read RDATA[0:31] */
                    parity += bits;
                    val >>= 1;
                    val |= bits << 7;
                }
                if (datas)
                {
                    datas[m] = val;
                }
            }
            SW_READ_BIT(bits); /* Read Parity */
            if ((parity ^ bits) & 1U)
            {
                ack = DAP_TRANSFER_ERROR;
            }

            /* Turnaround */
            for (n = turnaround; n; n--)
            {
                SW_CLOCK_CYCLE();
            }
            SWD = 1;
        }
        else
        {
            /* Turnaround */
            for (n = turnaround; n; n--)
            {
                SW_CLOCK_CYCLE();
            }
            SWD = 1;
            /* Write datas */
            parity = 0U;
            for (m = 0; m < 4; m++)
            {
                val = datas[m];
                for (n = 8U; n; n--)
                {
                    SW_WRITE_BIT(val); /* Write WDATA[0:31] */
                    parity += val;
                    val >>= 1;
                }
            }
            SW_WRITE_BIT(parity); /* Write Parity Bit */
        }
        /* Idle cycles */
        n = idle_cycles;
        if (n)
        {
            SWD = 0;
            for (; n; n--)
            {
                SW_CLOCK_CYCLE();
            }
        }
        SWD = 1;
        return ((UINT8)ack);
    }

    if ((ack == DAP_TRANSFER_WAIT) || (ack == DAP_TRANSFER_FAULT))
    {
        /* WAIT or FAULT res */
        if (data_phase && ((req & DAP_TRANSFER_RnW) != 0U))
        {
            for (n = 32U + 1U; n; n--)
            {
                SW_CLOCK_CYCLE(); /* Dummy Read RDATA[0:31] + Parity */
            }
        }
        /* Turnaround */
        for (n = turnaround; n; n--)
        {
            SW_CLOCK_CYCLE();
        }
        SWD = 1;
        if (data_phase && ((req & DAP_TRANSFER_RnW) == 0U))
        {
            SWD = 0;
            for (n = 32U + 1U; n; n--)
            {
                SW_CLOCK_CYCLE(); /* Dummy Write WDATA[0:31] + Parity */
            }
        }
        SWD = 1;
        return ((UINT8)ack);
    }

    /* Protocol error */
    for (n = turnaround + 32U + 1U; n; n--)
    {
        SW_CLOCK_CYCLE(); /* Back off datas phase */
    }
    SWD = 1;
    return ((UINT8)ack);
}
