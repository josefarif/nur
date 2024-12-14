#pragma once

#ifdef DEBUG
	#include <stdio.h>
	#define print(msg) printf(msg"\n")
	#define print_error(msg) printf("\x1b[41m""\x1b[37m""\x1b[1m""\x1b[4m""  ERROR  ""\x1b[0m""\x1b[43m""\x1b[30m  " msg "  \x1b[0m\n")
	#define assert(condition, msg) if (!(condition)) {print_error("ASSERTION FAILED: " #condition ": " msg);}
#else
	#define print(...)
	#define print_error(...)
	#define assert(condition, ...) condition
#endif