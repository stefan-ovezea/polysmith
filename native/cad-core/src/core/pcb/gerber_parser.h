// Gerber (RS-274X) parser — pure text in, command stream out.
//
// Part of the PCB / Gerber workspace (wiki/PCB-Workspace.md).  This
// layer deliberately knows nothing about geometry, the document model,
// the IPC protocol, or logging: it turns file text into operations in
// millimetres plus diagnostic lists, so it can be unit-tested without a
// document, an OCCT context, or an event loop.  The polygonizer
// (gerber_polygonize) consumes the operations; the document layer owns
// logging (polysmith::core::log_*) and user-facing messages.
//
// Scope is the X1 subset the project actually needs, validated against a
// KiCad 10.0.4 plot of a real 4-layer board: %FS/%MO, standard and macro
// apertures, modal interpolation (G01/G02/G03), regions (G36/G37),
// polarity (LPD/LPC), multi-quadrant arcs (G75), units, and the X2
// role attributes KiCad writes either as %TF…% or as `G04 #@! TF…*`
// comments.  Unsupported constructs are recorded and skipped — never a
// hard failure.

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace polysmith::core::pcb {

enum class PcbUnit { Millimeter, Inch };

/// `%FS` zero suppression: which end of a coordinate omits zeros.
enum class ZeroSuppression { Leading, Trailing, Explicit };

enum class CoordinateMode { Absolute, Incremental };

/// `%FSLAX46Y46*%` + `%MOMM*%` (G90/G91 tracked separately).
struct GerberFormat {
  int x_integer_digits = 0;
  int x_decimal_digits = 0;
  int y_integer_digits = 0;
  int y_decimal_digits = 0;
  ZeroSuppression zero_suppression = ZeroSuppression::Leading;
  CoordinateMode mode = CoordinateMode::Absolute;
  PcbUnit unit = PcbUnit::Millimeter;
  /// True once a %FS statement was seen — a file without one cannot have
  /// its coordinates interpreted and is reported as unusable.
  bool defined = false;
};

/// A parse problem.  Severity-free on purpose: the caller decides whether
/// a construct is fatal, a warning, or noise, and logs it in the app's
/// structured log.
struct PcbDiagnostic {
  int line = 0;  // 1-based source line
  std::string message;
};

/// One aperture-macro primitive with its parameter expressions
/// substituted (e.g. "$1+$1" with $1 = 0.25 yields 0.5).  `params` keeps
/// the written order after `code,exposure`; the polygonizer interprets
/// them per code (1 circle, 4 outline, 5 polygon, 20 vector line,
/// 21 rect, 22 obround).
struct MacroPrimitive {
  int code = 0;
  bool exposure_dark = true;
  std::vector<double> params;
};

/// A macro invocation resolved with concrete arguments.
struct ResolvedMacro {
  std::vector<MacroPrimitive> primitives;
  /// False when the macro used an unknown primitive code or an
  /// expression that could not be evaluated — the aperture then degrades
  /// (skipped with a diagnostic) instead of failing the whole file.
  bool supported = true;
};

/// `%AM<name>*` definition.  Statements are evaluated in order at
/// invocation time because `$n = …` assignments may precede primitives.
struct GerberMacro {
  struct Statement {
    bool is_assignment = false;
    std::string variable;  // "$4" when is_assignment
    std::string expression;  // assignment RHS, raw
    int code = 0;            // primitive code
    bool exposure_dark = true;
    std::vector<std::string> param_exprs;
    int line = 0;
  };

  std::string name;
  std::vector<Statement> statements;
};

enum class ApertureKind { Circle, Rectangle, Obround, Polygon, Macro };

struct GerberAperture {
  int code = 0;  // Dnn
  ApertureKind kind = ApertureKind::Circle;
  std::vector<double> params;  // C:[diameter]; R/O:[x,y]; P:[diameter,vertices,rotation]
  std::string macro_name;
  std::vector<double> macro_args;
  /// `TA.AperFunction` in effect when the aperture was defined
  /// (e.g. "SMDPad,CuDef", "Profile") — useful for pad/trace/via
  /// classification downstream.
  std::string function;
  int line = 0;
};

/// One operation of the file's command stream, in millimetres.
struct GerberOperation {
  enum class Kind {
    Move,        // D02 — travel, beam off
    Line,        // D01 under linear interpolation
    Arc,         // D01 under G02/G03 (I/J are centre offsets from the start point)
    Flash,       // D03 — stamp the current aperture
    RegionBegin,  // G36
    RegionEnd,   // G37
  };

  Kind kind = Kind::Move;
  double x = 0.0;
  double y = 0.0;
  double i = 0.0;
  double j = 0.0;
  bool clockwise = false;
  bool dark = true;         // %LPD / %LPC in effect
  int aperture_code = 0;    // Dnn in effect
  int line = 0;
};

struct GerberBounds {
  double min_x = 0.0;
  double min_y = 0.0;
  double max_x = 0.0;
  double max_y = 0.0;
  bool valid = false;
};

/// Neutral name for the shared planar bounds type, so drill files and
/// Gerber layers can be reported (and registered against each other)
/// without borrowing the Gerber vocabulary.
using PcbBounds = GerberBounds;

struct GerberFile {
  GerberFormat format;
  std::map<int, GerberAperture> apertures;
  std::map<std::string, GerberMacro> macros;
  std::vector<GerberOperation> operations;
  std::vector<PcbDiagnostic> diagnostics;
  /// Unique construct names that were skipped (e.g. "%SR", "G74").
  std::vector<std::string> unsupported;
  /// X2 attributes — written as %TF…% by some tools, as `G04 #@! TF…*`
  /// comments by KiCad.  These carry the layer role and polarity.
  std::string file_function;  // e.g. "Copper,L1,Top", "Profile"
  std::string file_polarity;  // "Positive" / "Negative"
  /// Arc centre interpretation (G75 multi-quadrant, the default assumed
  /// when a file uses arcs without saying).  G74 single-quadrant is
  /// reported as unsupported.
  bool multi_quadrant_arcs = true;
  GerberBounds bounds;
  /// False when the text held no usable format definition at all.
  bool ok = true;
};

/// Parses one Gerber file.  Never throws and never logs; problems land in
/// `diagnostics` / `unsupported` while the rest of the file still loads.
GerberFile parse_gerber(const std::string& text);

/// Evaluates a Gerber macro arithmetic expression: numbers, `$n`
/// variables (1-based: `$1` is `variables[0]`), `+ - x /`, parentheses,
/// unary minus.  `x` is multiplication — that is Gerber's spelling.
/// Returns nullopt for malformed input or an out-of-range variable.
std::optional<double> evaluate_macro_expression(
    const std::string& expression, const std::vector<double>& variables);

/// Resolves a macro definition for one invocation.
ResolvedMacro resolve_macro(const GerberMacro& macro,
                            const std::vector<double>& args,
                            std::vector<PcbDiagnostic>* diagnostics,
                            int line);

}  // namespace polysmith::core::pcb
