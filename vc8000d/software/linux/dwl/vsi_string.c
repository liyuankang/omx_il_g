/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#define NULL ((void *)0)

/* Append SRC on the end of DEST.  */
char *vsi_strcat (char *dest, const char *src) {
  char *s1 = dest;
  const char *s2 = src;
  char c;

  /* Find the end of the string.  */
  do
    c = *s1++;
  while (c != '\0');

  /* Make S1 point before the next character, so we can increment
    it while memory is read (wins on pipelined cpus).  */
  s1 -= 2;

  do {
    c = *s2++;
    *++s1 = c;
  } while (c != '\0');

  return dest;
}

/* Compare S1 and S2, returning less than, equal to or
   greater than zero if S1 is lexicographically less than,
   equal to or greater than S2.  */
int vsi_strcmp(const char *p1, const char *p2) {
  const unsigned char *s1 = (const unsigned char *) p1;
  const unsigned char *s2 = (const unsigned char *) p2;
  unsigned char c1, c2;

  do {
    c1 = (unsigned char) *s1++;
    c2 = (unsigned char) *s2++;
    if (c1 == '\0')
      return c1 - c2;
  } while (c1 == c2);

  return c1 - c2;
}

/* Compare no more than N characters of S1 and S2,
   returning less than, equal to or greater than zero
   if S1 is lexicographically less than, equal to or
   greater than S2.  */
int vsi_strncmp(const char *s1, const char *s2, unsigned int n) {
  unsigned char c1 = '\0';
  unsigned char c2 = '\0';

  if (n >= 4) {
    unsigned int n4 = n >> 2;
    do {
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0' || c1 != c2)
        return c1 - c2;
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0' || c1 != c2)
        return c1 - c2;
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0' || c1 != c2)
        return c1 - c2;
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0' || c1 != c2)
        return c1 - c2;
    } while (--n4 > 0);
    n &= 3;
  }

  while (n > 0) {
    c1 = (unsigned char) *s1++;
    c2 = (unsigned char) *s2++;
    if (c1 == '\0' || c1 != c2)
      return c1 - c2;
    n--;
  }

  return c1 - c2;
}


/* Copy SRC to DEST.  */
char *vsi_strcpy(char *dest, const char *src) {
  char c;
  char *s = (char *) src;
  const int off = dest - s - 1;

  do {
    c = *s++;
    s[off] = c;
  } while (c != '\0');

  return dest;
}

char *vsi_strncpy(char *s1, const char *s2, unsigned int n) {
  char c;
  char *s = s1;

  --s1;

  if (n >= 4) {
    unsigned int n4 = n >> 2;

    for (;;) {
      c = *s2++;
      *++s1 = c;
      if (c == '\0')
        break;
      c = *s2++;
      *++s1 = c;
      if (c == '\0')
        break;
      c = *s2++;
      *++s1 = c;
      if (c == '\0')
        break;
      c = *s2++;
      *++s1 = c;
      if (c == '\0')
        break;
      if (--n4 == 0)
        goto last_chars;
    }
    n = n - (s1 - s) - 1;
    if (n == 0)
      return s;
    goto zero_fill;
  }

last_chars:
  n &= 3;
  if (n == 0)
    return s;

  do {
    c = *s2++;
    *++s1 = c;
    if (--n == 0)
      return s;
  } while (c != '\0');

zero_fill:
  do
    *++s1 = '\0';
  while (--n > 0);

  return s;
}


/* Return the length of the null-terminated string STR.  Scan for
   the null terminator quickly by testing four bytes at a time.  */
unsigned int vsi_strlen(const char *str) {
  const char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int longword, himagic, lomagic;

  /* Handle the first few characters by reading one character at a time.
     Do this until CHAR_PTR is aligned on a longword boundary.  */
  for (char_ptr = str; ((unsigned long int) char_ptr
                        & (sizeof (longword) - 1)) != 0;
       ++char_ptr)
    if (*char_ptr == '\0')
      return char_ptr - str;

  /* All these elucidatory comments refer to 4-byte longwords,
     but the theory applies equally well to 8-byte longwords.  */

  longword_ptr = (unsigned long int *) char_ptr;

  /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
     the "holes."  Note that there is a hole just to the left of
     each byte, with an extra at the end:

     bits:  01111110 11111110 11111110 11111111
     bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

     The 1-bits make sure that carries propagate to the next 0-bit.
     The 0-bits provide holes for carries to fall into.  */
  himagic = 0x80808080L;
  lomagic = 0x01010101L;
  if (sizeof (longword) > 4) {
    /* 64-bit version of the magic.  */
    /* Do the shift in two steps to avoid a warning if long has 32 bits.  */
    himagic = ((himagic << 16) << 16) | himagic;
    lomagic = ((lomagic << 16) << 16) | lomagic;
  }

  /* Instead of the traditional loop which tests each character,
     we will test a longword at a time.  The tricky part is testing
     if *any of the four* bytes in the longword in question are zero.  */
  for (;;) {
    longword = *longword_ptr++;

    if (((longword - lomagic) & ~longword & himagic) != 0) {
      /* Which of the bytes was the zero?  If none of them were, it was
         a misfire; continue the search.  */

      const char *cp = (const char *) (longword_ptr - 1);

      if (cp[0] == 0)
        return cp - str;
      if (cp[1] == 0)
        return cp - str + 1;
      if (cp[2] == 0)
        return cp - str + 2;
      if (cp[3] == 0)
        return cp - str + 3;
      if (sizeof (longword) > 4) {
        if (cp[4] == 0)
          return cp - str + 4;
        if (cp[5] == 0)
          return cp - str + 5;
        if (cp[6] == 0)
          return cp - str + 6;
        if (cp[7] == 0)
          return cp - str + 7;
      }
    }
  }
}


