#ifndef TOPO
#define TOPO

#include <string>
#include <vector>
namespace leo {

class Topo
{
public:
  Topo(int uid_1, int uid_2);
  int m_uid_1;
  int m_uid_2;
};

} // namespace leo

#endif // TOPO_H
