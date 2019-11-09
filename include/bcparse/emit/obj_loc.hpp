#pragma once

#include <string>
#include <sstream>

namespace bcparse {
  class ObjLoc {
  public:
    enum class DataStoreLocation {
      NullDataStore = 0x0,
      StaticDataStore = 0x1,
      LocalDataStore = 0x2,
      RegisterDataStore = 0x3
    };

    ObjLoc(int location, DataStoreLocation dataStoreLocation)
      : m_location(location),
        m_dataStoreLocation(dataStoreLocation) {
    }

    ObjLoc(const ObjLoc &other)
      : m_location(other.m_location),
        m_dataStoreLocation(other.m_dataStoreLocation) {
    }

    inline int getLocation() const { return m_location; }
    inline DataStoreLocation getDataStoreLocation() const { return m_dataStoreLocation; }

    inline std::string toString() const {
      std::stringstream ss;

      switch (m_dataStoreLocation) {
        case DataStoreLocation::StaticDataStore:
          ss << "STATIC";
          break;
        case DataStoreLocation::LocalDataStore:
          ss << "LOCAL";
          break;
        case DataStoreLocation::RegisterDataStore:
          ss << "REGISTER";
          break;
      }

      ss << "[" << m_location << "]";

      return ss.str();
    }

  private:
    int m_location;
    DataStoreLocation m_dataStoreLocation;
  };
}