/* Return the length of the maximum initial segment
   of S which contains only characters in ACCEPT.  */
unsigned int vsi_strspn(const char *s, const char *accept) {
  const char *p;
  const char *a;
  unsigned int count = 0;

  for (p = s; *p != '\0'; ++p) {
    for (a = accept; *a != '\0'; ++a)
      if (*p == *a)
        break;
    if (*a == '\0')
      return count;
    else
      ++count;
  }

  return count;
}

/* Find the first occurrence in S of any character in ACCEPT.  */
char *vsi_strpbrk (const char *s, const char *accept) {
  while (*s != '\0') {
    const char *a = accept;
    while (*a != '\0')
      if (*a++ == *s)
        return (char *) s;
    ++s;
  }

  return NULL;
}

static char *olds;

/* Parse S into tokens separated by characters in DELIM.
   If S is NULL, the last string strtok() was called with is
   used.  For example:
  char s[] = "-abc-=-def";
  x = strtok(s, "-");   // x = "abc"
  x = strtok(NULL, "-=");   // x = "def"
  x = strtok(NULL, "=");    // x = NULL
    // s = "abc\0=-def\0"
*/
char *vsi_strtok(char *s, const char *delim) {
  char *token;

  if (s == NULL)
    s = olds;

  if(olds == NULL)
    return NULL;

  /* Scan leading delimiters.  */
  s += vsi_strspn (s, delim);
  if (*s == '\0') {
    olds = s;
    return NULL;
  }

  /* Find the end of the token.  */
  token = s;
  s = vsi_strpbrk (token, delim);
  if (s == NULL) {
    /* This token finishes the string.  */
    olds = NULL;
    token = NULL;
  } else {
    /* Terminate the token and make OLDS point past it.  */
    *s = '\0';
    olds = s + 1;
  }
  return token;
}

int vsi_isspace(char c) {
  /*
  ' '   (0x20)  space (SPC)
  '\t'  (0x09)  horizontal tab (TAB)
  '\n'  (0x0a)  newline (LF)
  '\v'  (0x0b)  vertical tab (VT)
  '\f'  (0x0c)  feed (FF)
  '\r'  (0x0d)  carriage return (CR)
  */
  if(c==0x20 || c==0x09 || c==0x0a || c==0x0b || c== 0x0c || c==0x0d)
    return 1;
  return 0;
}

char vsi_toupper(char c) {
  if (c >='a' && c <= 'z')
    c = c - ('a' - 'A');
  return c;
}

#define ULONG_MAX 0xFFFFFFFF
#define LONG_MAX 0x7FFFFFFF
#define LONG_MIN 0x80000000

