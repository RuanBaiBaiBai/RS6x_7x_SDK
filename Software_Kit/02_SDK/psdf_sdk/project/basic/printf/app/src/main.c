/**
 ******************************************************************************
 * @file    main.c
 * @brief   main define.
 * @verbatim    null
 ******************************************************************************
 * @attention
 *
 * Copyright (C) 2025 POSSUMIC TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of POSSUMIC TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */


/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "common.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */
/* Private macros.
 * ----------------------------------------------------------------------------
 */
/* Private variables.
 * ----------------------------------------------------------------------------
 */
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
/* Exported functions.
 * ----------------------------------------------------------------------------
 */
#define PAGES 931
int main(void)
{
    printf("printf test\n");

    /*
    * Integer formats:
    * Decimal: -9234  Unsigned: 4294958062
    * Decimal -9234 as:
    * Hex: FFFFDBEEh  C hex: 0xffffdbee  Octal: 37777755756
    * Characters in field:
    * Real numbers:
    * 251.736600 251.74 2.517366e+002 2.517366E+002
    */
    char ch = 'h';
    int count = -9234;
    double fp = 251.7366;

    printf( "Integer formats:\n"
            "   Decimal: %d  Unsigned: %u\n", count, count);
    printf( "Decimal %d as:\n   Hex: %Xh  "
            "C hex: 0x%x  Octal: %o\n", count, count, count, count );
    printf("Characters in field:\n"
          "%10c\n", ch);
    printf("Real numbers:\n   %f %.2f %e %E\n", fp, fp, fp, fp );
    const double RENT = 3852.99;  // const-style constant

    /*
    *931       *
    *+3852.99*
    1f 1F 0x1f
    **42** 42**-42**
    **    6**  006**00006**  006**
    */
    printf("*%-10d*\n", PAGES);
    printf("*%+4.2f*\n", RENT);
    printf("%x %X %#x\n", 31, 31, 31);
    printf("**%d**% d**% d**\n", 42, 42, -42);
    printf("**%5d**%5.3d**%5d**%5.3d**\n", 6, 6, 6, 6);

    /*
    *931*
    *       931*
    *931*
    */
    printf("*%2d*\n", PAGES);
    printf("*%10d*\n", PAGES);
    printf("*%*d*\n", 2, PAGES);

    /*
    *3852.99*
    *3853.0*
    *  3852.990*
    */
    printf("*%4.2f*\n", RENT);
    printf("*%3.1f*\n", RENT);
    printf("*%10.3f*\n", RENT);

    short num = 336;
    long n3 = 2000000000;
    long n4 = 1234567890;
    //num as short and unsigned short:  336 336 2000000000 1234567890
    printf("num as short and unsigned short:  %hd %hu\n", num, num);
    printf("%ld %ld\n", n3, n4);

    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
