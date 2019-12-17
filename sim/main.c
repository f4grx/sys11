/* working hc11 simulator for sys11 and others */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>

#include "core.h"
#include "gdbremote.h"

static struct option long_options[] =
  {
//    {"append",  no_argument,       0,  0 },
    {"bin"   , required_argument, 0, 'b' },
    {"s19"   , required_argument, 0, 's' },
    {0       , 0                , 0,  0  }
  };

sem_t end;

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

void sig(int sig)
  {
    printf("Signal caught\n");
    sem_post(&end);
  }

int main(int argc, char **argv)
  {
    struct hc11_core core;
    struct gdbremote_t remote;
    int c;
    struct sigaction sa_mine;
    int val;
    int prev;

    printf("sys11 simulator v0.1 by f4grx (c) 2019\n");

    sem_init(&end,0,0);
    memset(&sa_mine, 0, sizeof(struct sigaction));
    sa_mine.sa_handler = sig;
    sigaction(SIGINT, &sa_mine, NULL);

    hc11_core_init(&core);
    //map 32k of RAM in the first half of the address space
    hc11_core_map_ram(&core, "ram", 0x0000, 0x8000); //100h bytes masked by internal mem
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
                hc11_core_map_rom(&core, "rom", adr, size, bin);
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

    remote.port = 3333;
    remote.core = &core;
    gdbremote_init(&remote);

    hc11_core_reset(&core);
    prev = -1;
    while(1)
      {
        sem_getvalue(&end, &val);
        if(val)
          {
            printf("simulation interrupted\n");
            break;
          }

        if(prev != core.status)
          {
            printf("status: %d -> %d\n", prev, core.status);
            if(core.status == STATUS_STOPPED && prev != -1)
              {
                gdbremote_stopped(&remote);
              }
            prev = core.status;
          }

        if(core.status == STATUS_STEPPING)
          {
            printf("doing a step\n");
            hc11_core_step(&core);
            core.status = STATUS_STOPPED;
          }
        else if(core.status == STATUS_RUNNING)
          {
//            printf("(r)");
            hc11_core_step(&core);
          }
        else if(core.status == STATUS_STOPPED)
          {
//            printf("(s)");
          }
        usleep(1000);
      }
    sem_getvalue(&end, &val);
    if(!val)
      {
        printf("Waiting for ctrl-c...\n");
        sem_wait(&end);
      }
    gdbremote_close(&remote);
  }

