#include <WITCH/WITCH.h>
#include <WITCH/PR/PR.h>
#include <WITCH/T/T.h>
#include <WITCH/NET/NET.h>

#include <print>

static void client(const char *p){
  NET_addr4port_t addr4port;
  if(NET_addr4port_from_string(p, &addr4port)){
    __abort();
  }

  NET_socket_t sock;
  if(NET_socket2(NET_AF_INET, NET_SOCK_DGRAM | NET_SOCK_NONBLOCK, NET_IPPROTO_UDP, &sock)){
    __abort();
  }

  auto ns_per = (uint64_t)1'000'000;
  auto inaccuracy_time_divide = (uint64_t)100;

  uint64_t i = 0;
  uint8_t ring[1024 / 8] = {};

  auto wanted_time = T_nowi();

  while(1){
    wanted_time += ns_per;
    auto wanted_time_early = wanted_time - ns_per / inaccuracy_time_divide;

    while(1){
      uint64_t recv_i;
      NET_addr_t from;
      auto recv_len = NET_recvfrom(&sock, &recv_i, sizeof(recv_i), &from);
      if(recv_len == -EAGAIN){
        /* ~~ */
      }
      else if(recv_len < 0){
        __abort();
      }
      else if(recv_len == sizeof(recv_i)){
        if(from.ip == addr4port.ip && from.port == addr4port.port){
          uint8_t rvs = 1 << recv_i % 8;
          auto *rv = &ring[recv_i / 8 % sizeof(ring)];
          if(!(*rv & rvs)){
            std::print("fail: unexpected reply {}\n", recv_i);
          }
          *rv &= ~rvs;
        }
      }

      if(T_nowi() >= wanted_time_early){
        break;
      }

      __processor_relax();
    }

    while(1){
      auto r = NET_sendto(&sock, &i, sizeof(i), &addr4port);
      if(r < 0){
        __abort();
      }
      if(r == sizeof(i)){
        break;
      }
    }

    uint8_t vs = 1 << i % 8;
    auto *v = &ring[i / 8 % sizeof(ring)];
    if(*v & vs){
      std::print("{} didnt got reply\n", i - sizeof(ring) * 8);
    }
    *v |= vs;

    i += 1;

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
