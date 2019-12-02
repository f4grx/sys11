/* working hc11 simulator for sys11 and others */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>

#include "core.h"
#include "gdbremote.h"

static struct option long_options[] =
  {
//    {"add",     required_argument, 0,  0 },
//    {"append",  no_argument,       0,  0 },
    {"bin"   ,  required_argument, 0,  'b' },
    {"s19"   ,  required_argument, 0,  's' },
//    {"verbose", no_argument,       0,  0 },
//    {"create",  required_argument, 0, 'c'},
//    {"file",    required_argument, 0,  0 },
    {0,         0,                 0,  0 }
  };

static uint8_t* loadbin(const char *fname, uint16_t *size)
  {
    FILE *f;
    uint32_t lsize;
    uint8_t *blob;

    f = fopen(fname, "rb");

    if(!f)
      {
        printf("Unable to open: %s\n",fname);
        return NULL;
      }

    fseek(f, 0, SEEK_END);
    lsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(lsize > 65536)
      {
        printf("file %s is too big\n", fname);
err:
        fclose(f);
        return NULL;
      }
    blob = malloc(lsize);
    if(!blob)
      {
        printf("cannot alloc mem for %s\n", fname);
        goto err;
      }
    if(fread(blob, 1, lsize, f) != lsize)
      {
        printf("cannot read: %s\n", fname);
        free(blob);
        goto err;
      }

    fclose(f);
    *size = (uint16_t)lsize;
    return blob;
  }

void help(void)
  {
    printf("sim [-s,--s19 <file>] [-b,--bin <adr,file>]\n"
           "\n"
           "  -s --s19 <file>      Load S-record file\n"
           "  -b --bin <adr,file>  Load binary file at address\n"
         );
  }

int main(int argc, char **argv)
  {
    struct hc11_core core;
    struct gdbremote_t remote;
    int c;

    printf("sys11 simulator v0.1 by f4grx (c) 2019\n");

    hc11_core_init(&core);

    while (1)
      {
        int option_index = 0;
        c = getopt_long(argc, argv, "b:s:", long_options, &option_index);
        if (c == -1)
          {
            break;
          }
        switch (c)
          {
            case 'b':
              {
                char *ptr;
                uint16_t adr;
                uint16_t size;
                uint8_t *bin;
                ptr = strchr(optarg, ',');
                if(!ptr)
                  {
                    printf("--mapbin adr,binfile\n");
                    return -1;
                  }
                *ptr = 0;
                ptr++;
                printf("map something: file %s @ %s\n", ptr, optarg);
                bin = loadbin(ptr,&size);
                if(!bin)
                  {
                    printf("map failed\n");
                    return -1;
                  }
                adr = (uint16_t)strtoul(optarg, NULL, 0);
                hc11_core_map_rom(&core, adr, size, bin);
                break;
              }

            case 's':
              {
                printf("TODO load S19 file\n");
                return -1;
              }

            case '?':
              {
                help();
                return -1;
                break;
              }

            default:
              printf("?? getopt returned character code 0%o ??\n", c);
          }
      }


    if (optind < argc)
      {
        printf("non-option ARGV-elements: ");
        while (optind < argc)
          {
            printf("%s ", argv[optind++]);
          }
        printf("\n");
      }

    hc11_sci_init(&core);

    remote.port = 4444;
    gdbremote_init(&remote);

    hc11_core_reset(&core);
    while(1)
      {
        hc11_core_clock(&core);
      }

    gdbremote_close(&remote);
  }

