#include "ground-station.h"
#include "ns3/core-module.h"
#include <stdexcept>

namespace leo
{

GroundStation::GroundStation (
  int id, int uid,
  std::string name,
  double latitude, double longitude,
  double elevation, double xCartesian, double yCartesian,
  double zCartesian)
  : m_id(id),
    m_uid(uid),
    m_name(name),
    m_latitude(latitude),
    m_longitude(longitude),
    m_elevation(elevation),
    m_xCartesian(xCartesian),
    m_yCartesian(yCartesian),
    m_zCartesian(zCartesian)
{}

GroundStation::GroundStation (std::vector<std::string> gs) : GroundStation(
  std::stoi(gs[0]),
  std::stoi(gs[0]),
  gs[1],
  std::stod(gs[2]),
  std::stod(gs[3]),
  std::stod(gs[4]),
  std::stod(gs[5]),
  std::stod(gs[6]),
  std::stod(gs[7])
)
{}

GroundStation::GroundStation (std::vector<std::string> gs, int offset) : GroundStation(
  std::stoi(gs[0]),
  std::stoi(gs[0]) + offset,
  gs[1],
  std::stod(gs[2]),
  std::stod(gs[3]),
  std::stod(gs[4]),
  std::stod(gs[5]),
  std::stod(gs[6]),
  std::stod(gs[7])
)
{}

}