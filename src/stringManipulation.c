/*
 * intToString.c
 *
 *  Created on: 19/4/2019
 *      Author: nelsonf
 */
#include"stringManipulation.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

char* integerToString(int value, char* result, int base) {
   // check that the base if valid
   if (base < 2 || base > 36) { *result = '\0'; return result; }

   char* ptr = result, *ptr1 = result, tmp_char;
   int tmp_value;

   do {
      tmp_value = value;
      value /= base;
      *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
   } while ( value );

   // Apply negative sign
   if (tmp_value < 0) *ptr++ = '-';
   *ptr-- = '\0';
   while(ptr1 < ptr) {
      tmp_char = *ptr;
      *ptr--= *ptr1;
      *ptr1++ = tmp_char;
   }
   return result;
}

char* floatToString(float value, char* result) {
    /* La función divide el numero flotante en dos numeros siendo el primero
    la parte entera del mismo y el segundo la parte decimal.
    A la parte decimal se la multiplica 4 veces para poder obtener 4 digitos
    después de la coma y así luego utilizar la función integerToString para
    convertirlo a string.
    Al pasar ambos numeros se agrega además el punto decimal.
    */
    double parteEntera, parteDecimal;
    double pEnt;

    double pDecimal;
    int i;
    int fractionAbs;
    char *strEntero, *strDecimal;
    char strRes[15];

    parteDecimal = modf(value,&parteEntera);

    for(i=0;i<4;i++){
        parteDecimal = parteDecimal * 10;
    }
    result = integerToString((int) parteEntera,result,10);
    strcat(result,".");
    fractionAbs = fabs((int) parteDecimal);
    strcat(result,integerToString(fractionAbs,strRes,10));

    return result;
}