static unsigned long vsi_Strtoul(const char *input, char **endptr, int base, int isSignedValue) {
  char nbmax; // The maximum numerical digit that can be read
  int tbase = 10; // Default base when base is zero
  int MaxDigits;
  int ScanSuffix = 0; // Do we scan any suffixes?
  int ReadSomething=0; // Did we read something in?
  char sign = 0;   // If negative sign is '-'
  char cbmax = 0;  // Max digit that can be read for base > 10
  int digit;       // Digit  read in
  unsigned long long tvalue;
  unsigned long long maxval; // Max value that doesn't overflow
  unsigned long errval;
  unsigned long long value = 0;

  /* Store nptr into endptr immediately to save its value since
     we are going to reuse input */
  if (endptr) *endptr = (char *)input;

  /* The value of the "base" argument should be zero or
  2 <= base <= 36.
  What happens with negative values of "base" or with base 1?
  The standard is silent about that. We just do not accept any
  other values for "base" besides those explicitely allowed
  */
  if (base < 0 || base == 1 || base > 36) {
    return 0;
  }
  /* Is it better to crash later when dereferencing a NULL pointer?
  Or to give just an error now? */
  if (input == NULL) {
    return 0;
  }

  /* C99: 7.20.1.4.2 First, they decompose the input string into
    three parts:
  * an initial, possibly empty, sequence of white-space characters
  * (as specified by the isspace function), ...
  */
  while(vsi_isspace(*input)) {
    input++;
  }

  if((*input == '-') || (*input == '+')) {
    sign = *input;
    input++;
  }

  /* C99: 7.20.1.4.5: If the subject sequence has the expected
  form and the value of base is zero, the sequence of characters
  starting with the first digit is interpreted as an integer
  constant according to the rules of 6.4.4.1.
  C99: 7.20.1.4.3: If the value of base is 16, the characters
  0x or 0X may optionally precede the sequence of letters and
  digits, following the sign if present.
  */
  if(*input == '0' && (base == 0 || base == 16)) {
    if((input[1] == 'X') || (input[1] == 'x')) {
      tbase = 16;
      input+=2;
    } else if (base == 0) {
      tbase = 8;
      /* We have read a zero! We MUST bump "ReadSomething"
         because if not the trivial sequence "0" wouldn't
         be recognized!! Note that "0x" is NOT a valid
         sequence so we haven't read anything UNTIL we read
         a number later
      */
      ReadSomething=1;
      input++;
    }
  }

  /* C99: 7.20.1.4.7: If the subject sequence is empty or
  does not have the expected form, no conversion is performed;
  the value of nptr is stored in the object pointed to by
  endptr, provided that endptr is not a null pointer.
  */
  if (*input == 0 && ReadSomething == 0)      return 0;

  /* If we set tbase above put its value into base. We must
  save the fact that if the original base was zero we should
  look for an eventual suffix. Put that into the ScanSuffix
  variable. Obviously ScanSuffix could be eliminated and
  we could set tbase to 4876 say, since its value is no longer
  used. At the end we could test if tbase is 4876. That would
  save 4 bytes but make this more unreadable... Not worth */

  if(base == 0) {
    base = tbase;
    ScanSuffix = 1;
  }

  if(base <= 10) {
    nbmax = (char)((char)'0' + (char)(base - 1));
  } else {
    nbmax = '9';
    cbmax = (char)((char)'a' + (char)(base - 11));
  }
  if(isSignedValue) { /* We are returning a signed number */
    maxval = LONG_MAX;
    errval = LONG_MAX;
    if(sign == '-') {
      maxval = (long long)LONG_MAX+1LL;
      errval = (unsigned long)LONG_MIN;
    } else {
      maxval = LONG_MAX;
      errval = LONG_MAX;
    }
  } else { /* we are returning an unsigned number */
    maxval = ULONG_MAX;
    errval = ULONG_MAX;
  }
  /*
  The number of digits for a number in base "base".
  Assuming 2^base is the maximum number in the given base
  (in bits), we take the logarithm and divide by the
  logarithm of the base to get the number of digits.
  We take the ceiling since in the last position,
  a whole digit must be read
  */
#if 0
  MaxDigits = (int)
              (ceil(log(pow(2.0,sizeof(long)*CHAR_BIT))/log(base)));
#else
  MaxDigits = 10;
#endif
  /* Preliminary work is ended, we start converting the number */
  while(*input && ReadSomething <= MaxDigits) {
    if((*input >= '0') && (*input <= nbmax)) {
      digit = *input - '0';
    } else if((*input >= 'a') && (*input <= cbmax)) {
      digit = (*input - 'a') + 10;
    } else if((*input >= 'A') &&
              (*input <= (char)vsi_toupper(cbmax))) {
      digit = (*input - 'A') + 10;
    } else { /* invalid char. Stop. */
      break;
    }
    /* The only usage of tvalue is to nicely appear in
    our debugger to easily follow the computation.
    It will be eliminated when we KNOW that there are
    no bugs in left this sofwtare... :-;) */
    tvalue = (value* (unsigned long)base);
    value = tvalue + digit;
    input++;
    ReadSomething++;
  }
  if (*input && ReadSomething > MaxDigits) {
    /* Ignore all digits */
    while (((*input >= '0') && (*input <= nbmax)) ||
           ((*input >= 'a') && (*input <= cbmax)) ||
           ((*input >= 'A') &&
            (*input <= (char)vsi_toupper(cbmax)))) {
      input++;
    }
  }
  if (endptr && ReadSomething && ScanSuffix) {
    /* Accept U,L, UL, ULL suffixes.
      All other suffixes are ignored */
    if (*input == 'u' || *input == 'U') input++;
    if (*input == 'l' || *input == 'L') input ++;
    if (*input == 'l' || *input == 'L') input++;
  }
  if(endptr != NULL && ReadSomething > 0) {
    *endptr = (char *)input;
  }
  if (value > maxval || ReadSomething > MaxDigits) {
    value = errval;
  } else if (sign ==  '-') {
    value = -(value&0xffffffffULL);
  }
  return(value);
}

/*-----------------------------------------------------------*/
/* strtoul() - convert a string of base n to an unsigned long*/
/*-----------------------------------------------------------*/
unsigned long vsi_strtoul(const char *input, char **endptr, int base) {
  return(vsi_Strtoul(input, endptr, base, 0));
}

/*-----------------------------------------------------------*/
/* strtol() - convert a string of base n to a signed long    */
/*-----------------------------------------------------------*/
long vsi_strtol(const char *input, char **endptr, int base) {
  return(vsi_Strtoul(input, endptr, base, 1));
}

/* Convert a string to an int.  */
int vsi_atoi (const char *nptr) {
  return (int) vsi_strtol (nptr, (char **) NULL, 10);
}

