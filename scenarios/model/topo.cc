#include "topo.h"
#include "ns3/core-module.h"
#include <stdexcept>

namespace leo
{

Topo::Topo (int uid_1, int uid_2)
  : m_uid_1(uid_1),
    m_uid_2(uid_2)
{}

}