#include "tle.h"
#include "ns3/core-module.h"
#include <stdexcept>

namespace leo
{

Tle::Tle (int orbit, int uid, std::string title, std::string line1, std::string line2)
  : m_orbit(orbit),
    m_uid(uid),
    m_title(title),
    m_line1(line1),
    m_line2(line2)
{}

}