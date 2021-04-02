// Dummy CPU intensive app

extern "C" {
#include <base/hash.h>
#include <base/log.h>
#include <runtime/runtime.h>
#include <runtime/smalloc.h>
#include <runtime/storage.h>
}

#include "thread.h"
#include "timer.h"

#include <iostream>

uint64_t g_runtime_sec;

void MainHandler(void */*arg*/)
{
    barrier();
    auto start_ts = microtime();
    barrier();
  
    uint64_t stop_ts = start_ts + (g_runtime_sec+5)*1000000LL;

    rt::Sleep(5 * 1000 * 1000);

    uint64_t spin_cnt = 0;
    while(true)
    {
        spin_cnt++;
        if(spin_cnt % 1000000LL == 0)
        {
            barrier();
            auto ts = microtime();
            barrier();

            if(ts >= stop_ts)
            {
                break;
            }
        }
    }

    printf("spins/sec: %lf\n", ((double) spin_cnt)/((double) g_runtime_sec));
}


int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr,
            "usage: ./cpu_intensive [cfg] [runtime-in-secs]\n");
    return -EINVAL;
  }

  g_runtime_sec = std::strtoll(argv[2], nullptr, 0);
  

  int ret = runtime_init(argv[1], MainHandler, NULL);
  if (ret) printf("failed to start runtime\n");

  return ret;
}