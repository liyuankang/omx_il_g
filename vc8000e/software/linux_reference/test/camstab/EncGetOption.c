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
--                                                                            --
--  Abstract : Command line parameter parsing
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "EncGetOption.h"
#include "base_type.h"
#include <stdio.h>
#include <string.h>

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static i32 LongOption(i32 argc, char **argv, option_s *option, argument_s
		*argument, char **optArg);
static i32 ShortOption(i32 argc, char **argv, option_s *option, argument_s
		*argument, char **optArg);
static i32 Argument(i32 argc, char **argv, option_s *option, argument_s
		*argument, char **optArg, u32 length);
static i32 GetNext(i32 argc, char **argv, argument_s *argument, char **optArg);

/*------------------------------------------------------------------------------

	EncGetOption

	Parse command line options. This function should be called with argc
	and argv values that are parameters of main(). The function will parse
	the next parameter from the command line. The function returns the
	next option character and stores the current plase to structure
	argument_s. Structure option_s contain valid options and matched option
	and argument are argument_s structure.
	For example:

	option_s option[] = {
		{"help",           'H', 0},	// No armument
		{"input",          'i', 1},	// Argument is compulsory
		{"output",         'o', 2},	// Argument is optional
		{NULL,              0,  0}	// Format of last line

	Comman line format can be
	--input filename
	--input=filename
	--inputfilename
	-i filename
	-i=filename
	-ifilename

	Input	argc	Argument count as passed to main().
		argv	Argument values as passed to main().
		option	Valid options and argument requirements.
		argument Option and argument return structure.

	Return	1	Unknow option.
		0	Option and argument are OK.
		-1	No more options.
		-2	Option match but argument is missing.

------------------------------------------------------------------------------*/
i32 EncGetOption(i32 argc, char **argv, option_s *option, argument_s *argument)
{
	char *optArg = NULL;
	i32 ret;

	argument->optArg = "?";
	argument->shortOpt = '?';
	argument->enableArg = 0;

	if (GetNext(argc, argv, argument, &optArg) != 0) {
		return -1;	/* End of options */
	}

	/* Long option */
	if ((ret = LongOption(argc, argv, option, argument, &optArg)) != 1) {
		return ret;
	}

	/* Short option */
	if ((ret = ShortOption(argc, argv, option, argument, &optArg)) != 1) {
		return ret;
	}

	/* This is an unknown option but option anyway so optArg must return. */
	argument->optArg = optArg;

	return 1;
}

/*------------------------------------------------------------------------------

	LongOption

------------------------------------------------------------------------------*/
i32 LongOption(i32 argc, char **argv, option_s *option, argument_s *argument,
		char **optArg)
{
	i32 i = 0;
	u32 length;

	if (strncmp("--", *optArg, 2) != 0) {
		return 1;  /* not a long option */
	}

	while (option[i].longOpt != NULL) {
		length = strlen(option[i].longOpt);
		if (strncmp(option[i].longOpt, *optArg+2, length) == 0) {
			goto match;
		}
		i++;
	}
	return 1;  /* Matched option is not found. */

	match:
	length += 2;		/* Because an option start with -- */
	if (Argument(argc, argv, &option[i], argument, optArg, length) != 0) {
		return -2;
	}

	return 0;
}

/*------------------------------------------------------------------------------

	ShortOption

------------------------------------------------------------------------------*/
i32 ShortOption(i32 argc, char **argv, option_s *option, argument_s *argument,
		char **optArg)
{
	i32 i = 0;
	char shortOpt;

	if (strncmp("-", *optArg, 1) != 0) {
		return 1;
	}

	strncpy(&shortOpt, *optArg+1, 1);
	while (option[i].longOpt != NULL) {
		if (option[i].shortOpt  == shortOpt) {
			goto match;
		}
		i++;
	}
	return 1;

	match:
	if (Argument(argc, argv, &option[i], argument, optArg, 2) != 0) {
		return -2;
	}

	return 0;
}

/*------------------------------------------------------------------------------

	Argument

------------------------------------------------------------------------------*/
i32 Argument(i32 argc, char **argv, option_s *option, argument_s *argument,
		char **optArg, u32 length)
{
	char *arg;

	argument->shortOpt = option->shortOpt;
	arg = *optArg+length;

	/* Argument and option are together */
	if (strlen(arg) != 0) {
		/* There should be no argument, but we found one. */
		if (option->enableArg == 0) {
			return -1;
		}

		/* Remove = */
		if (strncmp("=", arg, 1) == 0) {
			arg++;
		}
		argument->enableArg = 1;
		argument->optArg = arg;
		return 0;
	}

	/* Argument and option are separate. */
	if (GetNext(argc, argv, argument, optArg) != 0) {
		/* There should be an argument (option->enableArg == 1), but we found no more parameters. */
		if (option->enableArg == 1) {
			return -1;
		}
		return 0;
	}

	/* Parameter is missing if next start with "-" but next time this
	 * option is OK so we must fix argument->optCnt */
	if (strncmp("-", *optArg, 1) == 0) {
		argument->optCnt--;
		if (option->enableArg == 1) {
			return -1;  /* There should be an argument (option->enableArg == 1), but not found. */
		}
		return 0;
	}

	/* There should be no argument (option->enableArg == 0), but we found one. */
	if (option->enableArg == 0) {
		return -1;
	}

	argument->enableArg = 1;
	argument->optArg = *optArg;

	return 0;
}

/*------------------------------------------------------------------------------

	GetNext

------------------------------------------------------------------------------*/
i32 GetNext(i32 argc, char **argv, argument_s *argument, char **optArg)
{
	/* End of options */
	if ((argument->optCnt >= argc) || (argument->optCnt < 0)) {
		return -1;
	}
	*optArg = argv[argument->optCnt];
	argument->optCnt++;

	return 0;
}
