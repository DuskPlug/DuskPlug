#pragma once

#include <string>

// Parses latitude,longitude as copied from Google Maps (for example
// "51.48096831196373, -3.209212141442959") or a Google Maps URL that
// contains @lat,lon or q=lat,lon.
bool ParseLatLonPair(const std::string& text, double& latitude, double& longitude);
