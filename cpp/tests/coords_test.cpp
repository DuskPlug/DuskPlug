#include "../src/coords.h"
#include "test_assert.h"

#include <cmath>
#include <string>

namespace {

bool Near(double actual, double expected) {
    return std::fabs(actual - expected) < 1e-9;
}

}  // namespace

TEST(ParseGoogleMapsCopy) {
    double lat = 0.0;
    double lon = 0.0;
    EXPECT_TRUE(ParseLatLonPair("51.48096831196373, -3.209212141442959", lat, lon));
    EXPECT_TRUE(Near(lat, 51.48096831196373));
    EXPECT_TRUE(Near(lon, -3.209212141442959));
}

TEST(ParseGoogleMapsCopyNoSpace) {
    double lat = 0.0;
    double lon = 0.0;
    EXPECT_TRUE(ParseLatLonPair("51.48096831196373,-3.209212141442959", lat, lon));
    EXPECT_TRUE(Near(lat, 51.48096831196373));
    EXPECT_TRUE(Near(lon, -3.209212141442959));
}

TEST(ParseGoogleMapsCopyWhitespace) {
    double lat = 0.0;
    double lon = 0.0;
    EXPECT_TRUE(ParseLatLonPair("  51.48096831196373, -3.209212141442959  ", lat, lon));
    EXPECT_TRUE(Near(lat, 51.48096831196373));
    EXPECT_TRUE(Near(lon, -3.209212141442959));
}

TEST(ParseGoogleMapsUrlAt) {
    double lat = 0.0;
    double lon = 0.0;
    EXPECT_TRUE(ParseLatLonPair(
        "https://www.google.com/maps/@51.48096831196373,-3.209212141442959,17z",
        lat,
        lon));
    EXPECT_TRUE(Near(lat, 51.48096831196373));
    EXPECT_TRUE(Near(lon, -3.209212141442959));
}

TEST(ParseGoogleMapsUrlQuery) {
    double lat = 0.0;
    double lon = 0.0;
    EXPECT_TRUE(ParseLatLonPair(
        "https://maps.google.com/?q=51.48096831196373,-3.209212141442959",
        lat,
        lon));
    EXPECT_TRUE(Near(lat, 51.48096831196373));
    EXPECT_TRUE(Near(lon, -3.209212141442959));
}

TEST(ParseLatLonRejectsInvalid) {
    double lat = 0.0;
    double lon = 0.0;
    EXPECT_FALSE(ParseLatLonPair("", lat, lon));
    EXPECT_FALSE(ParseLatLonPair("51.4809", lat, lon));
    EXPECT_FALSE(ParseLatLonPair("not a coordinate", lat, lon));
    EXPECT_FALSE(ParseLatLonPair("91.0, 0.0", lat, lon));
    EXPECT_FALSE(ParseLatLonPair("0.0, 181.0", lat, lon));
}

void RunCoordsTests() {
    std::printf("coords tests\n");
    RUN_TEST(ParseGoogleMapsCopy);
    RUN_TEST(ParseGoogleMapsCopyNoSpace);
    RUN_TEST(ParseGoogleMapsCopyWhitespace);
    RUN_TEST(ParseGoogleMapsUrlAt);
    RUN_TEST(ParseGoogleMapsUrlQuery);
    RUN_TEST(ParseLatLonRejectsInvalid);
}
