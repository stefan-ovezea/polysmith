constexpr double kBoxSpacing = 10.0;
constexpr double kReferencePlaneSize = 120.0;
constexpr double kSketchPlaneOffset = 0.2;
constexpr double kLineDimensionOffset = 12.0;
constexpr double kCircleDimensionOffset = 8.0;
// Minimum clearance between an arc and its arc-length extension arc, so
// the extension never falls inside the geometry it measures.
constexpr double kArcLengthMinGap = 6.0;
constexpr double kConstraintBadgeOffset = 3.5;
constexpr double kPi = 3.14159265358979323846264338327950288;

struct WorldPoint {
  double x;
  double y;
  double z;
};

struct WorldVector {
  double x;
  double y;
  double z;
};

struct BodyBounds {
  double center_x = 0.0;
  double center_y = 0.0;
  double center_z = 0.0;
  double width = 0.0;
  double height = 0.0;
  double depth = 0.0;
};
