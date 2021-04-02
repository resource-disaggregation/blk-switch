// Closed-loop storge client

extern "C" {
#include <base/hash.h>
#include <base/log.h>
#include <runtime/runtime.h>
#include <runtime/smalloc.h>
#include <runtime/storage.h>
}

#include "thread.h"
#include "timer.h"

#include <memory>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <random>

#include "RpcManager.h"

#define MAX_IO_SIZE 262144
#define SECTOR_SIZE 512
#define TOTAL_BLOCK_COUNT 547002288LL
#define LBA_ALIGNMENT (~0x7)
#define MAX_IO_DEPTH 128
#define MAX_CONNS 128

netaddr g_flash_server_addr;
// RpcEndpoint<ReflexHdr> *g_flash_server;
size_t g_io_size;
uint64_t g_runtime_sec;
uint32_t g_io_depth;
uint32_t g_num_conns;
uint32_t g_read_ratio;

std::vector<double> ProcessRequests(RpcEndpoint<ReflexHdr> *endpoint, size_t io_size, uint64_t stop_ts, uint32_t read_ratio)
{
    int ret;
    unsigned int io_lba_count = io_size / SECTOR_SIZE;
    std::vector<double> timings; // TODO: reserve space
    std::unique_ptr<unsigned char[]> buf(new unsigned char[MAX_IO_SIZE]());

    std::mt19937 rg(rand());
    std::uniform_int_distribution<size_t> wd(0, TOTAL_BLOCK_COUNT-1);
    std::uniform_int_distribution<uint32_t> read_d(0, 99);

    while(true)
    {
        barrier();
        auto ts = microtime();
        barrier();

        if(ts >= stop_ts)
        {
            break;
        }

        // Send request
        uint64_t start_ts = ts;

        bool is_write = (read_d(rg) < (100-read_ratio));

        Rpc<ReflexHdr> r;

        if(!is_write)
        {
            r.req.magic = 24;
            r.req.opcode = 0;
            r.req.lba = (wd(rg) & LBA_ALIGNMENT);
            r.req.lba_count = io_lba_count;
            r.req_body = nullptr;
            r.req_body_len = 0;

            r.rsp_body = buf.get();
            r.rsp_body_len = io_size;
        } else {
            r.req.magic = 24;
            r.req.opcode = 1;
            r.req.lba = (wd(rg) & LBA_ALIGNMENT);
            r.req.lba_count = io_lba_count;
            r.req_body = buf.get();
            r.req_body_len = io_size;

            r.rsp_body = nullptr;
            r.rsp_body_len = 0;
        }
        
        ret = endpoint->SubmitRequestBlocking(&r);

        if(ret)
        {
            log_err("Request failed");
            break;
        }

        barrier();
        auto end_ts = microtime();
        barrier();

        timings.push_back(end_ts - start_ts);

    }

    return timings;
}


void MainHandler(void */*arg*/)
{
    int ret;

    barrier();
    auto ts = microtime();
    barrier();
  
    uint64_t stop_ts = ts + (g_runtime_sec+5)*1000000LL;

    // g_flash_server = RpcEndpoint<ReflexHdr>::Create(g_flash_server_addr);
    std::unique_ptr<RpcEndpoint<ReflexHdr>> endpoints[MAX_CONNS];
    for(int i = 0; i < g_num_conns; i++)
    {
        endpoints[i].reset(RpcEndpoint<ReflexHdr>::Create(g_flash_server_addr));
        if (endpoints[i].get() == nullptr) {
            log_err("couldn't dial rpc connection");
            return;
        }
    }

    log_err("Connections to server created");

    // warm-up sleep
    rt::Sleep(5 * 1000 * 1000);

    // if (g_flash_server == nullptr) {
    //   log_err("couldn't dial rpc connection");
    //   return;
    // }


    std::vector<rt::Thread> th;
    std::unique_ptr<std::vector<double>> samples[MAX_IO_DEPTH];
    for (int i = 0; i < g_io_depth; ++i) {
        th.emplace_back(rt::Thread([&, i]{
            auto v = ProcessRequests(endpoints[ i % g_num_conns].get(), g_io_size, stop_ts, g_read_ratio);
            samples[i].reset(new std::vector<double>(std::move(v)));
        }));
    }

    // Wait for the workers to finish.
    for (auto& t: th)
        t.Join();

    // Accumulate timings from all workers
    std::vector<double> timings;
    for (int i = 0; i < g_io_depth; ++i) {
        auto &v = *samples[i];
        timings.insert(timings.end(), v.begin(), v.end());
    }

    std::sort(timings.begin(), timings.end());
    double sum = std::accumulate(timings.begin(), timings.end(), 0.0);
    double mean = sum / timings.size();
    double count = static_cast<double>(timings.size());
    double p9 = timings[count * 0.9];
    double p99 = timings[count * 0.99];
    double p999 = timings[count * 0.999];
    double p9999 = timings[count * 0.9999];
    double min = timings[0];
    double max = timings[timings.size() - 1];
    double iops = count / (double)g_runtime_sec;
    std::cout << std::setprecision(2) << std::fixed
                << " iops: "   << iops
                << " n: "      << timings.size()
                << " min: "    << min
                << " mean: "   << mean
                << " 90%: "    << p9
                << " 99%: "    << p99
                << " 99.9%: "  << p999
                << " 99.99%: " << p9999
                << " max: "    << max << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc < 8) {
    fprintf(stderr,
            "usage: ./storage_client [cfg] [storageserveraddr] [io_size] [io-depth] [runtime-in-secs] [num-conns] [read-ratio]\n");
    return -EINVAL;
  }

  if (StringToAddr(argv[2], &g_flash_server_addr)) {
    printf("failed to parse addr %s\n", argv[3]);
    return -EINVAL;
  }

  g_io_size = std::strtoll(argv[3], nullptr, 0);
  g_io_depth = (uint32_t) std::strtoll(argv[4], nullptr, 0);
  g_runtime_sec = std::strtoll(argv[5], nullptr, 0);
  g_num_conns = std::strtoll(argv[6], nullptr, 0);
  g_read_ratio = std::strtoll(argv[7], nullptr, 0);
  

  int ret = runtime_init(argv[1], MainHandler, NULL);
  if (ret) printf("failed to start runtime\n");

  return ret;
}
