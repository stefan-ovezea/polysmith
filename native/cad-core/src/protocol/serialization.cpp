#include "protocol/serialization.h"

#include <cmath>
#include <stdexcept>

#include "core/sketch/dof_counter.h"

namespace polysmith::protocol {

namespace {

#include "protocol/impl/json_helpers.inc"
#include "protocol/impl/feature_parameter_parsers.inc"
}  // namespace

#include "protocol/impl/basic_payloads_and_cam.inc"
#include "protocol/impl/cam_from_payload.inc"
#include "protocol/impl/feature_to_payload.inc"
#include "protocol/impl/document_session_to_payload.inc"
#include "protocol/impl/viewport_to_payload.inc"
#include "protocol/impl/feature_from_payload.inc"
#include "protocol/impl/document_from_payload.inc"
}  // namespace polysmith::protocol
