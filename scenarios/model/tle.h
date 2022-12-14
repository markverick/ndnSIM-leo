#ifndef TLE
#define TLE

#include <string>
#include <vector>
namespace leo {

class Tle
{
// TODO: Parse TLE for future advanced scenerios
public:
  Tle(int orbit, int uid, std::string title, std::string line1, std::string line2);
  int m_orbit;
  int m_uid;
  std::string m_title;
  std::string m_line1;
  std::string m_line2;
};

} // namespace leo

#endif // TLE_H
