#include <WITCH/WITCH.h>
#include <WITCH/PR/PR.h>
#include <WITCH/T/T.h>
#include <WITCH/NET/NET.h>
#include <WITCH/TH/TH.h>

#include <thread>
#include <print>

static void client(const char *p){
  NET_addr4port_t addr4port;
  if(NET_addr4port_from_string(p, &addr4port)){
    __abort();
  }

  alignas(64) uint8_t ring[1024] = {};

  uint64_t producer_index = 0;

  NET_socket_t sock;
  if(NET_socket2(NET_AF_INET, NET_SOCK_DGRAM, NET_IPPROTO_UDP, &sock)){
    __abort();
  }

  std::thread t([&](){
    while(1){
      uint64_t ci;
      NET_addr_t from;
      auto recv_len = NET_recvfrom(&sock, &ci, sizeof(ci), &from);
      if(recv_len < 0){
        __abort();
      }
      else if(recv_len == sizeof(ci)){
        if(from.ip == addr4port.ip && from.port == addr4port.port){
          auto pi = __atomic_load_n(&producer_index, __ATOMIC_SEQ_CST);
          if(ci + (sizeof(ring) / sizeof(ring[0])) <= pi){
            std::print(
              "fail: consumer is too late for producer.\n"
              "producer: {} consumer: {}\n",
              pi, ci
            );
            continue;
          }
  
          auto *rv = &ring[ci % (sizeof(ring) / sizeof(ring[0]))];
          if(!__atomic_load_n(rv, __ATOMIC_SEQ_CST)){
            std::print(
              "fail: unexpected reply.\n"
              "producer: {} consumer: {}\n",
              pi, ci
            );
            continue;
          }
          __atomic_store_n(rv, 0, __ATOMIC_SEQ_CST);
        }
      }
    }
  });

  auto ns_per = (uint64_t)1'000'000;
  auto inaccuracy_time_divide = (uint64_t)2;

  auto wanted_time = T_nowi();

  while(1){
    wanted_time += ns_per;
    auto wanted_time_early = wanted_time - ns_per / inaccuracy_time_divide;

    while(1){
      auto diff = (sint64_t)(wanted_time_early - T_nowi());

      if(diff <= 0){
        break;
      }
      if(diff > 500'000){
        TH_sleepi(diff);
      }

      __processor_relax();
    }

    auto *v = &ring[producer_index % (sizeof(ring) / sizeof(ring[0]))];
    if(__atomic_load_n(v, __ATOMIC_SEQ_CST)){
      std::print("{} didnt got reply\n", producer_index - (sizeof(ring) / sizeof(ring[0])));
    }
    __atomic_store_n(v, 1, __ATOMIC_SEQ_CST);

    while(1){
      auto r = NET_sendto(&sock, &producer_index, sizeof(producer_index), &addr4port);
      if(r < 0){
        __abort();
      }
      if(r == sizeof(producer_index)){
        break;
      }
    }

    __atomic_add_fetch(&producer_index, 1, __ATOMIC_SEQ_CST);

    auto now = T_nowi();
    auto diff = (sint64_t)now - (sint64_t)wanted_time;
    if(diff >= (sint64_t)ns_per){
      std::print("fail: {}ns late\n", diff);
    }
  }
}

static void server(const char *p){
  uint16_t port = atoi(p);

  NET_socket_t sock;
  if(NET_socket2(NET_AF_INET, NET_SOCK_DGRAM, NET_IPPROTO_UDP, &sock)){
    __abort();
  }

  if(NET_setsockopt(&sock, NET_SOL_SOCKET, NET_SO_REUSEADDR, 1)){
    __abort();
  }

  NET_addr_t bind_addr = {.ip = NET_INADDR_ANY, .port = port};
  if(NET_bind(&sock, &bind_addr)){
    __abort();
  }

  uint8_t buf[2048];
  while(1){
    NET_addr_t from;
    IO_ssize_t len = NET_recvfrom(&sock, buf, sizeof(buf), &from);
    if(len < 0){
      __abort();
    }
    while(1){
      auto r = NET_sendto(&sock, buf, len, &from);
      if(r < 0){
        __abort();
      }
      if(r == len){
        break;
      }
    }
  }
}

int main(int argc, const char** argv){
  if(argc == 1){
    std::print("need s or c argument\n");
    return 1;
  }

  if(argv[1][0] == 's'){
    if(argc != 3){
      std::print("need only port as argument\n");
      return 1;
    }

    server(argv[2]);
  }
  else if(argv[1][0] == 'c'){
    if(argc != 3){
      std::print("need only ip:port as argument\n");
      return 1;
    }

    client(argv[2]);
  }

  return 0;
}
