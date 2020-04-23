#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#include "core.h"

bool enables[65536];

void log_init(void)
  {
    int i;
    for(i=0;i<65536;i++)
      {
        enables[i] = false;
      }
  }

void log_enable(uint8_t system, uint8_t subsystem)
  {
    //enables[system<<8+subsystem] = true;
    int i;
    for(i=0;i<65536;i++)
      {
        enables[i] = true;
      }
  }

void log_msg(uint8_t system, uint8_t subsystem, const char *fmt, ...)
  {
    va_list ap;

    if(!enables[system<<8+subsystem])
      {
        return;
      }

    va_start(ap,fmt); 
    vprintf(fmt,ap);
    va_end(ap);
  }

