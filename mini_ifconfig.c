#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

int main()
{
  struct ifreq ifr;
  struct sockaddr_in sin;
  int skfd;

  skfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (skfd < 0)
      return -1;
      
  strcpy(ifr.ifr_name, "lo");
  memset(&sin, 0, sizeof(struct sockaddr));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_LOOPBACK;
  memcpy(&ifr.ifr_addr, &sin, sizeof(struct sockaddr));
  if (ioctl(skfd, SIOCSIFADDR, &ifr) < 0)
      return -1;
  strcpy(ifr.ifr_name, "lo");
  ifr.ifr_flags = (IFF_UP | IFF_RUNNING);
  if (ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0) {
      return -1;
  }
  return 0;
}
